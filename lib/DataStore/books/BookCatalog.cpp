#include "BookCatalog.h"

#include <ArduinoJson.h>
#include <BookStore.h>
#include <DataStoreUtils.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_task_wdt.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

// ── Types ─────────────────────────────────────────────────────────────────────

struct CollMeta {
  char id[9];     // 8-hex collection id + NUL
  char name[33];  // collection name, null-padded
  char note[65];  // collection note, null-padded
  int expectedCount = 0;
  int initialVolume = 0;
};

struct SortIdx {
  char key[33];    // sort key (title/name, lowercase-comparable)
  uint32_t start;  // byte offset of this line in the source file
  uint16_t len;    // byte length of the line including the trailing '\n'
};

// Max raw bytes (excluding NUL) for a collection name stored in REGISTRY_FILE's
// "n" field. This must match PhysicalBook::collection (an unbounded std::string)
// closely enough that the dashboard's name -> id lookup doesn't mismatch — the
// 32-byte limit used for on-device display titles (Entry::title[33]) is too
// small once multi-byte UTF-8 accents are involved (e.g. "Hirayasumi - Uma
// Pausa Relaxante Em Uma Casa Térrea" is 52 bytes, "20th Century Boys -
// Edição Definitiva" is 39 bytes). Worst case (every byte escaped):
// {"id":"...","n":"..."} + escaping = ~220 bytes, still under MAX_LINE (256).
constexpr size_t REG_NAME_MAX = 96;

// ── String helpers ────────────────────────────────────────────────────────────

static char upperFirstLetter(const char* s) {
  for (; *s; ++s) {
    if (*s == ' ') continue;
    const char c = static_cast<char>(toupper(static_cast<unsigned char>(*s)));
    return isalpha(static_cast<unsigned char>(c)) ? c : '#';
  }
  return '\0';
}

// Minimal JSON string escaping: only quotes and backslashes.
static void jsonEscapeStr(const char* src, char* dst, size_t maxDst) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 3 < maxDst; ++i) {
    if (src[i] == '"' || src[i] == '\\') dst[j++] = '\\';
    dst[j++] = src[i];
  }
  dst[j] = '\0';
}

// Extract the value of a JSON string field by name (e.g. "t" from {"t":"Foo"}).
// Simple scan — does not handle nested objects or escaped quote in the key name.
static void extractJsonStr(const char* json, const char* field, char* out, size_t maxOut) {
  char needle[16];
  snprintf(needle, sizeof(needle), "\"%s\":\"", field);
  const char* p = strstr(json, needle);
  if (!p) {
    out[0] = '\0';
    return;
  }
  p += strlen(needle);
  size_t i = 0;
  while (i < maxOut - 1 && *p && *p != '"') {
    if (*p == '\\' && *(p + 1)) ++p;  // skip one escaped char
    out[i++] = *p++;
  }
  out[i] = '\0';
}

static int extractJsonInt(const char* json, const char* field, int fallback = 0) {
  char needle[16];
  snprintf(needle, sizeof(needle), "\"%s\":", field);
  const char* p = strstr(json, needle);
  if (!p) return fallback;
  p += strlen(needle);
  while (*p == ' ') ++p;
  return static_cast<int>(strtol(p, nullptr, 10));
}

// Format a book as a compact NDJSON line (without trailing '\n').
static void formatBookLine(char* buf, size_t maxLen, const char* id, const char* title, const char* author,
                           const char* location, const char* volume = "") {
  char eid[36], et[70], ea[46], el[36], ev[36];
  jsonEscapeStr(id, eid, sizeof(eid));
  jsonEscapeStr(title, et, sizeof(et));
  jsonEscapeStr(author, ea, sizeof(ea));
  jsonEscapeStr(location, el, sizeof(el));
  jsonEscapeStr(volume ? volume : "", ev, sizeof(ev));
  snprintf(buf, maxLen, "{\"id\":\"%s\",\"t\":\"%s\",\"a\":\"%s\",\"l\":\"%s\",\"v\":\"%s\"}", eid, et, ea, el, ev);
}

// Format a collection header NDJSON line (without trailing '\n').
// note may be nullptr or empty to omit the "note" field.
static int formatCollectionHeader(char* buf, size_t maxLen, const char* collId, const char* name, int count,
                                  const char* note, int expectedCount = 0, int initialVolume = 0) {
  char eid[18], et[70], enote[130];
  jsonEscapeStr(collId, eid, sizeof(eid));
  jsonEscapeStr(name, et, sizeof(et));

  char meta[56] = {};
  if (expectedCount > 0) snprintf(meta + strlen(meta), sizeof(meta) - strlen(meta), ",\"e\":%d", expectedCount);
  if (initialVolume > 0) snprintf(meta + strlen(meta), sizeof(meta) - strlen(meta), ",\"iv\":%d", initialVolume);

  if (note && note[0]) {
    jsonEscapeStr(note, enote, sizeof(enote));
    return snprintf(buf, maxLen, "{\"id\":\"%s\",\"t\":\"%s\",\"c\":1,\"n\":%d%s,\"note\":\"%s\"}", eid, et, count,
                    meta, enote);
  }
  return snprintf(buf, maxLen, "{\"id\":\"%s\",\"t\":\"%s\",\"c\":1,\"n\":%d%s}", eid, et, count, meta);
}

static void formatRegistryLine(char* line, size_t maxLen, const char* collId, const char* name, int expectedCount,
                               int initialVolume) {
  char eid[18], en[REG_NAME_MAX * 2 + 4];
  jsonEscapeStr(collId, eid, sizeof(eid));
  jsonEscapeStr(name, en, sizeof(en));
  if (expectedCount > 0) {
    if (initialVolume > 0) {
      snprintf(line, maxLen, "{\"id\":\"%s\",\"n\":\"%s\",\"e\":%d,\"iv\":%d}", eid, en, expectedCount, initialVolume);
    } else {
      snprintf(line, maxLen, "{\"id\":\"%s\",\"n\":\"%s\",\"e\":%d}", eid, en, expectedCount);
    }
  } else {
    if (initialVolume > 0) {
      snprintf(line, maxLen, "{\"id\":\"%s\",\"n\":\"%s\",\"iv\":%d}", eid, en, initialVolume);
    } else {
      snprintf(line, maxLen, "{\"id\":\"%s\",\"n\":\"%s\"}", eid, en);
    }
  }
}

// Path of the raw-text note file for a collection: NOTES_DIR/{id8}.note
static void notePath(const char* collId, char* out, size_t outSize) {
  snprintf(out, outSize, "%s/%s.note", BookCatalog::NOTES_DIR, collId);
}

// ── IO helpers ────────────────────────────────────────────────────────────────

// Read the next non-empty line from f, stripping '\r'/'\n'.
// Returns false only when EOF is reached before any character is read.
static bool readLine(HalFile& f, char* buf, size_t maxLen) {
  while (true) {
    size_t i = 0;
    bool eof = false;
    while (i < maxLen - 1) {
      int c = f.read();
      if (c < 0) {
        eof = true;
        break;
      }
      if (c == '\r') continue;
      if (c == '\n') break;
      buf[i++] = static_cast<char>(c);
    }
    buf[i] = '\0';
    if (i > 0) return true;
    if (eof) return false;
    // empty line: skip and try again
  }
}

// Append a line (+ '\n') to an NDJSON file; creates the file if absent.
static void appendLine(const char* path, const char* line, bool* wasEmpty = nullptr) {
  HalFile f = Storage.open(path, O_RDWR | O_CREAT | O_AT_END);
  if (!f) return;
  if (wasEmpty) *wasEmpty = (f.fileSize() == 0);
  f.write(line, strlen(line));
  f.write("\n", 1);
}

// Count non-empty lines in a file.
static int countLines(const char* path) {
  HalFile f;
  if (!Storage.openFileForRead("CAT", path, f)) return 0;
  char buf[BookCatalog::MAX_LINE];
  int n = 0;
  while (readLine(f, buf, sizeof(buf))) ++n;
  return n;
}

// Delete every regular file (not subdirs) inside dirPath.
static void cleanFilesInDir(const char* dirPath) {
  HalFile dir = Storage.open(dirPath);
  if (!dir || !dir.isDirectory()) return;
  while (true) {
    char name[64];
    {
      HalFile f = dir.openNextFile();
      if (!f) break;
      if (f.isDirectory()) continue;
      f.getName(name, sizeof(name));
    }
    char path[120];
    snprintf(path, sizeof(path), "%s/%s", dirPath, name);
    Storage.remove(path);
  }
}

// Delete all files in TMP_DIR, then the directory itself.
static void cleanupTmpDir() {
  HalFile dir = Storage.open(BookCatalog::TMP_DIR);
  if (!dir || !dir.isDirectory()) return;
  while (true) {
    char name[64];
    {
      HalFile f = dir.openNextFile();
      if (!f) break;
      f.getName(name, sizeof(name));
    }
    char path[100];
    snprintf(path, sizeof(path), "%s/%s", BookCatalog::TMP_DIR, name);
    Storage.remove(path);
  }
  Storage.rmdir(BookCatalog::TMP_DIR);
}

// ── Collection-id registry ───────────────────────────────────────────────────
// REGISTRY_FILE persists name<->id mappings outside CATALOG_DIR so collection
// identity survives rebuild()/rename (see BookCatalog.h for the file format).

// Returns true if `id` (8-hex) already appears as an "id" field in REGISTRY_FILE.
static bool registryHasId(const char* id) {
  if (!Storage.exists(BookCatalog::REGISTRY_FILE)) return false;
  HalFile f;
  if (!Storage.openFileForRead("CAT", BookCatalog::REGISTRY_FILE, f)) return false;
  char buf[BookCatalog::MAX_LINE];
  while (readLine(f, buf, sizeof(buf))) {
    char lineId[9] = {};
    extractJsonStr(buf, "id", lineId, sizeof(lineId));
    if (strcmp(lineId, id) == 0) return true;
  }
  return false;
}

// Generate a fresh 8-hex id that doesn't collide with any "id" field already
// in REGISTRY_FILE, and append {"id":"...","n":"..."} for `key` to it.
static void registerNewCollection(const char* key, char* outId) {
  for (int attempt = 0; attempt < 5; ++attempt) {
    DataStoreUtils::generateHexId(outId, 9);
    if (!registryHasId(outId)) break;
  }

  char line[BookCatalog::MAX_LINE];
  formatRegistryLine(line, sizeof(line), outId, key, 0, 0);
  Storage.mkdir("/.myne");
  appendLine(BookCatalog::REGISTRY_FILE, line);
}

// Rewrite the "n" (name) field of the registry entry whose "id" equals collId.
// Returns true if an entry was found and rewritten.
static bool updateRegistryName(const char* collId, const char* newName) {
  if (!Storage.exists(BookCatalog::REGISTRY_FILE)) return false;

  char key[REG_NAME_MAX + 1] = {};
  strncpy(key, newName, REG_NAME_MAX);
  key[REG_NAME_MAX] = '\0';

  char tmpPath[120];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", BookCatalog::REGISTRY_FILE);

  bool found = false;
  {
    HalFile dst;
    if (!Storage.openFileForWrite("CAT", tmpPath, dst)) return false;
    HalFile src;
    if (Storage.openFileForRead("CAT", BookCatalog::REGISTRY_FILE, src)) {
      char buf[BookCatalog::MAX_LINE];
      while (readLine(src, buf, sizeof(buf))) {
        char lineId[9] = {};
        extractJsonStr(buf, "id", lineId, sizeof(lineId));
        if (!found && strcmp(lineId, collId) == 0) {
          found = true;
          const int expectedCount = extractJsonInt(buf, "e", 0);
          const int initialVolume = extractJsonInt(buf, "iv", 0);
          char line[BookCatalog::MAX_LINE];
          formatRegistryLine(line, sizeof(line), collId, key, expectedCount, initialVolume);
          dst.write(line, strlen(line));
          dst.write("\n", 1);
          continue;
        }
        dst.write(buf, strlen(buf));
        dst.write("\n", 1);
      }
    }
  }  // src and dst close here

  if (found) {
    Storage.remove(BookCatalog::REGISTRY_FILE);
    Storage.rename(tmpPath, BookCatalog::REGISTRY_FILE);
  } else {
    Storage.remove(tmpPath);
  }
  return found;
}

static bool updateRegistryExpectedCount(const char* collId, int expectedCount) {
  if (!Storage.exists(BookCatalog::REGISTRY_FILE)) return false;
  if (expectedCount < 0) expectedCount = 0;

  char tmpPath[120];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", BookCatalog::REGISTRY_FILE);

  bool found = false;
  {
    HalFile dst;
    if (!Storage.openFileForWrite("CAT", tmpPath, dst)) return false;
    HalFile src;
    if (Storage.openFileForRead("CAT", BookCatalog::REGISTRY_FILE, src)) {
      char buf[BookCatalog::MAX_LINE];
      while (readLine(src, buf, sizeof(buf))) {
        char lineId[9] = {};
        extractJsonStr(buf, "id", lineId, sizeof(lineId));
        if (!found && strcmp(lineId, collId) == 0) {
          found = true;
          char name[REG_NAME_MAX + 1] = {};
          extractJsonStr(buf, "n", name, sizeof(name));
          const int initialVolume = extractJsonInt(buf, "iv", 0);
          char line[BookCatalog::MAX_LINE];
          formatRegistryLine(line, sizeof(line), collId, name, expectedCount, initialVolume);
          dst.write(line, strlen(line));
          dst.write("\n", 1);
          continue;
        }
        dst.write(buf, strlen(buf));
        dst.write("\n", 1);
      }
    }
  }  // src and dst close here

  if (found) {
    Storage.remove(BookCatalog::REGISTRY_FILE);
    Storage.rename(tmpPath, BookCatalog::REGISTRY_FILE);
  } else {
    Storage.remove(tmpPath);
  }
  return found;
}

static bool updateRegistryInitialVolume(const char* collId, int initialVolume) {
  if (!Storage.exists(BookCatalog::REGISTRY_FILE)) return false;
  if (initialVolume < 0) initialVolume = 0;

  char tmpPath[120];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", BookCatalog::REGISTRY_FILE);

  bool found = false;
  {
    HalFile dst;
    if (!Storage.openFileForWrite("CAT", tmpPath, dst)) return false;
    HalFile src;
    if (Storage.openFileForRead("CAT", BookCatalog::REGISTRY_FILE, src)) {
      char buf[BookCatalog::MAX_LINE];
      while (readLine(src, buf, sizeof(buf))) {
        char lineId[9] = {};
        extractJsonStr(buf, "id", lineId, sizeof(lineId));
        if (!found && strcmp(lineId, collId) == 0) {
          found = true;
          char name[REG_NAME_MAX + 1] = {};
          extractJsonStr(buf, "n", name, sizeof(name));
          const int expectedCount = extractJsonInt(buf, "e", 0);
          char line[BookCatalog::MAX_LINE];
          formatRegistryLine(line, sizeof(line), collId, name, expectedCount, initialVolume);
          dst.write(line, strlen(line));
          dst.write("\n", 1);
          continue;
        }
        dst.write(buf, strlen(buf));
        dst.write("\n", 1);
      }
    }
  }  // src and dst close here

  if (found) {
    Storage.remove(BookCatalog::REGISTRY_FILE);
    Storage.rename(tmpPath, BookCatalog::REGISTRY_FILE);
  } else {
    Storage.remove(tmpPath);
  }
  return found;
}

// In-memory registry cache used by rebuild() Phase 1 to avoid a full
// REGISTRY_FILE scan per book (see BookCatalog.h header comment).
struct RegEntry {
  char id[9];
  char name[REG_NAME_MAX + 1];
};
// Capped so the cache stays ~25KB (same budget as before REG_NAME_MAX grew
// from 32 to 96) at sizeof(RegEntry)=106.
constexpr int MAX_CACHED_COLLECTIONS = 240;

// Like BookCatalog::resolveCollectionId, but resolves against an in-memory
// cache of (up to MAX_CACHED_COLLECTIONS) registry entries first. On a cache
// miss with a full cache, falls back to the file-scan resolveCollectionId.
static bool resolveCollectionIdCached(RegEntry* cache, int* count, const char* name, char* outId) {
  char key[REG_NAME_MAX + 1] = {};
  strncpy(key, name, REG_NAME_MAX);
  key[REG_NAME_MAX] = '\0';

  for (int i = 0; i < *count; ++i) {
    if (strcmp(cache[i].name, key) == 0) {
      strncpy(outId, cache[i].id, 9);
      return true;
    }
  }

  if (*count >= MAX_CACHED_COLLECTIONS) {
    return BookCatalog::resolveCollectionId(key, outId);
  }

  registerNewCollection(key, outId);
  strncpy(cache[*count].id, outId, sizeof(cache[*count].id));
  strncpy(cache[*count].name, key, sizeof(cache[*count].name));
  ++(*count);
  return true;
}

bool BookCatalog::resolveCollectionId(const char* name, char* outId) {
  char key[REG_NAME_MAX + 1] = {};
  strncpy(key, name, REG_NAME_MAX);
  key[REG_NAME_MAX] = '\0';

  if (Storage.exists(REGISTRY_FILE)) {
    HalFile f;
    if (Storage.openFileForRead("CAT", REGISTRY_FILE, f)) {
      char buf[MAX_LINE];
      while (readLine(f, buf, sizeof(buf))) {
        char lineName[REG_NAME_MAX + 1] = {};
        extractJsonStr(buf, "n", lineName, sizeof(lineName));
        if (strcmp(lineName, key) == 0) {
          extractJsonStr(buf, "id", outId, 9);
          if (outId[0]) return true;
        }
      }
    }
  }

  registerNewCollection(key, outId);
  return true;
}

// ── Sort ──────────────────────────────────────────────────────────────────────

static int cmpSortIdx(const void* a, const void* b) {
  return strcasecmp(static_cast<const SortIdx*>(a)->key, static_cast<const SortIdx*>(b)->key);
}

static int cmpCollMetaByName(const void* a, const void* b) {
  return strcasecmp(static_cast<const CollMeta*>(a)->name, static_cast<const CollMeta*>(b)->name);
}

// Sort a NDJSON file by its "t" (title) field.
// Uses a two-phase approach: build a compact SortIdx array (39 bytes/entry),
// then copy lines in sorted order to a temp file and rename.
// Memory: nLines × 39 bytes for the index; one MAX_LINE copy buffer.
static void sortNdjsonByTitle(const char* path) {
  // Phase A: count lines
  int nLines = 0;
  {
    HalFile f;
    char buf[BookCatalog::MAX_LINE];
    if (Storage.openFileForRead("CAT", path, f))
      while (readLine(f, buf, sizeof(buf))) ++nLines;
  }
  if (nLines < 2) return;

  // Phase B: build sort index with a single linear scan
  auto* idx = static_cast<SortIdx*>(malloc(nLines * sizeof(SortIdx)));
  if (!idx) {
    LOG_ERR("CAT", "sortNdjson: malloc %u failed", (unsigned)(nLines * sizeof(SortIdx)));
    return;
  }

  int n = 0;
  {
    HalFile f;
    char buf[BookCatalog::MAX_LINE];
    if (Storage.openFileForRead("CAT", path, f)) {
      while (n < nLines) {
        const auto lineStart = static_cast<uint32_t>(f.position());
        size_t bi = 0;
        bool eof = false;
        while (bi < sizeof(buf) - 1) {
          int c = f.read();
          if (c < 0) {
            eof = true;
            break;
          }
          if (c == '\r') continue;
          if (c == '\n') break;
          buf[bi++] = static_cast<char>(c);
        }
        buf[bi] = '\0';
        const auto lineEnd = static_cast<uint32_t>(f.position());
        if (bi > 0) {
          idx[n].start = lineStart;
          idx[n].len = static_cast<uint16_t>(lineEnd - lineStart);
          extractJsonStr(buf, "t", idx[n].key, sizeof(idx[n].key));
          ++n;
        }
        if (eof) break;
      }
    }
  }

  // Phase C: sort
  qsort(idx, n, sizeof(SortIdx), cmpSortIdx);

  // Phase D: write sorted lines to a temp file, one line at a time
  char tmpPath[120];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
  {
    HalFile src, dst;
    char buf[BookCatalog::MAX_LINE + 2];
    if (Storage.openFileForRead("CAT", path, src) && Storage.openFileForWrite("CAT", tmpPath, dst)) {
      for (int i = 0; i < n; ++i) {
        src.seekSet(idx[i].start);
        int got = src.read(buf, idx[i].len);
        if (got > 0) {
          // Normalise line ending to '\n'
          while (got > 0 && (buf[got - 1] == '\r' || buf[got - 1] == '\n')) --got;
          buf[got++] = '\n';
          dst.write(buf, static_cast<size_t>(got));
        }
      }
    }
  }  // src and dst close here

  free(idx);

  // Phase E: atomic replace
  Storage.remove(path);
  Storage.rename(tmpPath, path);
}

// ── Build phase 3: per-letter catalog file ────────────────────────────────────

static void buildLetterFile(char letter, uint16_t& idxOut) {
  idxOut = 0;

  // ─── Collect collection metas that belong to this letter ───────────────
  int nColls = 0;
  if (Storage.exists(BookCatalog::COLL_META_FILE)) {
    HalFile mf;
    char buf[BookCatalog::MAX_LINE];
    if (Storage.openFileForRead("CAT", BookCatalog::COLL_META_FILE, mf)) {
      while (readLine(mf, buf, sizeof(buf))) {
        char x[4] = {};
        extractJsonStr(buf, "x", x, sizeof(x));
        if (x[0] == letter) ++nColls;
      }
    }
  }

  CollMeta* colls = nullptr;
  int collCount = 0;

  if (nColls > 0) {
    colls = static_cast<CollMeta*>(malloc(nColls * sizeof(CollMeta)));
    if (colls) {
      HalFile mf;
      char buf[BookCatalog::MAX_LINE];
      if (Storage.openFileForRead("CAT", BookCatalog::COLL_META_FILE, mf)) {
        while (collCount < nColls && readLine(mf, buf, sizeof(buf))) {
          char x[4] = {};
          extractJsonStr(buf, "x", x, sizeof(x));
          if (x[0] != letter) continue;
          extractJsonStr(buf, "id", colls[collCount].id, sizeof(colls[collCount].id));
          extractJsonStr(buf, "t", colls[collCount].name, sizeof(colls[collCount].name));
          colls[collCount].expectedCount = extractJsonInt(buf, "e", 0);
          colls[collCount].initialVolume = extractJsonInt(buf, "iv", 0);
          // Load persistent collection note (outside catalog dir, survives rebuild)
          BookCatalog::getCollectionNote(colls[collCount].id, colls[collCount].note, sizeof(colls[collCount].note));
          ++collCount;
        }
      }
      if (collCount > 1) qsort(colls, collCount, sizeof(CollMeta), cmpCollMetaByName);
    }
  }

  // ─── Check standalone books for this letter ────────────────────────────
  char tmpPath[80];
  snprintf(tmpPath, sizeof(tmpPath), "%s/%c.ndjson", BookCatalog::TMP_DIR, letter);
  const bool hasStandalone = Storage.exists(tmpPath);
  const int nStandalone = hasStandalone ? countLines(tmpPath) : 0;

  const int total = collCount + nStandalone;
  if (total == 0) {
    free(colls);
    return;
  }

  idxOut = static_cast<uint16_t>(std::min(total, 65535));

  // ─── Write catalog/{letter}.ndjson ─────────────────────────────────────
  char letterPath[80];
  snprintf(letterPath, sizeof(letterPath), "%s/%c.ndjson", BookCatalog::CATALOG_DIR, letter);
  HalFile lf;
  if (!Storage.openFileForWrite("CAT", letterPath, lf)) {
    free(colls);
    return;
  }

  // Write collection headers (sorted by name)
  char buf[BookCatalog::MAX_LINE];
  for (int i = 0; i < collCount; ++i) {
    char collPath[80];
    snprintf(collPath, sizeof(collPath), "%s/%s.ndjson", BookCatalog::COLL_DIR, colls[i].id);
    const int cnt = countLines(collPath);

    const int len = formatCollectionHeader(buf, sizeof(buf), colls[i].id, colls[i].name, cnt, colls[i].note,
                                           colls[i].expectedCount, colls[i].initialVolume);
    if (len > 0) {
      lf.write(buf, static_cast<size_t>(len));
      lf.write("\n", 1);
    }
  }
  free(colls);
  colls = nullptr;

  // Write standalone books (already sorted in tmp file)
  if (hasStandalone) {
    HalFile sf;
    if (Storage.openFileForRead("CAT", tmpPath, sf)) {
      while (readLine(sf, buf, sizeof(buf))) {
        const size_t len = strlen(buf);
        lf.write(buf, len);
        lf.write("\n", 1);
      }
    }
  }
}

// ── Incremental update helpers ─────────────────────────────────────────────────

// Remove the line whose "id" field equals id. Returns true if a line was removed.
static bool removeLineById(const char* path, const char* id) {
  if (!Storage.exists(path)) return false;

  char tmpPath[120];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

  bool removed = false;
  {
    HalFile dst;
    if (!Storage.openFileForWrite("CAT", tmpPath, dst)) return false;
    HalFile src;
    if (Storage.openFileForRead("CAT", path, src)) {
      char buf[BookCatalog::MAX_LINE];
      while (readLine(src, buf, sizeof(buf))) {
        char lineId[17] = {};
        extractJsonStr(buf, "id", lineId, sizeof(lineId));
        if (!removed && strcmp(lineId, id) == 0) {
          removed = true;
          continue;  // drop this line
        }
        dst.write(buf, strlen(buf));
        dst.write("\n", 1);
      }
    }
  }  // src and dst close here

  if (removed) {
    Storage.remove(path);
    Storage.rename(tmpPath, path);
  } else {
    Storage.remove(tmpPath);
  }
  return removed;
}

// Section a new line should be inserted into within a per-letter NDJSON file:
// collection headers ("c":1, sorted by name) come first, then standalone
// books (sorted by title).
enum class InsertSection { Header, Book };

static bool isHeaderLine(const char* line) { return strstr(line, "\"c\":1") != nullptr; }

// Insert newLine into path at the position that keeps it sorted (case-insensitive)
// by "t" within its section, creating the file if it doesn't exist.
static void insertLineSorted(const char* path, const char* newLine, const char* newKey, InsertSection section) {
  char tmpPath[120];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

  {
    HalFile dst;
    if (!Storage.openFileForWrite("CAT", tmpPath, dst)) return;

    bool inserted = false;
    HalFile src;
    if (Storage.openFileForRead("CAT", path, src)) {
      char buf[BookCatalog::MAX_LINE];
      while (readLine(src, buf, sizeof(buf))) {
        const bool lineIsHeader = isHeaderLine(buf);

        if (!inserted) {
          if (section == InsertSection::Header && !lineIsHeader) {
            // Reached the books section without finding a later header: insert here.
            dst.write(newLine, strlen(newLine));
            dst.write("\n", 1);
            inserted = true;
          } else if ((section == InsertSection::Header && lineIsHeader) ||
                     (section == InsertSection::Book && !lineIsHeader)) {
            char key[33] = {};
            extractJsonStr(buf, "t", key, sizeof(key));
            if (strcasecmp(newKey, key) <= 0) {
              dst.write(newLine, strlen(newLine));
              dst.write("\n", 1);
              inserted = true;
            }
          }
          // else: section == Book && lineIsHeader -> still in header block, keep scanning
        }

        dst.write(buf, strlen(buf));
        dst.write("\n", 1);
      }
    }

    if (!inserted) {
      dst.write(newLine, strlen(newLine));
      dst.write("\n", 1);
    }
  }  // src and dst close here

  Storage.remove(path);
  Storage.rename(tmpPath, path);
}

// Rewrite the "n" (book count) field of the collection header identified by
// collId within letterPath. Returns true if the header was found.
static bool updateCollectionHeaderCount(const char* letterPath, const char* collId, int newCount,
                                        int newExpectedCount = -1, int newInitialVolume = -1) {
  if (!Storage.exists(letterPath)) return false;

  char tmpPath[120];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", letterPath);

  bool found = false;
  {
    HalFile dst;
    if (!Storage.openFileForWrite("CAT", tmpPath, dst)) return false;
    HalFile src;
    if (Storage.openFileForRead("CAT", letterPath, src)) {
      char buf[BookCatalog::MAX_LINE];
      while (readLine(src, buf, sizeof(buf))) {
        char lineId[9] = {};
        extractJsonStr(buf, "id", lineId, sizeof(lineId));
        if (!found && isHeaderLine(buf) && strcmp(lineId, collId) == 0) {
          found = true;
          char title[33] = {}, note[65] = {};
          extractJsonStr(buf, "t", title, sizeof(title));
          extractJsonStr(buf, "note", note, sizeof(note));
          const int expectedCount = newExpectedCount >= 0 ? newExpectedCount : extractJsonInt(buf, "e", 0);
          const int initialVolume = newInitialVolume >= 0 ? newInitialVolume : extractJsonInt(buf, "iv", 0);
          char line[BookCatalog::MAX_LINE];
          const int len =
              formatCollectionHeader(line, sizeof(line), collId, title, newCount, note, expectedCount, initialVolume);
          if (len > 0) {
            dst.write(line, static_cast<size_t>(len));
            dst.write("\n", 1);
          }
          continue;
        }
        dst.write(buf, strlen(buf));
        dst.write("\n", 1);
      }
    }
  }  // src and dst close here

  if (found) {
    Storage.remove(letterPath);
    Storage.rename(tmpPath, letterPath);
  } else {
    Storage.remove(tmpPath);
  }
  return found;
}

// Add 'delta' to the per-letter entry count stored in idx.bin.
static void adjustIdxCount(char letter, int delta) {
  if (delta == 0) return;
  int li;
  if (letter == '#') {
    li = 26;
  } else {
    li = toupper(static_cast<unsigned char>(letter)) - 'A';
    if (li < 0 || li >= 26) return;
  }

  HalFile f = Storage.open(BookCatalog::IDX_FILE, O_RDWR);
  if (!f) return;
  f.seekSet(static_cast<size_t>(li) * sizeof(uint16_t));
  uint16_t count = 0;
  f.read(&count, sizeof(count));
  int newCount = static_cast<int>(count) + delta;
  if (newCount < 0) newCount = 0;
  if (newCount > 65535) newCount = 65535;
  const auto newCountU16 = static_cast<uint16_t>(newCount);
  f.seekSet(static_cast<size_t>(li) * sizeof(uint16_t));
  f.write(&newCountU16, sizeof(newCountU16));
}

// Remove book's catalog entry, updating its collection header / idx.bin as needed.
static void removeEntry(const BookCatalog::BookChangeInfo& book) {
  if (book.collection[0]) {
    char collId[9];
    BookCatalog::resolveCollectionId(book.collection, collId);

    char collPath[80];
    snprintf(collPath, sizeof(collPath), "%s/%s.ndjson", BookCatalog::COLL_DIR, collId);
    if (!removeLineById(collPath, book.id)) return;

    const int newCount = countLines(collPath);
    char letter = upperFirstLetter(book.collection);
    if (!letter) letter = '#';
    char letterPath[80];
    snprintf(letterPath, sizeof(letterPath), "%s/%c.ndjson", BookCatalog::CATALOG_DIR, letter);

    if (newCount > 0) {
      updateCollectionHeaderCount(letterPath, collId, newCount);
    } else {
      // Collection emptied: drop its file, note, header, and registry entry entirely.
      Storage.remove(collPath);
      char notePathBuf[80];
      notePath(collId, notePathBuf, sizeof(notePathBuf));
      Storage.remove(notePathBuf);
      if (removeLineById(letterPath, collId)) adjustIdxCount(letter, -1);
      removeLineById(BookCatalog::REGISTRY_FILE, collId);
    }
  } else {
    const char letter = upperFirstLetter(book.title);
    if (!letter) return;
    char letterPath[80];
    snprintf(letterPath, sizeof(letterPath), "%s/%c.ndjson", BookCatalog::CATALOG_DIR, letter);
    if (removeLineById(letterPath, book.id)) adjustIdxCount(letter, -1);
  }
}

// Add book's catalog entry, creating its collection header if needed and
// updating idx.bin as needed.
static void addEntry(const BookCatalog::BookChangeInfo& book) {
  char lineBuf[BookCatalog::MAX_LINE];

  if (book.collection[0]) {
    char collId[9];
    BookCatalog::resolveCollectionId(book.collection, collId);

    char collPath[80];
    snprintf(collPath, sizeof(collPath), "%s/%s.ndjson", BookCatalog::COLL_DIR, collId);
    const bool isNewCollection = !Storage.exists(collPath);

    formatBookLine(lineBuf, sizeof(lineBuf), book.id, book.title, book.author, book.location, book.volume);
    insertLineSorted(collPath, lineBuf, book.title, InsertSection::Book);
    const int newCount = countLines(collPath);

    char letter = upperFirstLetter(book.collection);
    if (!letter) letter = '#';
    char letterPath[80];
    snprintf(letterPath, sizeof(letterPath), "%s/%c.ndjson", BookCatalog::CATALOG_DIR, letter);

    if (isNewCollection) {
      char note[65] = {};
      BookCatalog::getCollectionNote(collId, note, sizeof(note));
      const int expectedCount = BookCatalog::getCollectionExpectedCount(collId);
      const int initialVolume = BookCatalog::getCollectionInitialVolume(collId);
      char headerLine[BookCatalog::MAX_LINE];
      formatCollectionHeader(headerLine, sizeof(headerLine), collId, book.collection, newCount, note, expectedCount,
                             initialVolume);
      insertLineSorted(letterPath, headerLine, book.collection, InsertSection::Header);
      adjustIdxCount(letter, +1);
    } else {
      updateCollectionHeaderCount(letterPath, collId, newCount);
    }
  } else {
    const char letter = upperFirstLetter(book.title);
    if (!letter) return;

    formatBookLine(lineBuf, sizeof(lineBuf), book.id, book.title, book.author, book.location, book.volume);
    char letterPath[80];
    snprintf(letterPath, sizeof(letterPath), "%s/%c.ndjson", BookCatalog::CATALOG_DIR, letter);
    insertLineSorted(letterPath, lineBuf, book.title, InsertSection::Book);
    adjustIdxCount(letter, +1);
  }
}

// ── BookCatalog::rebuild ──────────────────────────────────────────────────────

bool BookCatalog::rebuild(const char* booksDir, void (*onProgress)(int processed, void* ctx), void* ctx) {
  // ─── Clean and recreate catalog directories ─────────────────────────────
  cleanFilesInDir(COLL_DIR);     // remove stale collection NDJSON files
  cleanFilesInDir(CATALOG_DIR);  // remove stale letter files and old idx.bin
  cleanFilesInDir(TMP_DIR);      // remove any leftover tmp files

  Storage.mkdir(CATALOG_DIR);
  Storage.mkdir(COLL_DIR);
  Storage.mkdir(TMP_DIR);
  Storage.mkdir(NOTES_DIR);  // never cleaned: persistent collection notes

  int processed = 0;

  // ─── Load registry cache for Phase 1 (avoids a REGISTRY_FILE scan per book) ──
  // Bounded ~25KB allocation, freed before Phase 2. If malloc fails or the
  // registry exceeds the cap, resolveCollectionIdCached falls back to the
  // file-scan resolveCollectionId — correct, just slower.
  auto* regCache = static_cast<RegEntry*>(malloc(MAX_CACHED_COLLECTIONS * sizeof(RegEntry)));
  int regCount = 0;
  if (regCache) {
    HalFile f;
    char buf[MAX_LINE];
    if (Storage.openFileForRead("CAT", REGISTRY_FILE, f)) {
      while (regCount < MAX_CACHED_COLLECTIONS && readLine(f, buf, sizeof(buf))) {
        extractJsonStr(buf, "id", regCache[regCount].id, sizeof(regCache[regCount].id));
        extractJsonStr(buf, "n", regCache[regCount].name, sizeof(regCache[regCount].name));
        ++regCount;
      }
    }
  } else {
    LOG_ERR("CAT", "rebuild: registry cache malloc failed (%u bytes)",
            (unsigned)(MAX_CACHED_COLLECTIONS * sizeof(RegEntry)));
    regCount = MAX_CACHED_COLLECTIONS;  // force fallback to resolveCollectionId
  }

  // ─── Phase 1: stream every book JSON, one at a time ─────────────────────
  // Memory: one JsonDocument (reused), two small stack buffers — O(1).
  {
    JsonDocument doc;
    char lineBuf[MAX_LINE];

    HalFile dir = Storage.open(booksDir);
    if (!dir || !dir.isDirectory()) {
      LOG_ERR("CAT", "Cannot open books dir: %s", booksDir);
      return false;
    }

    while (true) {
      esp_task_wdt_reset();

      char name[64];
      {
        HalFile entry = dir.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) continue;

        entry.getName(name, sizeof(name));
        const size_t nl = strlen(name);
        if (nl < 6 || strcmp(name + nl - 5, ".json") != 0) continue;

        char id[17] = {};
        memcpy(id, name, std::min(nl - 5, (size_t)16));

        doc.clear();
        if (deserializeJson(doc, entry) != DeserializationError::Ok) continue;

        const char* title = doc["t"] | "";
        const char* author = doc["a"] | "";
        const char* collection = doc["c"] | "";
        const char* location = doc["l"] | "";
        const char* volume = doc["v"] | "";

        if (!title[0] && !id[0]) continue;

        if (collection[0]) {
          char collId[9];
          resolveCollectionIdCached(regCache, &regCount, collection, collId);

          char collPath[80];
          snprintf(collPath, sizeof(collPath), "%s/%s.ndjson", COLL_DIR, collId);

          bool isNew = false;
          formatBookLine(lineBuf, sizeof(lineBuf), id, title, author, location, volume);
          appendLine(collPath, lineBuf, &isNew);

          if (isNew) {
            // Record this collection's metadata for phase 3
            const char letter = upperFirstLetter(collection);
            char eid[18], et[70];
            jsonEscapeStr(collId, eid, sizeof(eid));
            jsonEscapeStr(collection, et, sizeof(et));
            const int expectedCount = BookCatalog::getCollectionExpectedCount(collId);
            const int initialVolume = BookCatalog::getCollectionInitialVolume(collId);
            char meta[64] = {};
            if (expectedCount > 0)
              snprintf(meta + strlen(meta), sizeof(meta) - strlen(meta), ",\"e\":%d", expectedCount);
            if (initialVolume > 0)
              snprintf(meta + strlen(meta), sizeof(meta) - strlen(meta), ",\"iv\":%d", initialVolume);
            snprintf(lineBuf, sizeof(lineBuf), "{\"id\":\"%s\",\"t\":\"%s\",\"x\":\"%c\"%s}", eid, et,
                     letter ? letter : '?', meta);
            appendLine(COLL_META_FILE, lineBuf);
          }
        } else {
          const char letter = upperFirstLetter(title);
          if (!letter) continue;

          char tmpPath[80];
          snprintf(tmpPath, sizeof(tmpPath), "%s/%c.ndjson", TMP_DIR, letter);
          formatBookLine(lineBuf, sizeof(lineBuf), id, title, author, location, volume);
          appendLine(tmpPath, lineBuf);
        }
        ++processed;
        if (onProgress) onProgress(processed, ctx);
        yield();
      }
    }
    LOG_INF("CAT", "Phase 1: %d books scanned", processed);
  }

  free(regCache);
  regCache = nullptr;

  // ─── Phase 2: sort each collection NDJSON file by title ─────────────────
  // Memory: O(collection_size) — one collection at a time.
  {
    HalFile cd = Storage.open(COLL_DIR);
    if (cd && cd.isDirectory()) {
      while (true) {
        esp_task_wdt_reset();
        char name[32];
        {
          HalFile cf = cd.openNextFile();
          if (!cf) break;
          if (cf.isDirectory()) continue;
          cf.getName(name, sizeof(name));
        }
        char path[80];
        snprintf(path, sizeof(path), "%s/%s", COLL_DIR, name);
        sortNdjsonByTitle(path);
      }
    }
  }

  // ─── Phase 2.5: sort each per-letter tmp file by title ──────────────────
  // Memory: O(letter_size) — one letter at a time.
  {
    HalFile td = Storage.open(TMP_DIR);
    if (td && td.isDirectory()) {
      while (true) {
        esp_task_wdt_reset();
        char name[32];
        {
          HalFile tf = td.openNextFile();
          if (!tf) break;
          if (tf.isDirectory()) continue;
          tf.getName(name, sizeof(name));
        }
        if (strcmp(name, "colls.ndjson") == 0) continue;  // skip COLL_META_FILE
        char path[80];
        snprintf(path, sizeof(path), "%s/%s", TMP_DIR, name);
        sortNdjsonByTitle(path);
      }
    }
  }

  // ─── Phase 3: build per-letter catalog files ─────────────────────────────
  // Memory: O(letter_size) — one letter at a time.
  uint16_t idxBuf[27] = {};
  for (int li = 0; li <= 26; ++li) {
    esp_task_wdt_reset();
    const char letter = (li < 26) ? static_cast<char>('A' + li) : '#';
    buildLetterFile(letter, idxBuf[li]);
  }

  {
    HalFile idxFile;
    if (Storage.openFileForWrite("CAT", IDX_FILE, idxFile)) idxFile.write(idxBuf, sizeof(idxBuf));
  }

  cleanupTmpDir();

  LOG_INF("CAT", "Catalog rebuilt: %d books", processed);
  return true;
}

// ── Read helpers ──────────────────────────────────────────────────────────────

// Read up to maxCount NDJSON entries from 'path' starting at line index 'start'.
static int readNdjsonPage(const char* path, int start, int maxCount, BookCatalog::Entry* out) {
  if (!Storage.exists(path)) return 0;
  HalFile f;
  if (!Storage.openFileForRead("CAT", path, f)) return 0;

  // Skip 'start' lines
  char buf[BookCatalog::MAX_LINE];
  for (int i = 0; i < start; ++i) {
    esp_task_wdt_reset();
    if ((i & 0x0F) == 0x0F) yield();
    if (!readLine(f, buf, sizeof(buf))) return 0;
  }

  JsonDocument doc;
  int n = 0;
  while (n < maxCount && readLine(f, buf, sizeof(buf))) {
    esp_task_wdt_reset();
    doc.clear();
    if (deserializeJson(doc, buf) != DeserializationError::Ok) continue;
    auto& e = out[n];
    e.isCollection = (doc["c"] | 0) != 0;
    strncpy(e.id, doc["id"] | "", 16);
    e.id[16] = '\0';
    strncpy(e.title, doc["t"] | "", 32);
    e.title[32] = '\0';
    strncpy(e.author, doc["a"] | "", 20);
    e.author[20] = '\0';
    strncpy(e.location, doc["l"] | "", 16);
    e.location[16] = '\0';
    strncpy(e.volume, doc["v"] | "", 16);
    e.volume[16] = '\0';
    strncpy(e.note, doc["note"] | "", 64);
    e.note[64] = '\0';
    e.count = e.isCollection ? (doc["n"] | 0) : 0;
    e.expectedCount = e.isCollection ? (doc["e"] | 0) : 0;
    e.initialVolume = e.isCollection ? (doc["iv"] | 0) : 0;
    ++n;
    yield();
  }
  return n;
}

bool BookCatalog::readLetterIndex(uint16_t* out27) {
  if (!Storage.exists(IDX_FILE)) return false;
  HalFile f;
  if (!Storage.openFileForRead("CAT", IDX_FILE, f)) return false;
  f.read(out27, 27 * sizeof(uint16_t));
  return true;
}

int BookCatalog::letterCount(char letter) {
  if (!Storage.exists(IDX_FILE)) return 0;
  HalFile f;
  if (!Storage.openFileForRead("CAT", IDX_FILE, f)) return 0;
  int li;
  if (letter == '#') {
    li = 26;
  } else {
    li = toupper(static_cast<unsigned char>(letter)) - 'A';
    if (li < 0 || li >= 26) return 0;
  }
  f.seekSet(static_cast<size_t>(li) * sizeof(uint16_t));
  uint16_t count = 0;
  f.read(&count, sizeof(count));
  return static_cast<int>(count);
}

int BookCatalog::readLetterPage(char letter, int start, int maxCount, Entry* out) {
  char path[80];
  snprintf(path, sizeof(path), "%s/%c.ndjson", CATALOG_DIR,
           static_cast<char>(toupper(static_cast<unsigned char>(letter))));
  return readNdjsonPage(path, start, maxCount, out);
}

int BookCatalog::collectionCount(const char* collId) {
  char path[80];
  snprintf(path, sizeof(path), "%s/%s.ndjson", COLL_DIR, collId);
  return countLines(path);
}

int BookCatalog::readCollectionPage(const char* collId, int start, int maxCount, Entry* out) {
  char path[80];
  snprintf(path, sizeof(path), "%s/%s.ndjson", COLL_DIR, collId);
  return readNdjsonPage(path, start, maxCount, out);
}

bool BookCatalog::getCollectionNote(const char* collId, char* out, size_t maxOut) {
  out[0] = '\0';
  char path[80];
  notePath(collId, path, sizeof(path));
  HalFile f;
  if (!Storage.openFileForRead("CAT", path, f)) return false;
  const size_t toRead = std::min(f.fileSize(), maxOut - 1);
  const int n = f.read(out, toRead);
  out[n > 0 ? static_cast<size_t>(n) : 0] = '\0';
  return out[0] != '\0';
}

bool BookCatalog::setCollectionNote(const char* collId, const char* note) {
  char path[80];
  notePath(collId, path, sizeof(path));

  if (!note || !note[0]) {
    Storage.remove(path);
    return true;
  }

  Storage.mkdir(NOTES_DIR);
  HalFile f;
  if (!Storage.openFileForWrite("CAT", path, f)) return false;
  f.write(note, strlen(note));
  return true;
}

int BookCatalog::getCollectionExpectedCount(const char* collId) {
  if (!Storage.exists(REGISTRY_FILE)) return 0;
  HalFile f;
  if (!Storage.openFileForRead("CAT", REGISTRY_FILE, f)) return 0;
  char buf[MAX_LINE];
  while (readLine(f, buf, sizeof(buf))) {
    char id[9] = {};
    extractJsonStr(buf, "id", id, sizeof(id));
    if (strcmp(id, collId) == 0) return extractJsonInt(buf, "e", 0);
  }
  return 0;
}

bool BookCatalog::setCollectionExpectedCount(const char* collId, int expectedCount) {
  if (expectedCount < 0) expectedCount = 0;
  if (!updateRegistryExpectedCount(collId, expectedCount)) return false;

  char name[REG_NAME_MAX + 1] = {};
  if (Storage.exists(REGISTRY_FILE)) {
    HalFile f;
    char buf[MAX_LINE];
    if (Storage.openFileForRead("CAT", REGISTRY_FILE, f)) {
      while (readLine(f, buf, sizeof(buf))) {
        char id[9] = {};
        extractJsonStr(buf, "id", id, sizeof(id));
        if (strcmp(id, collId) == 0) {
          extractJsonStr(buf, "n", name, sizeof(name));
          break;
        }
      }
    }
  }

  if (name[0] && Storage.exists(IDX_FILE)) {
    char letter = upperFirstLetter(name);
    if (!letter) letter = '#';
    char letterPath[80];
    snprintf(letterPath, sizeof(letterPath), "%s/%c.ndjson", CATALOG_DIR, letter);
    updateCollectionHeaderCount(letterPath, collId, collectionCount(collId), expectedCount);
  }
  return true;
}

int BookCatalog::getCollectionInitialVolume(const char* collId) {
  if (!Storage.exists(REGISTRY_FILE)) return 0;
  HalFile f;
  if (!Storage.openFileForRead("CAT", REGISTRY_FILE, f)) return 0;
  char buf[MAX_LINE];
  while (readLine(f, buf, sizeof(buf))) {
    char id[9] = {};
    extractJsonStr(buf, "id", id, sizeof(id));
    if (strcmp(id, collId) == 0) return extractJsonInt(buf, "iv", 0);
  }
  return 0;
}

bool BookCatalog::setCollectionInitialVolume(const char* collId, int initialVolume) {
  if (initialVolume < 0) initialVolume = 0;
  if (!updateRegistryInitialVolume(collId, initialVolume)) return false;

  char name[REG_NAME_MAX + 1] = {};
  if (Storage.exists(REGISTRY_FILE)) {
    HalFile f;
    char buf[MAX_LINE];
    if (Storage.openFileForRead("CAT", REGISTRY_FILE, f)) {
      while (readLine(f, buf, sizeof(buf))) {
        char id[9] = {};
        extractJsonStr(buf, "id", id, sizeof(id));
        if (strcmp(id, collId) == 0) {
          extractJsonStr(buf, "n", name, sizeof(name));
          break;
        }
      }
    }
  }

  if (name[0] && Storage.exists(IDX_FILE)) {
    char letter = upperFirstLetter(name);
    if (!letter) letter = '#';
    char letterPath[80];
    snprintf(letterPath, sizeof(letterPath), "%s/%c.ndjson", CATALOG_DIR, letter);
    updateCollectionHeaderCount(letterPath, collId, collectionCount(collId), -1, initialVolume);
  }
  return true;
}

void BookCatalog::forEachCollection(void (*cb)(const char* id, const char* name, int expectedCount, int initialVolume,
                                               void* ctx),
                                    void* ctx) {
  if (!Storage.exists(REGISTRY_FILE)) return;
  HalFile f;
  if (!Storage.openFileForRead("CAT", REGISTRY_FILE, f)) return;
  char buf[MAX_LINE];
  while (readLine(f, buf, sizeof(buf))) {
    char id[9] = {}, name[REG_NAME_MAX + 1] = {};
    extractJsonStr(buf, "id", id, sizeof(id));
    extractJsonStr(buf, "n", name, sizeof(name));
    const int expectedCount = extractJsonInt(buf, "e", 0);
    const int initialVolume = extractJsonInt(buf, "iv", 0);
    if (id[0]) cb(id, name, expectedCount, initialVolume, ctx);
  }
}

bool BookCatalog::renameCollection(const char* id, const char* newName) {
  if (!updateRegistryName(id, newName)) return false;

  // Update every member book's stored "collection" field so future
  // addEntry/removeEntry calls resolve to the correct (unchanged) id.
  char collPath[80];
  snprintf(collPath, sizeof(collPath), "%s/%s.ndjson", COLL_DIR, id);
  if (Storage.exists(collPath)) {
    HalFile f;
    if (Storage.openFileForRead("CAT", collPath, f)) {
      char buf[MAX_LINE];
      BookStore store;
      while (readLine(f, buf, sizeof(buf))) {
        char bookId[17] = {};
        extractJsonStr(buf, "id", bookId, sizeof(bookId));
        if (!bookId[0]) continue;
        PhysicalBook book;
        if (!store.get(bookId, book)) continue;
        book.collection = newName;
        store.update(book);
      }
    }
  }

  // Header text/letter-bucket placement is now stale until the next rebuild.
  HalFile f;
  Storage.openFileForWrite("CAT", SYNC_FLAG_PATH, f);
  return true;
}

bool BookCatalog::applyBookChange(const BookChangeInfo* oldBook, const BookChangeInfo* newBook) {
  // Register the collection name<->id mapping immediately, even if the full
  // catalog hasn't been built yet (idx.bin missing). Otherwise /api/collections
  // and notes don't see newly-created collections until the next rebuild.
  if (newBook && newBook->collection[0]) {
    char collId[9];
    resolveCollectionId(newBook->collection, collId);
  }

  if (!Storage.exists(IDX_FILE)) return false;  // catalog not built yet

  if (oldBook) removeEntry(*oldBook);
  if (newBook) addEntry(*newBook);

  return true;
}
