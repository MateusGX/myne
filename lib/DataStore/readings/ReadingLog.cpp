#include "ReadingLog.h"

#include <DataStoreUtils.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <ctime>

// ── Status helpers ────────────────────────────────────────────────────────────

const char* ReadingLog::statusToStr(ReadingStatus s) {
  switch (s) {
    case ReadingStatus::WantToRead:
      return "want";
    case ReadingStatus::Reading:
      return "reading";
    case ReadingStatus::Paused:
      return "paused";
    case ReadingStatus::Finished:
      return "finished";
    case ReadingStatus::Dropped:
      return "dropped";
  }
  return "reading";
}

ReadingStatus ReadingLog::strToStatus(const char* s) {
  if (!s) return ReadingStatus::Reading;
  if (strcmp(s, "want") == 0) return ReadingStatus::WantToRead;
  if (strcmp(s, "paused") == 0) return ReadingStatus::Paused;
  if (strcmp(s, "finished") == 0) return ReadingStatus::Finished;
  if (strcmp(s, "dropped") == 0) return ReadingStatus::Dropped;
  return ReadingStatus::Reading;
}

// ── Date / ID helpers ─────────────────────────────────────────────────────────

std::string ReadingLog::currentDateString() {
  char buf[11];
  DataStoreUtils::currentDate(buf);
  return buf;
}

std::string ReadingLog::newId() {
  char buf[16];
  DataStoreUtils::generateId(buf, sizeof(buf));
  return buf;
}

// ── Load ──────────────────────────────────────────────────────────────────────

std::vector<Reading> ReadingLog::loadForBook(const std::string& bookId) const {
  struct Ctx {
    std::vector<Reading>* result;
  };

  std::vector<Reading> result;

  auto deserializeFn = [](JsonDocument& doc, void* data) -> bool {
    auto& result = *static_cast<std::vector<Reading>*>(data);
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return false;

    result.reserve(arr.size());
    for (JsonObject obj : arr) {
      Reading r;
      r.id = obj["id"] | "";
      if (r.id.empty()) continue;
      r.status = [&]() -> ReadingStatus {
        const char* s = obj["s"] | "reading";
        if (strcmp(s, "want") == 0) return ReadingStatus::WantToRead;
        if (strcmp(s, "paused") == 0) return ReadingStatus::Paused;
        if (strcmp(s, "finished") == 0) return ReadingStatus::Finished;
        if (strcmp(s, "dropped") == 0) return ReadingStatus::Dropped;
        return ReadingStatus::Reading;
      }();
      r.readingType = (obj["rt"] | 0) == 1 ? ReadingType::Chapter : ReadingType::Page;

      JsonArray sessions = obj["sessions"].as<JsonArray>();
      if (!sessions.isNull()) {
        const size_t cap = std::min(sessions.size(), MAX_SESSIONS);
        r.sessions.reserve(cap);
        for (JsonObject sv : sessions) {
          if (r.sessions.size() >= MAX_SESSIONS) break;
          ReadingSession s;
          s.date = sv["d"] | "";
          s.time = sv["tm"] | "";
          s.position = sv["p"] | 0;
          r.sessions.push_back(std::move(s));
        }
      }
      result.push_back(std::move(r));
    }
    return true;
  };

  store_.load(bookId.c_str(), deserializeFn, &result);
  return result;
}

// ── Save ──────────────────────────────────────────────────────────────────────

bool ReadingLog::saveForBook(const std::string& bookId, const std::vector<Reading>& readings) const {
  store_.init();

  auto serializeFn = [](JsonDocument& doc, const void* data) {
    const auto& readings = *static_cast<const std::vector<Reading>*>(data);
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& r : readings) {
      JsonObject obj = arr.add<JsonObject>();
      obj["id"] = r.id;
      obj["s"] = ReadingLog::statusToStr(r.status);
      if (r.readingType == ReadingType::Chapter) obj["rt"] = 1;
      JsonArray sv = obj["sessions"].to<JsonArray>();
      for (const auto& s : r.sessions) {
        JsonObject se = sv.add<JsonObject>();
        if (!s.date.empty()) se["d"] = s.date;
        if (!s.time.empty()) se["tm"] = s.time;
        se["p"] = s.position;
      }
    }
  };

  if (!store_.save(bookId.c_str(), serializeFn, &readings)) {
    LOG_ERR("RLOG", "Failed to save readings for %s", bookId.c_str());
    return false;
  }
  saveSummaryBinary(bookId, readings);
  return true;
}

// ── Summary binary ────────────────────────────────────────────────────────────

static const Reading* chooseBestReading(const std::vector<Reading>& readings) {
  const Reading* chosen = &readings.back();
  for (const auto& r : readings) {
    if (r.status == ReadingStatus::Reading) {
      chosen = &r;
      break;
    }
  }
  return chosen;
}

void ReadingLog::saveSummaryBinary(const std::string& bookId, const std::vector<Reading>& readings) const {
  uint8_t rec[SUM_REC_SIZE] = {};
  if (!readings.empty()) {
    const Reading* chosen = chooseBestReading(readings);
    rec[0] = 1;
    rec[1] = static_cast<uint8_t>(chosen->status);
    rec[2] = static_cast<uint8_t>(chosen->readingType);
    const int32_t pos = chosen->lastPosition();
    memcpy(&rec[3], &pos, 4);
    const auto& d = chosen->lastDate();
    const size_t len = d.size() < 10u ? d.size() : 10u;
    memcpy(&rec[7], d.data(), len);
  }
  store_.saveSummary(bookId.c_str(), rec);
}

bool ReadingLog::loadSummaryForBook(const std::string& bookId, ReadingSummary& out) const {
  uint8_t rec[SUM_REC_SIZE] = {};
  if (!store_.loadSummary(bookId.c_str(), rec)) {
    // Lazy migration: generate summary from JSON on first access.
    const auto readings = loadForBook(bookId);
    if (readings.empty()) return false;
    saveSummaryBinary(bookId, readings);
    const Reading* chosen = chooseBestReading(readings);
    out.hasReading = true;
    out.status = chosen->status;
    out.readingType = chosen->readingType;
    out.lastPosition = chosen->lastPosition();
    const auto& d = chosen->lastDate();
    const size_t len = d.size() < 10u ? d.size() : 10u;
    memcpy(out.lastDate, d.data(), len);
    out.lastDate[10] = '\0';
    return true;
  }
  out.hasReading = rec[0] != 0;
  out.status = static_cast<ReadingStatus>(rec[1]);
  out.readingType = static_cast<ReadingType>(rec[2]);
  int32_t pos;
  memcpy(&pos, &rec[3], 4);
  out.lastPosition = pos;
  memcpy(out.lastDate, &rec[7], 10);
  out.lastDate[10] = '\0';
  return true;
}

void ReadingLog::deleteForBook(const std::string& bookId) const { store_.remove(bookId.c_str()); }

// ── Stats cache ───────────────────────────────────────────────────────────────

namespace {

struct YearEntry {
  int16_t year;
  int16_t monthly[12];
};
struct MonthEntry {
  int16_t year;
  uint8_t month;
  int16_t daily[31];
};

struct RebuildCtx {
  JsonDocument doc;

  int32_t totalReadings = 0;
  int32_t totalSessions = 0;
  int32_t byStatus[5] = {};
  int16_t byTracking[2] = {};
  char latestDate[11] = {};

  std::vector<YearEntry> yearData;
  std::vector<MonthEntry> monthData;

  void (*onProgress)(void* ctx, int done, int total) = nullptr;
  void* progressCtx = nullptr;
  int processed = 0;
  int total = 0;
};

static bool processReadingFile(void* vctx, HalFile& file, const char* /*id*/) {
  auto& ctx = *static_cast<RebuildCtx*>(vctx);

  ctx.doc.clear();
  if (deserializeJson(ctx.doc, file) != DeserializationError::Ok) {
    if (ctx.onProgress) ctx.onProgress(ctx.progressCtx, ++ctx.processed, ctx.total);
    return true;
  }

  JsonArrayConst readings = ctx.doc.as<JsonArrayConst>();
  if (readings.isNull()) {
    if (ctx.onProgress) ctx.onProgress(ctx.progressCtx, ++ctx.processed, ctx.total);
    return true;
  }

  for (JsonObjectConst r : readings) {
    ++ctx.totalReadings;
    const char* s = r["s"] | "reading";
    int si = 1;
    if (strcmp(s, "want") == 0)
      si = 0;
    else if (strcmp(s, "paused") == 0)
      si = 2;
    else if (strcmp(s, "finished") == 0)
      si = 3;
    else if (strcmp(s, "dropped") == 0)
      si = 4;
    ++ctx.byStatus[si];
    ++ctx.byTracking[(r["rt"] | 0) == 1 ? 1 : 0];

    JsonArrayConst sessions = r["sessions"].as<JsonArrayConst>();
    if (sessions.isNull()) continue;

    for (JsonObjectConst sess : sessions) {
      ++ctx.totalSessions;
      const char* d = sess["d"] | "";
      if (d[0] == '\0') continue;
      if (strcmp(d, ctx.latestDate) > 0) {
        strncpy(ctx.latestDate, d, 10);
        ctx.latestDate[10] = '\0';
      }
      int y = 0, m = 0, day = 0;
      if (sscanf(d, "%d-%d-%d", &y, &m, &day) != 3) continue;
      if (m < 1 || m > 12 || day < 1 || day > 31) continue;

      YearEntry* ye = nullptr;
      for (auto& e : ctx.yearData) {
        if (e.year == (int16_t)y) {
          ye = &e;
          break;
        }
      }
      if (!ye) {
        ctx.yearData.push_back({(int16_t)y, {}});
        ye = &ctx.yearData.back();
      }
      ++ye->monthly[m - 1];

      MonthEntry* me = nullptr;
      for (auto& e : ctx.monthData) {
        if (e.year == (int16_t)y && e.month == (uint8_t)m) {
          me = &e;
          break;
        }
      }
      if (!me) {
        ctx.monthData.push_back({(int16_t)y, (uint8_t)m, {}});
        me = &ctx.monthData.back();
      }
      ++me->daily[day - 1];
    }
  }

  if (ctx.onProgress) ctx.onProgress(ctx.progressCtx, ++ctx.processed, ctx.total);
  return true;
}

}  // namespace

void ReadingLog::rebuildStats(const char* booksDirPath, void (*onProgress)(void* ctx, int done, int total),
                              void* ctx) const {
  statsStore_.init();

  const int totalBooks = RecordStore{booksDirPath}.count();
  const int totalFiles = store_.count();

  if (onProgress) onProgress(ctx, 0, totalFiles);

  RebuildCtx rctx;
  rctx.yearData.reserve(20);
  rctx.monthData.reserve(60);
  rctx.onProgress = onProgress;
  rctx.progressCtx = ctx;
  rctx.total = totalFiles;

  store_.forEachFile(&rctx, processReadingFile);

  // ── Write summary.bin ───────────────────────────────────────────────────────
  {
    uint8_t rec[STATS_SUM_SIZE] = {};
    const int32_t books = (int32_t)totalBooks;
    memcpy(&rec[0], &books, 4);
    memcpy(&rec[4], &rctx.totalReadings, 4);
    memcpy(&rec[8], &rctx.totalSessions, 4);
    for (int i = 0; i < 5; ++i) memcpy(&rec[12 + i * 4], &rctx.byStatus[i], 4);
    memcpy(&rec[32], &rctx.byTracking[0], 2);
    memcpy(&rec[34], &rctx.byTracking[1], 2);
    int16_t ly = 2024, lm = 1;
    if (rctx.latestDate[0] != '\0') {
      int iy = 0, im = 0, id = 0;
      sscanf(rctx.latestDate, "%d-%d-%d", &iy, &im, &id);
      ly = (int16_t)iy;
      lm = (int16_t)im;
    }
    memcpy(&rec[36], &ly, 2);
    memcpy(&rec[38], &lm, 2);
    statsStore_.writeSummary(rec, STATS_SUM_SIZE);
  }

  // ── Write year bins: {YYYY}.bin — 12 × int16_t ─────────────────────────────
  for (const auto& ye : rctx.yearData) {
    statsStore_.writeYear((int)ye.year, ye.monthly);
  }

  // ── Write month bins: {YYYY-MM}.bin — 31 × int16_t ─────────────────────────
  for (const auto& me : rctx.monthData) {
    statsStore_.writeMonth((int)me.year, (int)me.month, me.daily);
  }

  LOG_INF("RLOG", "Stats rebuilt: %d books, %d sessions", totalBooks, (int)rctx.totalSessions);
}

bool ReadingLog::loadStatsSummary(StatsCache& out) const {
  uint8_t rec[STATS_SUM_SIZE] = {};
  if (!statsStore_.readSummary(rec, STATS_SUM_SIZE)) return false;

  int32_t v32;
  memcpy(&v32, &rec[0], 4);
  out.totalBooks = v32;
  memcpy(&v32, &rec[4], 4);
  out.totalReadings = v32;
  memcpy(&v32, &rec[8], 4);
  out.totalSessions = v32;
  for (int i = 0; i < 5; ++i) {
    memcpy(&v32, &rec[12 + i * 4], 4);
    out.byStatus[i] = v32;
  }
  int16_t v16;
  memcpy(&v16, &rec[32], 2);
  out.byTracking[0] = v16;
  memcpy(&v16, &rec[34], 2);
  out.byTracking[1] = v16;
  memcpy(&v16, &rec[36], 2);
  out.latestYear = v16;
  memcpy(&v16, &rec[38], 2);
  out.latestMonth = v16;
  return true;
}

void ReadingLog::refreshTotalBooks(const char* booksDirPath) const {
  uint8_t rec[STATS_SUM_SIZE] = {};
  if (!statsStore_.readSummary(rec, STATS_SUM_SIZE)) return;  // stats cache not built yet

  const int32_t totalBooks = static_cast<int32_t>(RecordStore{booksDirPath}.count());
  memcpy(&rec[0], &totalBooks, 4);
  statsStore_.writeSummary(rec, STATS_SUM_SIZE);
}

void ReadingLog::loadStatsYear(int year, int* out12) const { statsStore_.readYear(year, out12); }

void ReadingLog::loadStatsMonth(int year, int month, int* out31) const { statsStore_.readMonth(year, month, out31); }
