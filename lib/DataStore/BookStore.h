#pragma once
#include <cstddef>
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
  std::string note;
};

class BookStore {
 public:
  // Per-book JSON directory.
  static constexpr const char* DIR_PATH = "/.myne/books";
  static constexpr const char* NOTE_ROOT_DIR = "/.myne/notes";
  // Per-book plain-text note directory. Keyed by book id.
  static constexpr const char* NOTE_DIR = "/.myne/notes/books";

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

  // Read/write/clear the plain-text note for a book.
  static bool getNote(const char* id, char* out, size_t maxOut);
  static bool setNote(const char* id, const char* note);

  // Count .json files without parsing. O(N), O(1) RAM.
  int countBooks() const { return store_.count(); }

 private:
  RecordStore store_{DIR_PATH};
  bool initialized_ = false;

  static void serializeBook(JsonDocument& doc, const void* data);
  static bool deserializeBook(JsonDocument& doc, void* data);
};
