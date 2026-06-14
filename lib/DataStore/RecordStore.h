#pragma once
#include <ArduinoJson.h>
#include <HalStorage.h>

#include <cstddef>

// Generic per-record JSON store with optional binary summary cache.
//
// On disk:
//   {dir}/{id}.json       full record (JSON)
//   {sumDir}/{id}.bin     binary summary of fixed size (optional, fast read)
//
// Type erasure via function pointers — no templates, no virtual dispatch,
// no std::function. Each domain provides its own serialize/deserialize fns.
class RecordStore {
 public:
  constexpr RecordStore(const char* dir, const char* sumDir = nullptr, size_t sumSize = 0)
      : dir_(dir), sumDir_(sumDir), sumSize_(sumSize) {}

  // Ensure dir (and optional sumDir) exist on the SD card.
  void init() const;

  // Check whether {dir}/{id}.json exists.
  bool exists(const char* id) const;

  // Serialize data into a JsonDocument via serializeFn, then write to
  // {dir}/{id}.json.
  bool save(const char* id, void (*serializeFn)(JsonDocument&, const void*), const void* data) const;

  // Read {dir}/{id}.json, parse JSON, call deserializeFn to populate data.
  // Returns false on missing file or parse error.
  bool load(const char* id, bool (*deserializeFn)(JsonDocument&, void*), void* data) const;

  // Remove {dir}/{id}.json (and {sumDir}/{id}.bin if configured).
  bool remove(const char* id) const;

  // Write sumSize_ bytes from data into {sumDir}/{id}.bin.
  bool saveSummary(const char* id, const void* data) const;

  // Read sumSize_ bytes from {sumDir}/{id}.bin into data.
  // Returns false if file missing.
  bool loadSummary(const char* id, void* data) const;

  // Count .json files in dir_ without parsing content. O(N) scan, O(1) RAM.
  int count() const;

  // Iterate over all record IDs (filename without .json extension).
  // fn returns false to stop early.
  void forEach(void* ctx, bool (*fn)(void* ctx, const char* id)) const;

  // Open each {dir}/{id}.json file in turn and call fn with the live HalFile
  // and the id. Lets the caller reuse its own JsonDocument across iterations
  // to avoid repeated ArduinoJson arena alloc/free on large passes.
  void forEachFile(void* ctx, bool (*fn)(void* ctx, HalFile& file, const char* id)) const;

  void jsonPath(char* out, size_t size, const char* id) const;
  void sumPath(char* out, size_t size, const char* id) const;
  const char* dir() const { return dir_; }

 private:
  const char* dir_;
  const char* sumDir_;
  size_t sumSize_;
};
