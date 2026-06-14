#include "TimeSeriesStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

void TimeSeriesStore::init() const { Storage.mkdir(dir_); }

bool TimeSeriesStore::readSummary(void* out, size_t size) const {
  char path[80];
  snprintf(path, sizeof(path), "%s/summary.bin", dir_);
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("TSTORE", path, file)) return false;
  // Storage.readFileToBuffer() reads bufferSize-1 bytes (text/null-terminator
  // convention); read the raw fixed-size blob directly to avoid losing the
  // last byte.
  return file.read(out, size) == static_cast<int>(size);
}

bool TimeSeriesStore::writeSummary(const void* data, size_t size) const {
  char path[80];
  snprintf(path, sizeof(path), "%s/summary.bin", dir_);
  HalFile f;
  if (!Storage.openFileForWrite("TSTORE", path, f)) {
    LOG_ERR("TSTORE", "Failed to write summary");
    return false;
  }
  f.write(static_cast<const uint8_t*>(data), size);
  return true;
}

void TimeSeriesStore::readYear(int year, int* out12) const {
  memset(out12, 0, 12 * sizeof(int));
  char path[64];
  snprintf(path, sizeof(path), "%s/%d.bin", dir_, year);
  if (!Storage.exists(path)) return;
  uint8_t rec[24] = {};
  HalFile file;
  if (!Storage.openFileForRead("TSTORE", path, file)) return;
  file.read(rec, sizeof(rec));
  for (int i = 0; i < 12; ++i) {
    int16_t v;
    memcpy(&v, &rec[i * 2], 2);
    out12[i] = v;
  }
}

void TimeSeriesStore::writeYear(int year, const int16_t* data12) const {
  char path[64];
  snprintf(path, sizeof(path), "%s/%d.bin", dir_, year);
  uint8_t rec[24] = {};
  for (int i = 0; i < 12; ++i) memcpy(&rec[i * 2], &data12[i], 2);
  HalFile f;
  if (Storage.openFileForWrite("TSTORE", path, f)) f.write(rec, 24);
}

void TimeSeriesStore::readMonth(int year, int month, int* out31) const {
  memset(out31, 0, 31 * sizeof(int));
  char path[64];
  snprintf(path, sizeof(path), "%s/%d-%02d.bin", dir_, year, month);
  if (!Storage.exists(path)) return;
  uint8_t rec[62] = {};
  HalFile file;
  if (!Storage.openFileForRead("TSTORE", path, file)) return;
  file.read(rec, sizeof(rec));
  for (int i = 0; i < 31; ++i) {
    int16_t v;
    memcpy(&v, &rec[i * 2], 2);
    out31[i] = v;
  }
}

void TimeSeriesStore::writeMonth(int year, int month, const int16_t* data31) const {
  char path[64];
  snprintf(path, sizeof(path), "%s/%d-%02d.bin", dir_, year, month);
  uint8_t rec[62] = {};
  for (int i = 0; i < 31; ++i) memcpy(&rec[i * 2], &data31[i], 2);
  HalFile f;
  if (Storage.openFileForWrite("TSTORE", path, f)) f.write(rec, 62);
}
