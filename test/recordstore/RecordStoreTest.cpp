#include <ArduinoJson.h>
#include <HalStorage.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../datastore_common/TestHarness.h"
#include "RecordStore.h"

namespace {

struct TestRecord {
  std::string name;
  int value;
};

void serializeTestRecord(JsonDocument& doc, const void* data) {
  const auto& r = *static_cast<const TestRecord*>(data);
  doc["name"] = r.name;
  doc["value"] = r.value;
}

bool deserializeTestRecord(JsonDocument& doc, void* data) {
  auto& r = *static_cast<TestRecord*>(data);
  r.name = doc["name"] | "";
  r.value = doc["value"] | 0;
  return true;
}

}  // namespace

static constexpr const char* RECORDS_DIR = "/.myne/test-records";
static constexpr const char* SUMMARY_DIR = "/.myne/test-records-sum";
static constexpr size_t SUM_SIZE = 8;

void testInitCreatesDirs() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  ASSERT_FALSE(Storage.exists(RECORDS_DIR));
  ASSERT_FALSE(Storage.exists(SUMMARY_DIR));

  store.init();
  ASSERT_TRUE(Storage.exists(RECORDS_DIR));
  ASSERT_TRUE(Storage.exists(SUMMARY_DIR));

  printf("  init() created %s and %s\n", RECORDS_DIR, SUMMARY_DIR);
  PASS();
}

void testSaveLoadRoundTrip() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  ASSERT_FALSE(store.exists("rec1"));

  TestRecord in{"hello", 42};
  ASSERT_TRUE(store.save("rec1", serializeTestRecord, &in));
  ASSERT_TRUE(store.exists("rec1"));

  TestRecord out{};
  ASSERT_TRUE(store.load("rec1", deserializeTestRecord, &out));
  ASSERT_EQ(out.name, in.name);
  ASSERT_EQ(out.value, in.value);

  printf("  {id}.json round-trip ok\n");
  PASS();
}

void testLoadMissingReturnsFalse() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  TestRecord out{};
  ASSERT_FALSE(store.load("missing", deserializeTestRecord, &out));

  printf("  load() on missing record returns false\n");
  PASS();
}

void testRemove() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  TestRecord in{"to-delete", 1};
  ASSERT_TRUE(store.save("rec1", serializeTestRecord, &in));

  uint8_t sum[SUM_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8};
  ASSERT_TRUE(store.saveSummary("rec1", sum));

  ASSERT_TRUE(store.remove("rec1"));
  ASSERT_FALSE(store.exists("rec1"));

  char sumPath[96];
  store.sumPath(sumPath, sizeof(sumPath), "rec1");
  ASSERT_FALSE(Storage.exists(sumPath));

  printf("  remove() deletes {id}.json and {id}.bin\n");
  PASS();
}

void testRemoveMissingReturnsFalse() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  ASSERT_FALSE(store.remove("missing"));

  printf("  remove() on missing record returns false\n");
  PASS();
}

void testSaveLoadSummaryRoundTrip() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  uint8_t in[SUM_SIZE] = {10, 20, 30, 40, 50, 60, 70, 0xAA};
  ASSERT_TRUE(store.saveSummary("rec1", in));

  uint8_t out[SUM_SIZE] = {};
  ASSERT_TRUE(store.loadSummary("rec1", out));
  ASSERT_TRUE(memcmp(out, in, sizeof(in)) == 0);

  printf("  {id}.bin summary round-trip ok, including the last byte\n");
  PASS();
}

void testLoadSummaryMissingReturnsFalse() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  uint8_t out[SUM_SIZE] = {};
  ASSERT_FALSE(store.loadSummary("missing", out));

  printf("  loadSummary() on missing file returns false\n");
  PASS();
}

void testCount() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  ASSERT_EQ(store.count(), 0);

  TestRecord rec{"r", 0};
  ASSERT_TRUE(store.save("a", serializeTestRecord, &rec));
  ASSERT_TRUE(store.save("b", serializeTestRecord, &rec));
  ASSERT_TRUE(store.save("c", serializeTestRecord, &rec));
  ASSERT_EQ(store.count(), 3);

  ASSERT_TRUE(store.remove("b"));
  ASSERT_EQ(store.count(), 2);

  printf("  count() reflects saved/removed records\n");
  PASS();
}

void testForEach() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  TestRecord rec{"r", 0};
  ASSERT_TRUE(store.save("a", serializeTestRecord, &rec));
  ASSERT_TRUE(store.save("b", serializeTestRecord, &rec));
  ASSERT_TRUE(store.save("c", serializeTestRecord, &rec));

  std::vector<std::string> ids;
  store.forEach(&ids, [](void* ctx, const char* id) -> bool {
    static_cast<std::vector<std::string>*>(ctx)->emplace_back(id);
    return true;
  });

  std::sort(ids.begin(), ids.end());
  ASSERT_EQ(ids.size(), (size_t)3);
  ASSERT_EQ(ids[0], "a");
  ASSERT_EQ(ids[1], "b");
  ASSERT_EQ(ids[2], "c");

  printf("  forEach() visits all record ids\n");
  PASS();
}

void testForEachStopsEarly() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  TestRecord rec{"r", 0};
  ASSERT_TRUE(store.save("a", serializeTestRecord, &rec));
  ASSERT_TRUE(store.save("b", serializeTestRecord, &rec));
  ASSERT_TRUE(store.save("c", serializeTestRecord, &rec));

  int visited = 0;
  store.forEach(&visited, [](void* ctx, const char*) -> bool {
    (*static_cast<int*>(ctx))++;
    return false;  // stop after first
  });

  ASSERT_EQ(visited, 1);

  printf("  forEach() stops early when fn returns false\n");
  PASS();
}

void testForEachFile() {
  TempStorageDir tmp;
  RecordStore store(RECORDS_DIR, SUMMARY_DIR, SUM_SIZE);
  store.init();

  TestRecord rec{"hello", 7};
  ASSERT_TRUE(store.save("a", serializeTestRecord, &rec));
  ASSERT_TRUE(store.save("b", serializeTestRecord, &rec));

  struct Ctx {
    int count = 0;
    int unexpectedId = 0;
    size_t totalSize = 0;
  } ctx;

  store.forEachFile(&ctx, [](void* c, HalFile& file, const char* id) -> bool {
    auto* state = static_cast<Ctx*>(c);
    if (strcmp(id, "a") != 0 && strcmp(id, "b") != 0) state->unexpectedId++;
    state->count++;
    state->totalSize += file.size();
    return true;
  });

  ASSERT_EQ(ctx.count, 2);
  ASSERT_EQ(ctx.unexpectedId, 0);
  ASSERT_TRUE(ctx.totalSize > 0);

  printf("  forEachFile() visits all record files\n");
  PASS();
}

int main() {
  printf("=== RecordStore Tests ===\n\n");

  testInitCreatesDirs();
  testSaveLoadRoundTrip();
  testLoadMissingReturnsFalse();
  testRemove();
  testRemoveMissingReturnsFalse();
  testSaveLoadSummaryRoundTrip();
  testLoadSummaryMissingReturnsFalse();
  testCount();
  testForEach();
  testForEachStopsEarly();
  testForEachFile();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
