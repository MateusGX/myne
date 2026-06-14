#pragma once
#include <string>

#include "DataStoreUtils.h"
#include "RecordStore.h"

struct PhysicalBook {
  std::string id;
  std::string title;
  std::string author;
  std::string collection;
  std::string volume;
  std::string location;
  std::string notes;
};

class BookStore {
 public:
  // Per-book JSON directory.
  static constexpr const char* DIR_PATH = "/.myne/books";

  // Ensure the books directory exists.
  bool init();

  // Write a new book JSON file. Assigns book.id from millis().
  bool create(PhysicalBook& book);

  // Overwrite an existing book JSON file.
  bool update(const PhysicalBook& book);

  // Load a book by id. Returns false if the book doesn't exist.
  bool get(const std::string& id, PhysicalBook& out) const;

  // Delete a book JSON file.
  bool remove(const std::string& id);

  // Count .json files without parsing. O(N), O(1) RAM.
  int countBooks() const { return store_.count(); }

 private:
  RecordStore store_{DIR_PATH};
  bool initialized_ = false;

  static void serializeBook(JsonDocument& doc, const void* data);
  static bool deserializeBook(JsonDocument& doc, void* data);
};
