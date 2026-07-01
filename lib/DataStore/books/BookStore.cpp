#include "BookStore.h"

#include <Logging.h>

#include <cstring>

namespace {

void notePath(const char* id, char* out, size_t outSize) {
  snprintf(out, outSize, "%s/%s.note", BookStore::NOTE_DIR, id);
}

void serializeBookImpl(JsonDocument& doc, const void* data) {
  const auto& b = *static_cast<const PhysicalBook*>(data);
  doc["id"] = b.id;
  doc["t"] = b.title;
  doc["a"] = b.author;
  doc["c"] = b.collection;
  doc["v"] = b.volume;
  doc["l"] = b.location;
}

bool deserializeBookImpl(JsonDocument& doc, void* data) {
  auto& b = *static_cast<PhysicalBook*>(data);
  b.id = doc["id"] | "";
  b.title = doc["t"] | "";
  b.author = doc["a"] | "";
  b.collection = doc["c"] | "";
  b.volume = doc["v"] | "";
  b.location = doc["l"] | "";
  b.note = doc["note"] | "";
  return !b.id.empty();
}

}  // namespace

bool BookStore::init() {
  if (initialized_) return true;
  store_.init();
  Storage.mkdir(NOTE_ROOT_DIR);
  Storage.mkdir(NOTE_DIR);
  initialized_ = true;
  return true;
}

bool BookStore::create(PhysicalBook& book) {
  init();
  char idBuf[16];
  DataStoreUtils::generateId(idBuf, sizeof(idBuf));
  book.id = idBuf;
  if (!store_.save(idBuf, serializeBookImpl, &book)) {
    LOG_ERR("BOOKS", "Failed to write book %s", idBuf);
    return false;
  }
  if (!setNote(idBuf, book.note.c_str())) {
    LOG_ERR("BOOKS", "Failed to write note for book %s", idBuf);
    return false;
  }
  return true;
}

bool BookStore::update(const PhysicalBook& book) {
  init();
  if (!store_.exists(book.id.c_str())) {
    LOG_ERR("BOOKS", "Book not found for update: %s", book.id.c_str());
    return false;
  }
  if (!store_.save(book.id.c_str(), serializeBookImpl, &book)) {
    LOG_ERR("BOOKS", "Failed to write book %s", book.id.c_str());
    return false;
  }
  if (!setNote(book.id.c_str(), book.note.c_str())) {
    LOG_ERR("BOOKS", "Failed to write note for book %s", book.id.c_str());
    return false;
  }
  return true;
}

bool BookStore::get(const std::string& id, PhysicalBook& out) const {
  if (!store_.load(id.c_str(), deserializeBookImpl, &out)) return false;
  char note[512] = {};
  if (getNote(id.c_str(), note, sizeof(note))) {
    out.note = note;
  }
  return true;
}

bool BookStore::remove(const std::string& id) {
  init();
  if (!store_.remove(id.c_str())) {
    LOG_ERR("BOOKS", "Book not found for delete: %s", id.c_str());
    return false;
  }
  setNote(id.c_str(), "");
  return true;
}

bool BookStore::getNote(const char* id, char* out, size_t maxOut) {
  if (!out || maxOut == 0) return false;
  out[0] = '\0';
  if (!id || !id[0]) return false;
  char path[96];
  notePath(id, path, sizeof(path));
  if (!Storage.exists(path)) return false;
  const size_t n = Storage.readFileToBuffer(path, out, maxOut);
  out[n < maxOut ? n : maxOut - 1] = '\0';
  return n > 0;
}

bool BookStore::setNote(const char* id, const char* note) {
  if (!id || !id[0]) return false;
  Storage.mkdir(NOTE_ROOT_DIR);
  Storage.mkdir(NOTE_DIR);
  char path[96];
  notePath(id, path, sizeof(path));
  if (!note || !note[0]) {
    if (Storage.exists(path)) Storage.remove(path);
    return true;
  }
  HalFile f;
  if (!Storage.openFileForWrite("BOOKS", path, f)) return false;
  f.write(note, strlen(note));
  return true;
}

void BookStore::serializeBook(JsonDocument& doc, const void* data) { serializeBookImpl(doc, data); }
bool BookStore::deserializeBook(JsonDocument& doc, void* data) { return deserializeBookImpl(doc, data); }
