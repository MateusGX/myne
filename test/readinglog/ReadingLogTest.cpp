#include <HalStorage.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

#include "../datastore_common/TestHarness.h"
#include "BookStore.h"
#include "ReadingLog.h"

void testLoadForBookMissingReturnsEmpty() {
  TempStorageDir tmp;
  ReadingLog log;

  ASSERT_TRUE(log.loadForBook("missing").empty());

  printf("  loadForBook() on missing book returns empty vector\n");
  PASS();
}

void testSaveLoadRoundTrip() {
  TempStorageDir tmp;
  ReadingLog log;

  std::vector<Reading> readings;

  Reading r1;
  r1.id = "r1";
  r1.status = ReadingStatus::Finished;
  r1.readingType = ReadingType::Chapter;
  r1.sessions.push_back({"2024-01-01", "09:00", 10});
  r1.sessions.push_back({"2024-01-05", "20:15", 50});
  readings.push_back(r1);

  Reading r2;
  r2.id = "r2";
  r2.status = ReadingStatus::Reading;
  r2.readingType = ReadingType::Page;
  r2.sessions.push_back({"2024-02-01", "", 5});
  readings.push_back(r2);

  ASSERT_TRUE(log.saveForBook("book1", readings));

  auto out = log.loadForBook("book1");
  ASSERT_EQ(out.size(), (size_t)2);

  ASSERT_EQ(out[0].id, "r1");
  ASSERT_EQ((int)out[0].status, (int)ReadingStatus::Finished);
  ASSERT_EQ((int)out[0].readingType, (int)ReadingType::Chapter);
  ASSERT_EQ(out[0].sessions.size(), (size_t)2);
  ASSERT_EQ(out[0].sessions[0].date, "2024-01-01");
  ASSERT_EQ(out[0].sessions[0].time, "09:00");
  ASSERT_EQ(out[0].sessions[0].position, 10);
  ASSERT_EQ(out[0].lastPosition(), 50);
  ASSERT_EQ(out[0].lastDate(), "2024-01-05");

  ASSERT_EQ(out[1].id, "r2");
  ASSERT_EQ((int)out[1].status, (int)ReadingStatus::Reading);
  ASSERT_EQ((int)out[1].readingType, (int)ReadingType::Page);
  ASSERT_EQ(out[1].sessions[0].time, "");

  printf("  saveForBook()/loadForBook() round-trip ok\n");
  PASS();
}

void testStatusRoundTripForAllValues() {
  TempStorageDir tmp;
  ReadingLog log;

  ReadingStatus all[] = {ReadingStatus::WantToRead, ReadingStatus::Reading, ReadingStatus::Paused,
                         ReadingStatus::Finished, ReadingStatus::Dropped};

  std::vector<Reading> readings;
  for (size_t i = 0; i < 5; ++i) {
    Reading r;
    r.id = "r" + std::to_string(i);
    r.status = all[i];
    readings.push_back(r);
  }
  ASSERT_TRUE(log.saveForBook("book1", readings));

  auto out = log.loadForBook("book1");
  ASSERT_EQ(out.size(), (size_t)5);
  for (size_t i = 0; i < 5; ++i) {
    ASSERT_EQ((int)out[i].status, (int)all[i]);
  }

  printf("  statusToStr()/strToStatus() round-trip for all ReadingStatus values\n");
  PASS();
}

void testLoadSummaryForBookLazyMigration() {
  TempStorageDir tmp;
  ReadingLog log;

  std::vector<Reading> readings;
  Reading r;
  r.id = "r1";
  r.status = ReadingStatus::Reading;
  r.readingType = ReadingType::Page;
  r.sessions.push_back({"2024-01-15", "10:30", 42});
  readings.push_back(r);

  ASSERT_TRUE(log.saveForBook("book1", readings));

  // Remove the summary.bin to force lazy migration from JSON.
  RecordStore sumStore(ReadingLog::DIR_PATH, ReadingLog::SUM_DIR_PATH, ReadingLog::SUM_REC_SIZE);
  char sumPath[96];
  sumStore.sumPath(sumPath, sizeof(sumPath), "book1");
  Storage.remove(sumPath);
  ASSERT_FALSE(Storage.exists(sumPath));

  ReadingSummary out{};
  ASSERT_TRUE(log.loadSummaryForBook("book1", out));
  ASSERT_TRUE(out.hasReading);
  ASSERT_EQ((int)out.status, (int)ReadingStatus::Reading);
  ASSERT_EQ((int)out.readingType, (int)ReadingType::Page);
  ASSERT_EQ(out.lastPosition, 42);
  ASSERT_EQ(std::string(out.lastDate), "2024-01-15");

  // Lazy migration writes summary.bin back out.
  ASSERT_TRUE(Storage.exists(sumPath));

  printf("  loadSummaryForBook() lazily migrates from JSON when summary.bin is missing\n");
  PASS();
}

// ReadingSummary's on-disk lastDate (rec[7..16], "YYYY-MM-DD") must survive a
// full save/load round-trip through summary.bin, including its last byte
// (the final digit of the date). See lib/DataStore/core/RecordStore.cpp:92
// and lib/DataStore/readings/ReadingLog.cpp:184.
void testLoadSummaryForBookDateRoundTrip() {
  TempStorageDir tmp;
  ReadingLog log;

  std::vector<Reading> readings;
  Reading r;
  r.id = "r1";
  r.status = ReadingStatus::Reading;
  r.sessions.push_back({"2024-01-15", "", 42});
  readings.push_back(r);

  ASSERT_TRUE(log.saveForBook("book1", readings));

  ReadingSummary out{};
  ASSERT_TRUE(log.loadSummaryForBook("book1", out));
  ASSERT_EQ(std::string(out.lastDate), "2024-01-15");

  printf("  loadSummaryForBook() returns the full lastDate from summary.bin\n");
  PASS();
}

void testDeleteForBook() {
  TempStorageDir tmp;
  ReadingLog log;

  std::vector<Reading> readings;
  Reading r;
  r.id = "r1";
  r.sessions.push_back({"2024-01-01", "", 1});
  readings.push_back(r);
  ASSERT_TRUE(log.saveForBook("book1", readings));
  ASSERT_FALSE(log.loadForBook("book1").empty());

  log.deleteForBook("book1");
  ASSERT_TRUE(log.loadForBook("book1").empty());

  ReadingSummary out{};
  ASSERT_FALSE(log.loadSummaryForBook("book1", out));

  printf("  deleteForBook() removes the readings JSON and summary\n");
  PASS();
}

void testRebuildStatsAndLoadSummary() {
  TempStorageDir tmp;
  ReadingLog log;
  BookStore books;
  books.init();

  PhysicalBook b1;
  b1.title = "Book One";
  ASSERT_TRUE(books.create(b1));
  usleep(2000);

  PhysicalBook b2;
  b2.title = "Book Two";
  ASSERT_TRUE(books.create(b2));

  std::vector<Reading> r1list;
  Reading r1;
  r1.id = "r1";
  r1.status = ReadingStatus::Finished;
  r1.readingType = ReadingType::Page;
  r1.sessions.push_back({"2024-01-10", "", 1});
  r1.sessions.push_back({"2024-02-05", "", 2});
  r1list.push_back(r1);
  ASSERT_TRUE(log.saveForBook(b1.id, r1list));

  std::vector<Reading> r2list;
  Reading r2;
  r2.id = "r2";
  r2.status = ReadingStatus::Reading;
  r2.readingType = ReadingType::Chapter;
  r2.sessions.push_back({"2024-01-20", "", 3});
  r2list.push_back(r2);
  ASSERT_TRUE(log.saveForBook(b2.id, r2list));

  log.rebuildStats(BookStore::DIR_PATH);

  StatsCache stats{};
  ASSERT_TRUE(log.loadStatsSummary(stats));
  ASSERT_EQ(stats.totalBooks, 2);
  ASSERT_EQ(stats.totalReadings, 2);
  ASSERT_EQ(stats.totalSessions, 3);
  ASSERT_EQ(stats.byStatus[(int)ReadingStatus::Finished], 1);
  ASSERT_EQ(stats.byStatus[(int)ReadingStatus::Reading], 1);
  ASSERT_EQ(stats.byTracking[0], 1);  // Page (r1)
  ASSERT_EQ(stats.byTracking[1], 1);  // Chapter (r2)
  ASSERT_EQ(stats.latestYear, 2024);
  ASSERT_EQ(stats.latestMonth, 2);  // latest session date is 2024-02-05

  int year[12] = {};
  log.loadStatsYear(2024, year);
  ASSERT_EQ(year[0], 2);  // January: r1 (1/10) + r2 (1/20)
  ASSERT_EQ(year[1], 1);  // February: r1 (2/5)

  int month1[31] = {};
  log.loadStatsMonth(2024, 1, month1);
  ASSERT_EQ(month1[9], 1);   // day 10
  ASSERT_EQ(month1[19], 1);  // day 20

  printf("  rebuildStats()/loadStatsSummary()/loadStatsYear()/loadStatsMonth() ok\n");
  PASS();
}

void testRefreshTotalBooks() {
  TempStorageDir tmp;
  ReadingLog log;
  BookStore books;
  books.init();

  PhysicalBook b1;
  b1.title = "Book One";
  ASSERT_TRUE(books.create(b1));

  // Stats cache not built yet — refreshTotalBooks() is a no-op.
  log.refreshTotalBooks(BookStore::DIR_PATH);
  StatsCache stats{};
  ASSERT_FALSE(log.loadStatsSummary(stats));

  log.rebuildStats(BookStore::DIR_PATH);
  ASSERT_TRUE(log.loadStatsSummary(stats));
  ASSERT_EQ(stats.totalBooks, 1);

  usleep(2000);
  PhysicalBook b2;
  b2.title = "Book Two";
  ASSERT_TRUE(books.create(b2));

  log.refreshTotalBooks(BookStore::DIR_PATH);
  ASSERT_TRUE(log.loadStatsSummary(stats));
  ASSERT_EQ(stats.totalBooks, 2);

  printf("  refreshTotalBooks() updates totalBooks without a full rebuild\n");
  PASS();
}

void testLoadStatsYearMonthZeroFillWhenMissing() {
  TempStorageDir tmp;
  ReadingLog log;

  int year[12];
  for (int& v : year) v = 99;
  log.loadStatsYear(1999, year);
  for (int v : year) ASSERT_EQ(v, 0);

  int month[31];
  for (int& v : month) v = 99;
  log.loadStatsMonth(1999, 1, month);
  for (int v : month) ASSERT_EQ(v, 0);

  printf("  loadStatsYear()/loadStatsMonth() zero-fill when cache is missing\n");
  PASS();
}

void testNewIdGeneratesDistinctIds() {
  std::string id1 = ReadingLog::newId();
  ASSERT_FALSE(id1.empty());

  usleep(2000);
  std::string id2 = ReadingLog::newId();
  ASSERT_TRUE(id1 != id2);

  printf("  newId() generates distinct millis()-based ids\n");
  PASS();
}

int main() {
  printf("=== ReadingLog Tests ===\n\n");

  testLoadForBookMissingReturnsEmpty();
  testSaveLoadRoundTrip();
  testStatusRoundTripForAllValues();
  testLoadSummaryForBookLazyMigration();
  testLoadSummaryForBookDateRoundTrip();
  testDeleteForBook();
  testRebuildStatsAndLoadSummary();
  testRefreshTotalBooks();
  testLoadStatsYearMonthZeroFillWhenMissing();
  testNewIdGeneratesDistinctIds();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
