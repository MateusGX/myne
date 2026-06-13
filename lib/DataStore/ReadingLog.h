#pragma once
#include "RecordStore.h"
#include "TimeSeriesStore.h"

#include <string>
#include <vector>

enum class ReadingType : uint8_t { Page = 0, Chapter = 1 };

enum class ReadingStatus : uint8_t {
  WantToRead = 0,
  Reading    = 1,
  Paused     = 2,
  Finished   = 3,
  Dropped    = 4
};

struct ReadingSession {
  std::string date;  // "YYYY-MM-DD" or empty if clock not synced
  std::string time;  // "HH:MM" or empty if not recorded
  int position = 0;
};

struct Reading {
  std::string id;
  ReadingStatus status      = ReadingStatus::Reading;
  ReadingType   readingType = ReadingType::Page;
  std::vector<ReadingSession> sessions;

  int lastPosition() const {
    return sessions.empty() ? 0 : sessions.back().position;
  }
  const std::string& lastDate() const {
    static const std::string empty;
    return sessions.empty() ? empty : sessions.back().date;
  }
};

// 17-byte binary snapshot for fast book-detail-view access.
// Binary layout:
//   [0]    hasReading  (uint8_t)
//   [1]    status      (uint8_t, ReadingStatus)
//   [2]    readingType (uint8_t, ReadingType)
//   [3-6]  lastPosition (int32_t LE)
//   [7-16] lastDate    (char[10] "YYYY-MM-DD", zero-padded)
struct ReadingSummary {
  bool          hasReading   = false;
  ReadingStatus status       = ReadingStatus::Reading;
  ReadingType   readingType  = ReadingType::Page;
  int           lastPosition = 0;
  char          lastDate[11] = {};  // "YYYY-MM-DD\0"
};

// 40-byte global stats cache.
// Binary layout (STATS_SUM_SIZE = 40 bytes):
//   [0-3]   totalBooks    (int32_t LE)
//   [4-7]   totalReadings (int32_t LE)
//   [8-11]  totalSessions (int32_t LE)
//   [12-31] byStatus[5]   (int32_t LE each: WantToRead…Dropped)
//   [32-33] byTracking[0] (int16_t LE: Page)
//   [34-35] byTracking[1] (int16_t LE: Chapter)
//   [36-37] latestYear    (int16_t LE)
//   [38-39] latestMonth   (int16_t LE, 1-12)
struct StatsCache {
  int32_t totalBooks    = 0;
  int32_t totalReadings = 0;
  int32_t totalSessions = 0;
  int32_t byStatus[5]   = {};  // WantToRead … Dropped
  int16_t byTracking[2] = {};  // Page, Chapter
  int16_t latestYear    = 2024;
  int16_t latestMonth   = 1;
};

class ReadingLog {
 public:
  static constexpr const char* DIR_PATH       = "/.myne/readings";
  static constexpr const char* SUM_DIR_PATH   = "/.myne/readings-sum";
  static constexpr const char* STATS_DIR_PATH = "/.myne/stats-cache";
  static constexpr size_t      MAX_SESSIONS   = 200;
  static constexpr size_t      SUM_REC_SIZE   = 17;
  static constexpr size_t      STATS_SUM_SIZE = 40;

  // Load all readings for a book. Returns empty vector if none.
  std::vector<Reading> loadForBook(const std::string& bookId) const;

  // Write readings JSON + binary summary for this book.
  bool saveForBook(const std::string& bookId,
                   const std::vector<Reading>& readings) const;

  // Load the compact binary summary (lazy-generates from JSON on first call).
  // Returns false if no readings exist for this book.
  bool loadSummaryForBook(const std::string& bookId,
                          ReadingSummary& out) const;

  // Delete readings JSON and summary binary for this book.
  void deleteForBook(const std::string& bookId) const;

  // ── Stats cache ──────────────────────────────────────────────────────────────

  // Rebuild stats-cache from all readings JSON files (one-time O(N) pass).
  // booksDirPath: directory to count for totalBooks (pass BookStore::DIR_PATH).
  // onProgress(ctx, done, total) is called before the loop (done=0) and after
  // each file. Pass nullptr to skip progress reporting.
  void rebuildStats(const char* booksDirPath,
                    void (*onProgress)(void* ctx, int done, int total) = nullptr,
                    void* ctx = nullptr) const;

  // Load the pre-computed global stats summary. Returns false if not built yet.
  bool loadStatsSummary(StatsCache& out) const;

  // Recompute and rewrite just the totalBooks field of the stats cache after a
  // book is added or removed. No-op if the stats cache hasn't been built yet.
  void refreshTotalBooks(const char* booksDirPath) const;

  // Load per-year session counts into out12[12] (one int per month).
  // Zero-fills if the year file is missing.
  void loadStatsYear(int year, int* out12) const;

  // Load per-month session counts into out31[31] (one int per day, index 0 = day 1).
  // Zero-fills if the month file is missing.
  void loadStatsMonth(int year, int month, int* out31) const;

  static const char* statusToStr(ReadingStatus s);
  static std::string currentDateString();
  static std::string newId();

 private:
  RecordStore     store_{DIR_PATH, SUM_DIR_PATH, SUM_REC_SIZE};
  TimeSeriesStore statsStore_{STATS_DIR_PATH};

  static ReadingStatus strToStatus(const char* s);
  void saveSummaryBinary(const std::string& bookId,
                         const std::vector<Reading>& readings) const;
};
