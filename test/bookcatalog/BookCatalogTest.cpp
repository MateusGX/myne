#include <HalStorage.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../datastore_common/TestHarness.h"
#include "BookCatalog.h"
#include "BookStore.h"

namespace {

struct CollectionEntry {
  std::string id;
  std::string name;
  int expectedCount = 0;
  int initialVolume = 0;
};

void collectCollection(const char* id, const char* name, int expectedCount, int initialVolume, void* ctx) {
  static_cast<std::vector<CollectionEntry>*>(ctx)->push_back({id, name, expectedCount, initialVolume});
}

PhysicalBook makeBook(BookStore& store, const std::string& title, const std::string& author,
                      const std::string& collection = "", const std::string& location = "") {
  PhysicalBook b;
  b.title = title;
  b.author = author;
  b.collection = collection;
  b.location = location;
  store.create(b);
  usleep(2000);  // BookStore::create() ids are millis()-based; avoid collisions
  return b;
}

BookCatalog::BookChangeInfo toChangeInfo(const PhysicalBook& b) {
  return BookCatalog::BookChangeInfo{b.id.c_str(),       b.title.c_str(),  b.author.c_str(),
                                     b.location.c_str(), b.volume.c_str(), b.collection.c_str()};
}

// Six books used by most tests:
//   Alpha Book / Banana Book / 1984 / Solo Book - standalone, letters A/B/#/S
//   Saga Vol 1 / Saga Vol 2 - "Saga Collection" (letter S)
struct SampleBooks {
  PhysicalBook alpha, banana, n1984, saga1, saga2, solo;
};

SampleBooks createSampleBooks(BookStore& books) {
  SampleBooks s;
  s.alpha = makeBook(books, "Alpha Book", "Author A", "", "Shelf 1");
  s.banana = makeBook(books, "Banana Book", "Author B", "", "Shelf 2");
  s.n1984 = makeBook(books, "1984", "George Orwell");
  s.saga1 = makeBook(books, "Saga Vol 1", "Author C", "Saga Collection");
  s.saga2 = makeBook(books, "Saga Vol 2", "Author C", "Saga Collection");
  s.solo = makeBook(books, "Solo Book", "Author D");
  return s;
}

}  // namespace

void testResolveCollectionIdAndForEachCollection() {
  TempStorageDir tmp;

  char id1[9] = {}, id2[9] = {}, id3[9] = {};
  ASSERT_TRUE(BookCatalog::resolveCollectionId("My Collection", id1));
  ASSERT_TRUE(BookCatalog::resolveCollectionId("My Collection", id2));
  ASSERT_EQ(std::string(id1), std::string(id2));

  ASSERT_TRUE(BookCatalog::resolveCollectionId("Other Collection", id3));
  ASSERT_TRUE(std::string(id1) != std::string(id3));

  std::vector<CollectionEntry> colls;
  BookCatalog::forEachCollection(collectCollection, &colls);
  ASSERT_EQ(colls.size(), (size_t)2);

  bool foundMy = false, foundOther = false;
  for (const auto& c : colls) {
    if (c.name == "My Collection" && c.id == id1) foundMy = true;
    if (c.name == "Other Collection" && c.id == id3) foundOther = true;
  }
  ASSERT_TRUE(foundMy);
  ASSERT_TRUE(foundOther);

  printf("  resolveCollectionId() is idempotent; forEachCollection() lists registered collections\n");
  PASS();
}

void testCollectionExpectedCount() {
  TempStorageDir tmp;

  char id[9] = {};
  ASSERT_TRUE(BookCatalog::resolveCollectionId("My Collection", id));
  ASSERT_EQ(BookCatalog::getCollectionExpectedCount(id), 0);
  ASSERT_TRUE(BookCatalog::setCollectionExpectedCount(id, 12));
  ASSERT_EQ(BookCatalog::getCollectionExpectedCount(id), 12);
  ASSERT_TRUE(BookCatalog::setCollectionExpectedCount(id, -1));
  ASSERT_EQ(BookCatalog::getCollectionExpectedCount(id), 0);
  ASSERT_TRUE(BookCatalog::setCollectionExpectedCount(id, 9));
  ASSERT_EQ(BookCatalog::getCollectionInitialVolume(id), 0);
  ASSERT_TRUE(BookCatalog::setCollectionInitialVolume(id, 2));
  ASSERT_EQ(BookCatalog::getCollectionInitialVolume(id), 2);
  ASSERT_TRUE(BookCatalog::setCollectionInitialVolume(id, -1));
  ASSERT_EQ(BookCatalog::getCollectionInitialVolume(id), 0);
  ASSERT_TRUE(BookCatalog::setCollectionInitialVolume(id, 3));

  std::vector<CollectionEntry> colls;
  BookCatalog::forEachCollection(collectCollection, &colls);
  ASSERT_EQ(colls.size(), (size_t)1);
  ASSERT_EQ(colls[0].id, std::string(id));
  ASSERT_EQ(colls[0].expectedCount, 9);
  ASSERT_EQ(colls[0].initialVolume, 3);

  ASSERT_TRUE(BookCatalog::renameCollection(id, "My Renamed Collection"));
  ASSERT_EQ(BookCatalog::getCollectionExpectedCount(id), 9);
  ASSERT_EQ(BookCatalog::getCollectionInitialVolume(id), 3);

  printf("  collection metadata persists, clears, lists, and survives rename\n");
  PASS();
}

void testRebuildAndReadCatalog() {
  TempStorageDir tmp;
  BookStore books;
  books.init();

  uint16_t idx[27] = {};
  ASSERT_FALSE(BookCatalog::readLetterIndex(idx));

  createSampleBooks(books);

  char sagaId[9] = {};
  ASSERT_TRUE(BookCatalog::resolveCollectionId("Saga Collection", sagaId));
  ASSERT_TRUE(BookCatalog::setCollectionExpectedCount(sagaId, 3));
  ASSERT_TRUE(BookCatalog::setCollectionInitialVolume(sagaId, 5));

  ASSERT_TRUE(BookCatalog::rebuild(BookStore::DIR_PATH));

  ASSERT_TRUE(BookCatalog::readLetterIndex(idx));
  ASSERT_EQ((int)idx[0], 1);   // 'A' -> Alpha Book
  ASSERT_EQ((int)idx[1], 1);   // 'B' -> Banana Book
  ASSERT_EQ((int)idx[18], 2);  // 'S' -> Saga Collection header + Solo Book
  ASSERT_EQ((int)idx[26], 1);  // '#' -> "1984"

  ASSERT_EQ(BookCatalog::letterCount('A'), 1);
  ASSERT_EQ(BookCatalog::letterCount('S'), 2);
  ASSERT_EQ(BookCatalog::letterCount('#'), 1);
  ASSERT_EQ(BookCatalog::letterCount('Z'), 0);

  BookCatalog::Entry entries[10];

  int n = BookCatalog::readLetterPage('A', 0, 10, entries);
  ASSERT_EQ(n, 1);
  ASSERT_FALSE(entries[0].isCollection);
  ASSERT_EQ(std::string(entries[0].title), "Alpha Book");
  ASSERT_EQ(std::string(entries[0].author), "Author A");
  ASSERT_EQ(std::string(entries[0].location), "Shelf 1");

  n = BookCatalog::readLetterPage('#', 0, 10, entries);
  ASSERT_EQ(n, 1);
  ASSERT_EQ(std::string(entries[0].title), "1984");

  // Letter 'S' has the "Saga Collection" header first, then "Solo Book".
  n = BookCatalog::readLetterPage('S', 0, 10, entries);
  ASSERT_EQ(n, 2);
  ASSERT_TRUE(entries[0].isCollection);
  ASSERT_EQ(std::string(entries[0].title), "Saga Collection");
  ASSERT_EQ(entries[0].count, 2);
  ASSERT_EQ(entries[0].expectedCount, 3);
  ASSERT_EQ(entries[0].initialVolume, 5);
  ASSERT_FALSE(entries[1].isCollection);
  ASSERT_EQ(std::string(entries[1].title), "Solo Book");

  ASSERT_TRUE(BookCatalog::resolveCollectionId("Saga Collection", sagaId));
  ASSERT_EQ(std::string(sagaId), std::string(entries[0].id));

  ASSERT_EQ(BookCatalog::collectionCount(sagaId), 2);
  n = BookCatalog::readCollectionPage(sagaId, 0, 10, entries);
  ASSERT_EQ(n, 2);
  ASSERT_EQ(std::string(entries[0].title), "Saga Vol 1");
  ASSERT_EQ(std::string(entries[1].title), "Saga Vol 2");

  printf("  rebuild()/readLetterIndex()/readLetterPage()/readCollectionPage() ok\n");
  PASS();
}

void testGetSetCollectionNote() {
  TempStorageDir tmp;
  BookStore books;
  books.init();
  createSampleBooks(books);
  ASSERT_TRUE(BookCatalog::rebuild(BookStore::DIR_PATH));

  char sagaId[9] = {};
  ASSERT_TRUE(BookCatalog::resolveCollectionId("Saga Collection", sagaId));

  char note[65] = {};
  ASSERT_FALSE(BookCatalog::getCollectionNote(sagaId, note, sizeof(note)));

  ASSERT_TRUE(BookCatalog::setCollectionNote(sagaId, "Great series"));
  ASSERT_TRUE(BookCatalog::getCollectionNote(sagaId, note, sizeof(note)));
  ASSERT_EQ(std::string(note), "Great series");

  // An empty note removes the note file.
  ASSERT_TRUE(BookCatalog::setCollectionNote(sagaId, ""));
  ASSERT_FALSE(BookCatalog::getCollectionNote(sagaId, note, sizeof(note)));

  printf("  getCollectionNote()/setCollectionNote() round-trip and clear ok\n");
  PASS();
}

void testRenameCollection() {
  TempStorageDir tmp;
  BookStore books;
  books.init();
  SampleBooks s = createSampleBooks(books);
  ASSERT_TRUE(BookCatalog::rebuild(BookStore::DIR_PATH));

  char sagaId[9] = {};
  ASSERT_TRUE(BookCatalog::resolveCollectionId("Saga Collection", sagaId));

  ASSERT_FALSE(Storage.exists(BookCatalog::SYNC_FLAG_PATH));
  ASSERT_TRUE(BookCatalog::renameCollection(sagaId, "Saga Renamed"));
  ASSERT_TRUE(Storage.exists(BookCatalog::SYNC_FLAG_PATH));

  std::vector<CollectionEntry> colls;
  BookCatalog::forEachCollection(collectCollection, &colls);
  bool found = false;
  for (const auto& c : colls) {
    if (c.id == sagaId) {
      ASSERT_EQ(c.name, "Saga Renamed");
      found = true;
    }
  }
  ASSERT_TRUE(found);

  PhysicalBook out;
  ASSERT_TRUE(books.get(s.saga1.id, out));
  ASSERT_EQ(out.collection, "Saga Renamed");
  ASSERT_TRUE(books.get(s.saga2.id, out));
  ASSERT_EQ(out.collection, "Saga Renamed");

  // Renaming a non-existent collection id fails.
  ASSERT_FALSE(BookCatalog::renameCollection("deadbeef", "Whatever"));

  printf("  renameCollection() updates the registry, member books, and sets the sync flag\n");
  PASS();
}

void testApplyBookChangeBeforeRebuild() {
  TempStorageDir tmp;
  BookStore books;
  books.init();
  PhysicalBook b = makeBook(books, "Some Book", "Author");

  BookCatalog::BookChangeInfo info = toChangeInfo(b);
  ASSERT_FALSE(BookCatalog::applyBookChange(nullptr, &info));

  printf("  applyBookChange() returns false before the catalog is built\n");
  PASS();
}

void testApplyBookChangeAddRemoveStandalone() {
  TempStorageDir tmp;
  BookStore books;
  books.init();
  SampleBooks s = createSampleBooks(books);
  ASSERT_TRUE(BookCatalog::rebuild(BookStore::DIR_PATH));

  // Add a new standalone book under a fresh letter.
  PhysicalBook zulu = makeBook(books, "Zulu Adventures", "Author Z");
  BookCatalog::BookChangeInfo zuluInfo = toChangeInfo(zulu);
  ASSERT_TRUE(BookCatalog::applyBookChange(nullptr, &zuluInfo));

  ASSERT_EQ(BookCatalog::letterCount('Z'), 1);
  BookCatalog::Entry entries[10];
  int n = BookCatalog::readLetterPage('Z', 0, 10, entries);
  ASSERT_EQ(n, 1);
  ASSERT_EQ(std::string(entries[0].title), "Zulu Adventures");
  ASSERT_EQ(std::string(entries[0].id), zulu.id);

  // Remove an existing standalone book.
  ASSERT_EQ(BookCatalog::letterCount('A'), 1);
  BookCatalog::BookChangeInfo alphaInfo = toChangeInfo(s.alpha);
  ASSERT_TRUE(BookCatalog::applyBookChange(&alphaInfo, nullptr));
  ASSERT_EQ(BookCatalog::letterCount('A'), 0);

  printf("  applyBookChange() adds and removes standalone books\n");
  PASS();
}

void testApplyBookChangeUpdateStandalone() {
  TempStorageDir tmp;
  BookStore books;
  books.init();
  SampleBooks s = createSampleBooks(books);
  ASSERT_TRUE(BookCatalog::rebuild(BookStore::DIR_PATH));

  PhysicalBook updated = s.banana;
  updated.title = "Cactus Book";
  ASSERT_TRUE(books.update(updated));

  BookCatalog::BookChangeInfo oldInfo = toChangeInfo(s.banana);
  BookCatalog::BookChangeInfo newInfo = toChangeInfo(updated);
  ASSERT_TRUE(BookCatalog::applyBookChange(&oldInfo, &newInfo));

  ASSERT_EQ(BookCatalog::letterCount('B'), 0);
  ASSERT_EQ(BookCatalog::letterCount('C'), 1);

  BookCatalog::Entry entries[10];
  int n = BookCatalog::readLetterPage('C', 0, 10, entries);
  ASSERT_EQ(n, 1);
  ASSERT_EQ(std::string(entries[0].title), "Cactus Book");

  printf("  applyBookChange() moves a book between letters on update\n");
  PASS();
}

void testApplyBookChangeCollectionLifecycle() {
  TempStorageDir tmp;
  BookStore books;
  books.init();
  createSampleBooks(books);
  ASSERT_TRUE(BookCatalog::rebuild(BookStore::DIR_PATH));

  char sagaId[9] = {};
  ASSERT_TRUE(BookCatalog::resolveCollectionId("Saga Collection", sagaId));
  ASSERT_EQ(BookCatalog::collectionCount(sagaId), 2);
  ASSERT_TRUE(BookCatalog::setCollectionExpectedCount(sagaId, 4));
  ASSERT_TRUE(BookCatalog::setCollectionInitialVolume(sagaId, 2));

  // Add a third book to the existing "Saga Collection".
  PhysicalBook saga3 = makeBook(books, "Saga Vol 3", "Author C", "Saga Collection");
  BookCatalog::BookChangeInfo saga3Info = toChangeInfo(saga3);
  ASSERT_TRUE(BookCatalog::applyBookChange(nullptr, &saga3Info));
  ASSERT_EQ(BookCatalog::collectionCount(sagaId), 3);

  BookCatalog::Entry entries[10];
  int n = BookCatalog::readLetterPage('S', 0, 10, entries);
  ASSERT_TRUE(n >= 1);
  ASSERT_TRUE(entries[0].isCollection);
  ASSERT_EQ(std::string(entries[0].id), std::string(sagaId));
  ASSERT_EQ(entries[0].count, 3);
  ASSERT_EQ(entries[0].expectedCount, 4);
  ASSERT_EQ(entries[0].initialVolume, 2);

  // Add a book that creates a brand-new collection ("Trilogy", letter T).
  PhysicalBook trilogy1 = makeBook(books, "Trilogy Vol 1", "Author E", "Trilogy");
  BookCatalog::BookChangeInfo trilogyInfo = toChangeInfo(trilogy1);
  ASSERT_TRUE(BookCatalog::applyBookChange(nullptr, &trilogyInfo));

  char trilogyId[9] = {};
  ASSERT_TRUE(BookCatalog::resolveCollectionId("Trilogy", trilogyId));
  ASSERT_EQ(BookCatalog::letterCount('T'), 1);
  ASSERT_EQ(BookCatalog::collectionCount(trilogyId), 1);

  n = BookCatalog::readLetterPage('T', 0, 10, entries);
  ASSERT_EQ(n, 1);
  ASSERT_TRUE(entries[0].isCollection);
  ASSERT_EQ(std::string(entries[0].title), "Trilogy");
  ASSERT_EQ(entries[0].count, 1);

  // Removing the only book in "Trilogy" deletes the whole collection.
  ASSERT_TRUE(BookCatalog::applyBookChange(&trilogyInfo, nullptr));
  ASSERT_EQ(BookCatalog::letterCount('T'), 0);
  ASSERT_EQ(BookCatalog::collectionCount(trilogyId), 0);

  std::vector<CollectionEntry> colls;
  BookCatalog::forEachCollection(collectCollection, &colls);
  for (const auto& c : colls) ASSERT_TRUE(c.id != trilogyId);

  printf("  applyBookChange() handles collection growth, creation, and deletion\n");
  PASS();
}

int main() {
  printf("=== BookCatalog Tests ===\n\n");

  testResolveCollectionIdAndForEachCollection();
  testCollectionExpectedCount();
  testRebuildAndReadCatalog();
  testGetSetCollectionNote();
  testRenameCollection();
  testApplyBookChangeBeforeRebuild();
  testApplyBookChangeAddRemoveStandalone();
  testApplyBookChangeUpdateStandalone();
  testApplyBookChangeCollectionLifecycle();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
