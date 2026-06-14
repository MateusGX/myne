#include "BookStore.h"

#include <Logging.h>

namespace {

void serializeBookImpl(JsonDocument& doc, const void* data) {
  const auto& b = *static_cast<const PhysicalBook*>(data);
  doc["id"] = b.id;
  doc["t"] = b.title;
  doc["a"] = b.author;
  doc["c"] = b.collection;
  doc["v"] = b.volume;
  doc["l"] = b.location;
  doc["n"] = b.notes;
}

bool deserializeBookImpl(JsonDocument& doc, void* data) {
  auto& b = *static_cast<PhysicalBook*>(data);
  b.id = doc["id"] | "";
  b.title = doc["t"] | "";
  b.author = doc["a"] | "";
  b.collection = doc["c"] | "";
  b.volume = doc["v"] | "";
  b.location = doc["l"] | "";
  b.notes = doc["n"] | "";
  return !b.id.empty();
}

}  // namespace

bool BookStore::init() {
  if (initialized_) return true;
  store_.init();
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
  return true;
}

bool BookStore::get(const std::string& id, PhysicalBook& out) const {
  return store_.load(id.c_str(), deserializeBookImpl, &out);
}

bool BookStore::remove(const std::string& id) {
  init();
  if (!store_.remove(id.c_str())) {
    LOG_ERR("BOOKS", "Book not found for delete: %s", id.c_str());
    return false;
  }
  return true;
}

void BookStore::serializeBook(JsonDocument& doc, const void* data) { serializeBookImpl(doc, data); }
bool BookStore::deserializeBook(JsonDocument& doc, void* data) { return deserializeBookImpl(doc, data); }
