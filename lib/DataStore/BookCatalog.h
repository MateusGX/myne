#pragma once
#include <cstddef>
#include <cstdint>

// Device-maintained physical-book catalog stored as NDJSON.
// Rebuilt by BookCatalog::rebuild() — triggered automatically at boot or after
// the network activity when SYNC_FLAG_PATH is present.
// All reads are paginated — never loads the full catalog into memory.
//
// On disk (all under CATALOG_DIR):
//   idx.bin          27 × uint16_t  — entry count per letter A-Z + '#' (index 0='A', 26='#')
//   {A-Z}.ndjson     collections first (by name), then standalone books (by title)
//   #.ndjson         books/collections whose title starts with a digit or symbol
//   c/{id8}.ndjson   books within one collection (sorted by title)
//
// Collection identity is persistent: REGISTRY_FILE maps collection name <->
// 8-hex id, so ids survive renames/syncs/rebuilds (see resolveCollectionId).
//
// NDJSON line formats (max MAX_LINE bytes each):
//   Book:       {"id":"...","t":"...","a":"...","l":"..."}
//   Collection: {"id":"...","t":"...","c":1,"n":42}  (c=1: header; n: book count)
class BookCatalog {
 public:
  static constexpr const char* CATALOG_DIR    = "/.myne/catalog";
  static constexpr const char* IDX_FILE       = "/.myne/catalog/idx.bin";
  static constexpr const char* COLL_DIR       = "/.myne/catalog/c";
  static constexpr const char* TMP_DIR        = "/.myne/catalog/tmp";
  static constexpr const char* COLL_META_FILE = "/.myne/catalog/tmp/colls.ndjson";
  static constexpr const char* SYNC_FLAG_PATH = "/.myne/sync_needed";
  // Persistent collection-id registry: one line per collection,
  // {"id":"<8hex>","n":"<name>"}. Lives outside CATALOG_DIR so it survives
  // rebuild()'s cleanFilesInDir() calls.
  static constexpr const char* REGISTRY_FILE  = "/.myne/collections.ndjson";
  // Persistent per-collection notes, keyed by collection id. Lives outside
  // CATALOG_DIR so notes survive rebuild()'s cleanFilesInDir() calls.
  static constexpr const char* NOTES_DIR      = "/.myne/notes";
  static constexpr size_t      MAX_LINE       = 256;

  struct Entry {
    bool  isCollection = false;
    char  id[17]       = {};   // book id OR 8-hex collection id, null-terminated
    char  title[33]    = {};   // book title OR collection name
    char  author[21]   = {};   // empty for collections
    char  location[17] = {};   // empty for collections
    char  volume[17]   = {};   // book volume (e.g. "Vol. 1"); empty for collections
    char  note[65]     = {};   // collection note; empty for books
    int   count        = 0;    // 0 for books; books-in-collection for headers
  };

  // Returns the persistent 8-hex id for a collection name, creating a new
  // registry entry (with a fresh random id) if the name isn't registered yet.
  // outId must be >= 9 bytes. Names are truncated to 96 raw bytes for
  // storage/lookup (independent of the 32-char Entry::title[33] truncation
  // used for on-device display).
  static bool resolveCollectionId(const char* name, char* outId);

  // Calls cb(id, name, ctx) for every registered collection (id is 8-hex,
  // name is the original UTF-8 collection name, both NUL-terminated).
  static void forEachCollection(void (*cb)(const char* id, const char* name, void* ctx), void* ctx);

  // Renames collection `id` to `newName`: updates the registry entry, updates
  // every member book's stored "collection" field, and flags the catalog for
  // a full rebuild (header text/letter-bucket placement is stale until then).
  // Returns false if `id` isn't registered.
  static bool renameCollection(const char* id, const char* newName);

  // Rebuild entire catalog from per-book JSON files in booksDir.
  // Phase 1 is O(1) memory; sort passes are O(letter_size) or O(collection_size).
  // SD card capacity is the only hard limit on the number of books.
  // onProgress(processed, ctx) is called after each book in Phase 1 (pass nullptr to skip).
  static bool rebuild(const char* booksDir,
                      void (*onProgress)(int processed, void* ctx) = nullptr,
                      void* ctx = nullptr);

  // Read per-letter entry counts into out27[27] (index 0 = 'A', index 26 = '#').
  // Returns false if idx.bin is missing (catalog not built yet).
  static bool readLetterIndex(uint16_t* out27);

  // Total entries for a letter (collections + standalone books).
  // Returns 0 if not built or no books for this letter.
  static int letterCount(char letter);

  // Load up to maxCount entries starting at absolute index 'start' for a letter.
  // Collections appear first, followed by standalone books sorted by title.
  // Returns number of entries loaded (may be < maxCount at end of list).
  static int readLetterPage(char letter, int start, int maxCount, Entry* out);

  // Total books in a collection.
  static int collectionCount(const char* collId);

  // Load up to maxCount entries starting at 'start' from a collection.
  // Returns number of entries loaded.
  static int readCollectionPage(const char* collId, int start, int maxCount, Entry* out);

  // Read the persistent note for a collection (identified by its 8-hex id).
  // Returns false if no note exists. out must be at least maxOut bytes.
  static bool getCollectionNote(const char* collId, char* out, size_t maxOut);

  // Write (or clear) the persistent note for a collection.
  // Pass an empty string to remove the note.
  static bool setCollectionNote(const char* collId, const char* note);

  // Raw fields describing a book's catalog-relevant attributes.
  // All pointers must be non-null; use "" for empty fields.
  struct BookChangeInfo {
    const char* id;
    const char* title;
    const char* author;
    const char* location;
    const char* volume;
    const char* collection;
  };

  // Apply an incremental add/update/remove without a full rebuild:
  //   oldBook == nullptr  -> book created   (insert newBook)
  //   newBook == nullptr  -> book deleted   (remove oldBook)
  //   both non-null       -> book updated   (remove oldBook, insert newBook)
  // Touches only the affected per-letter/per-collection NDJSON files and idx.bin.
  // Returns false if the catalog has not been built yet (idx.bin missing) —
  // callers should fall back to setting SYNC_FLAG_PATH for a full rebuild.
  static bool applyBookChange(const BookChangeInfo* oldBook, const BookChangeInfo* newBook);
};
