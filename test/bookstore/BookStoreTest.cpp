#include <HalStorage.h>
#include <unistd.h>

#include <cstdio>

#include "../datastore_common/TestHarness.h"
#include "BookStore.h"

void testInitCreatesDir() {
  TempStorageDir tmp;
  BookStore store;
  ASSERT_FALSE(Storage.exists(BookStore::DIR_PATH));

  ASSERT_TRUE(store.init());
  ASSERT_TRUE(Storage.exists(BookStore::DIR_PATH));

  // init() is idempotent.
  ASSERT_TRUE(store.init());

  printf("  init() created %s and is idempotent\n", BookStore::DIR_PATH);
  PASS();
}

void testCreateAssignsId() {
  TempStorageDir tmp;
  BookStore store;
  store.init();

  PhysicalBook book;
  book.title = "The Pragmatic Programmer";
  book.author = "Hunt & Thomas";

  ASSERT_TRUE(book.id.empty());
  ASSERT_TRUE(store.create(book));
  ASSERT_FALSE(book.id.empty());

  PhysicalBook out;
  ASSERT_TRUE(store.get(book.id, out));
  ASSERT_EQ(out.id, book.id);
  ASSERT_EQ(out.title, book.title);
  ASSERT_EQ(out.author, book.author);

  printf("  create() assigns id %s and persists the book\n", book.id.c_str());
  PASS();
}

void testCreateMultipleBooksGetDistinctIds() {
  TempStorageDir tmp;
  BookStore store;
  store.init();

  PhysicalBook a;
  a.title = "Book A";
  ASSERT_TRUE(store.create(a));

  // generateId() is millis()-based; sleep to guarantee a different id.
  usleep(2000);

  PhysicalBook b;
  b.title = "Book B";
  ASSERT_TRUE(store.create(b));

  ASSERT_TRUE(a.id != b.id);
  ASSERT_EQ(store.countBooks(), 2);

  printf("  create() assigns distinct ids (%s, %s)\n", a.id.c_str(), b.id.c_str());
  PASS();
}

void testUpdateRequiresExistence() {
  TempStorageDir tmp;
  BookStore store;
  store.init();

  PhysicalBook ghost;
  ghost.id = "does-not-exist";
  ghost.title = "Ghost Book";
  ASSERT_FALSE(store.update(ghost));

  printf("  update() on a non-existent book returns false\n");
  PASS();
}

void testUpdateOverwrites() {
  TempStorageDir tmp;
  BookStore store;
  store.init();

  PhysicalBook book;
  book.title = "Original Title";
  book.author = "Original Author";
  ASSERT_TRUE(store.create(book));

  book.title = "Updated Title";
  book.notes = "Some notes";
  ASSERT_TRUE(store.update(book));

  PhysicalBook out;
  ASSERT_TRUE(store.get(book.id, out));
  ASSERT_EQ(out.title, "Updated Title");
  ASSERT_EQ(out.author, "Original Author");
  ASSERT_EQ(out.notes, "Some notes");

  printf("  update() overwrites the stored book\n");
  PASS();
}

void testGetMissingReturnsFalse() {
  TempStorageDir tmp;
  BookStore store;
  store.init();

  PhysicalBook out;
  ASSERT_FALSE(store.get("missing", out));

  printf("  get() on a missing book returns false\n");
  PASS();
}

void testRemoveRequiresExistence() {
  TempStorageDir tmp;
  BookStore store;
  store.init();

  ASSERT_FALSE(store.remove("missing"));

  PhysicalBook book;
  book.title = "Removable";
  ASSERT_TRUE(store.create(book));
  ASSERT_EQ(store.countBooks(), 1);

  ASSERT_TRUE(store.remove(book.id));
  ASSERT_EQ(store.countBooks(), 0);

  PhysicalBook out;
  ASSERT_FALSE(store.get(book.id, out));

  printf("  remove() requires existence and deletes the book\n");
  PASS();
}

void testCountBooks() {
  TempStorageDir tmp;
  BookStore store;
  store.init();

  ASSERT_EQ(store.countBooks(), 0);

  for (int i = 0; i < 3; ++i) {
    PhysicalBook b;
    b.title = "Book";
    ASSERT_TRUE(store.create(b));
    usleep(2000);
  }

  ASSERT_EQ(store.countBooks(), 3);

  printf("  countBooks() reflects the number of stored books\n");
  PASS();
}

int main() {
  printf("=== BookStore Tests ===\n\n");

  testInitCreatesDir();
  testCreateAssignsId();
  testCreateMultipleBooksGetDistinctIds();
  testUpdateRequiresExistence();
  testUpdateOverwrites();
  testGetMissingReturnsFalse();
  testRemoveRequiresExistence();
  testCountBooks();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
