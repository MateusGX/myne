#include "RecordStore.h"

#include <Logging.h>
#include <esp_task_wdt.h>

#include <cstring>

void RecordStore::jsonPath(char* out, size_t size, const char* id) const {
  snprintf(out, size, "%s/%s.json", dir_, id);
}

void RecordStore::sumPath(char* out, size_t size, const char* id) const {
  snprintf(out, size, "%s/%s.bin", sumDir_, id);
}

void RecordStore::init() const {
  Storage.mkdir(dir_);
  if (sumDir_) Storage.mkdir(sumDir_);
}

bool RecordStore::exists(const char* id) const {
  char path[96];
  jsonPath(path, sizeof(path), id);
  return Storage.exists(path);
}

bool RecordStore::save(const char* id, void (*serializeFn)(JsonDocument&, const void*), const void* data) const {
  char path[96];
  jsonPath(path, sizeof(path), id);
  HalFile file;
  if (!Storage.openFileForWrite("STORE", path, file)) {
    LOG_ERR("STORE", "Failed to open %s for write", path);
    return false;
  }
  JsonDocument doc;
  serializeFn(doc, data);
  serializeJson(doc, file);
  return true;
}

bool RecordStore::load(const char* id, bool (*deserializeFn)(JsonDocument&, void*), void* data) const {
  char path[96];
  jsonPath(path, sizeof(path), id);
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("STORE", path, file)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, file) != DeserializationError::Ok) {
    LOG_ERR("STORE", "Parse error: %s", path);
    return false;
  }
  return deserializeFn(doc, data);
}

bool RecordStore::remove(const char* id) const {
  char path[96];
  jsonPath(path, sizeof(path), id);
  if (!Storage.exists(path)) {
    LOG_ERR("STORE", "Not found: %s", id);
    return false;
  }
  Storage.remove(path);
  if (sumDir_ && sumSize_) {
    sumPath(path, sizeof(path), id);
    if (Storage.exists(path)) Storage.remove(path);
  }
  return true;
}

bool RecordStore::saveSummary(const char* id, const void* data) const {
  if (!sumDir_ || !sumSize_) return false;
  char path[96];
  sumPath(path, sizeof(path), id);
  HalFile file;
  if (!Storage.openFileForWrite("STORE", path, file)) {
    LOG_ERR("STORE", "Failed to open summary %s", path);
    return false;
  }
  file.write(static_cast<const uint8_t*>(data), sumSize_);
  return true;
}

bool RecordStore::loadSummary(const char* id, void* data) const {
  if (!sumDir_ || !sumSize_) return false;
  char path[96];
  sumPath(path, sizeof(path), id);
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("STORE", path, file)) return false;
  // Storage.readFileToBuffer() reads bufferSize-1 bytes (text/null-terminator
  // convention); read the raw fixed-size blob directly to avoid losing the
  // last byte.
  return file.read(data, sumSize_) == static_cast<int>(sumSize_);
}

int RecordStore::count() const {
  if (!Storage.exists(dir_)) return 0;
  HalFile dir = Storage.open(dir_);
  if (!dir || !dir.isDirectory()) return 0;
  int n = 0;
  while (true) {
    esp_task_wdt_reset();
    HalFile entry = dir.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) continue;
    char name[64];
    entry.getName(name, sizeof(name));
    const size_t nl = strlen(name);
    if (nl >= 5 && strcmp(name + nl - 5, ".json") == 0) ++n;
  }
  return n;
}

void RecordStore::forEach(void* ctx, bool (*fn)(void* ctx, const char* id)) const {
  if (!Storage.exists(dir_)) return;
  HalFile dir = Storage.open(dir_);
  if (!dir || !dir.isDirectory()) return;
  while (true) {
    esp_task_wdt_reset();
    HalFile entry = dir.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) continue;
    char name[64];
    entry.getName(name, sizeof(name));
    const size_t nl = strlen(name);
    if (nl < 6 || strcmp(name + nl - 5, ".json") != 0) continue;
    name[nl - 5] = '\0';  // strip .json → id
    if (!fn(ctx, name)) return;
  }
}

void RecordStore::forEachFile(void* ctx, bool (*fn)(void* ctx, HalFile& file, const char* id)) const {
  if (!Storage.exists(dir_)) return;
  HalFile dir = Storage.open(dir_);
  if (!dir || !dir.isDirectory()) return;
  while (true) {
    esp_task_wdt_reset();
    HalFile entry = dir.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) continue;
    char name[64];
    entry.getName(name, sizeof(name));
    const size_t nl = strlen(name);
    if (nl < 6 || strcmp(name + nl - 5, ".json") != 0) continue;
    name[nl - 5] = '\0';  // strip .json → id
    if (!fn(ctx, entry, name)) return;
  }
}
