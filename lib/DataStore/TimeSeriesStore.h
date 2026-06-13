#pragma once
#include <cstddef>
#include <cstdint>

// Binary time-series store for temporal aggregates.
//
// On disk:
//   {dir}/summary.bin        arbitrary fixed-size global summary blob
//   {dir}/{year}.bin         12 × int16_t LE  (sessions per month)
//   {dir}/{year}-{mm}.bin    31 × int16_t LE  (sessions per day)
class TimeSeriesStore {
 public:
  explicit constexpr TimeSeriesStore(const char* dir) : dir_(dir) {}

  // Ensure directory exists.
  void init() const;

  // Read/write arbitrary-size global summary blob.
  bool readSummary(void* out, size_t size) const;
  bool writeSummary(const void* data, size_t size) const;

  // out12 must be int[12]. Zero-filled when file missing.
  void readYear(int year, int* out12) const;
  // data12 is const int16_t[12].
  void writeYear(int year, const int16_t* data12) const;

  // out31 must be int[31]. Zero-filled when file missing.
  void readMonth(int year, int month, int* out31) const;
  // data31 is const int16_t[31].
  void writeMonth(int year, int month, const int16_t* data31) const;

  const char* dir() const { return dir_; }

 private:
  const char* dir_;
};
