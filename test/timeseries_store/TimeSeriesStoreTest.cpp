#include <HalStorage.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../datastore_common/TestHarness.h"
#include "TimeSeriesStore.h"

static constexpr const char* STATS_DIR = "/.myne/stats-cache";

void testInitCreatesDir() {
  TempStorageDir tmp;
  TimeSeriesStore store(STATS_DIR);
  ASSERT_FALSE(Storage.exists(STATS_DIR));

  store.init();
  ASSERT_TRUE(Storage.exists(STATS_DIR));

  printf("  init() created %s\n", STATS_DIR);
  PASS();
}

void testSummaryRoundTrip() {
  TempStorageDir tmp;
  TimeSeriesStore store(STATS_DIR);
  store.init();

  struct Blob {
    int32_t a;
    int16_t b;
    uint8_t c[6];
  };
  Blob in{12345, -7, {1, 2, 3, 4, 5, 0xAA}};

  ASSERT_TRUE(store.writeSummary(&in, sizeof(in)));

  Blob out{};
  ASSERT_TRUE(store.readSummary(&out, sizeof(out)));
  ASSERT_EQ(out.a, in.a);
  ASSERT_EQ(out.b, in.b);
  ASSERT_TRUE(memcmp(out.c, in.c, sizeof(in.c)) == 0);

  printf("  summary.bin round-trip ok, including the last byte\n");
  PASS();
}

void testReadSummaryMissingReturnsFalse() {
  TempStorageDir tmp;
  TimeSeriesStore store(STATS_DIR);
  store.init();

  uint8_t out[8] = {};
  ASSERT_FALSE(store.readSummary(out, sizeof(out)));

  printf("  readSummary() on missing file returns false\n");
  PASS();
}

void testYearRoundTrip() {
  TempStorageDir tmp;
  TimeSeriesStore store(STATS_DIR);
  store.init();

  int16_t in12[12];
  for (int i = 0; i < 12; ++i) in12[i] = static_cast<int16_t>((i + 1) * 3);
  in12[11] = 300;  // December: exercise the int16 high byte

  store.writeYear(2025, in12);

  int out12[12] = {};
  store.readYear(2025, out12);
  for (int i = 0; i < 12; ++i) ASSERT_EQ(out12[i], (int)in12[i]);

  printf("  {year}.bin round-trip ok\n");
  PASS();
}

void testYearMissingFileZeroFills() {
  TempStorageDir tmp;
  TimeSeriesStore store(STATS_DIR);
  store.init();

  int out12[12];
  for (int i = 0; i < 12; ++i) out12[i] = 0xAAAA;  // non-zero sentinel
  store.readYear(1999, out12);
  for (int i = 0; i < 12; ++i) ASSERT_EQ(out12[i], 0);

  printf("  readYear() zero-fills when %s/1999.bin is missing\n", STATS_DIR);
  PASS();
}

void testMonthRoundTrip() {
  TempStorageDir tmp;
  TimeSeriesStore store(STATS_DIR);
  store.init();

  int16_t in31[31];
  for (int i = 0; i < 31; ++i) in31[i] = static_cast<int16_t>(i);
  in31[30] = 300;  // day 31: exercise the int16 high byte

  store.writeMonth(2025, 3, in31);

  int out31[31] = {};
  store.readMonth(2025, 3, out31);
  for (int i = 0; i < 31; ++i) ASSERT_EQ(out31[i], (int)in31[i]);

  printf("  {year}-{mm}.bin round-trip ok\n");
  PASS();
}

void testMonthMissingFileZeroFills() {
  TempStorageDir tmp;
  TimeSeriesStore store(STATS_DIR);
  store.init();

  int out31[31];
  for (int i = 0; i < 31; ++i) out31[i] = 0xAAAA;
  store.readMonth(2025, 12, out31);
  for (int i = 0; i < 31; ++i) ASSERT_EQ(out31[i], 0);

  printf("  readMonth() zero-fills when %s/2025-12.bin is missing\n", STATS_DIR);
  PASS();
}

void testMonthAndYearAreIndependent() {
  TempStorageDir tmp;
  TimeSeriesStore store(STATS_DIR);
  store.init();

  int16_t year12[12];
  for (int i = 0; i < 12; ++i) year12[i] = static_cast<int16_t>(i + 1);
  store.writeYear(2025, year12);

  int16_t month31[31];
  for (int i = 0; i < 31; ++i) month31[i] = static_cast<int16_t>(i + 100);
  store.writeMonth(2025, 1, month31);

  int outYear[12] = {};
  store.readYear(2025, outYear);
  for (int i = 0; i < 12; ++i) ASSERT_EQ(outYear[i], (int)year12[i]);

  int outMonth[31] = {};
  store.readMonth(2025, 1, outMonth);
  for (int i = 0; i < 31; ++i) ASSERT_EQ(outMonth[i], (int)month31[i]);

  printf("  %s/2025.bin and %s/2025-01.bin do not collide\n", STATS_DIR, STATS_DIR);
  PASS();
}

int main() {
  printf("=== TimeSeriesStore Tests ===\n\n");

  testInitCreatesDir();
  testSummaryRoundTrip();
  testReadSummaryMissingReturnsFalse();
  testYearRoundTrip();
  testYearMissingFileZeroFills();
  testMonthRoundTrip();
  testMonthMissingFileZeroFills();
  testMonthAndYearAreIndependent();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
