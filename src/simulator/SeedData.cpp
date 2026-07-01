#ifdef SIMULATOR

#include "SeedData.h"

#include <ArduinoJson.h>
#include <BookCatalog.h>
#include <BookStore.h>
#include <HalStorage.h>
#include <Logging.h>
#include <ReadingLog.h>

static constexpr const char* BOOK1_ID = "sim0000000000001";
static constexpr const char* BOOK2_ID = "sim0000000000002";
static constexpr const char* BOOK3_ID = "sim0000000000003";
static constexpr const char* LOTR_COLLECTION = "O Senhor dos Aneis";

static void writeJson(const char* path, const JsonDocument& doc) {
  char buf[512];
  size_t len = serializeJson(doc, buf, sizeof(buf));
  HalFile file;
  if (!Storage.openFileForWrite("SEED", path, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(buf), len);
}

static void seedBook(const char* id, const char* title, const char* author, const char* collection, const char* volume,
                     const char* location, const char* note = "") {
  char path[80];
  snprintf(path, sizeof(path), "%s/%s.json", BookStore::DIR_PATH, id);
  if (Storage.exists(path)) return;

  Storage.mkdir(BookStore::DIR_PATH);
  JsonDocument doc;
  doc["id"] = id;
  doc["t"] = title;
  doc["a"] = author;
  doc["c"] = collection;
  doc["v"] = volume;
  doc["l"] = location;
  writeJson(path, doc);
  BookStore::setNote(id, note);
}

static void seedCollectionNote(const char* collection, const char* note) {
  char id[9];
  BookCatalog::resolveCollectionId(collection, id);
  BookCatalog::setCollectionNote(id, note);
}

static void seedReadings(const char* bookId, const char* status, int type,
                         std::initializer_list<std::tuple<const char*, const char*, int>> sessions) {
  char path[80];
  snprintf(path, sizeof(path), "%s/%s.json", ReadingLog::DIR_PATH, bookId);
  if (Storage.exists(path)) return;

  Storage.mkdir(ReadingLog::DIR_PATH);
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  JsonObject obj = arr.add<JsonObject>();
  obj["id"] = bookId;
  obj["s"] = status;
  if (type == 1) obj["rt"] = 1;
  JsonArray sv = obj["sessions"].to<JsonArray>();
  for (const auto& [date, time, pos] : sessions) {
    JsonObject se = sv.add<JsonObject>();
    se["d"] = date;
    se["tm"] = time;
    se["p"] = pos;
  }

  writeJson(path, doc);
}

void seedSimulatorData() {
  seedBook(BOOK1_ID, "O Senhor dos Aneis - A Sociedade do Anel", "J.R.R. Tolkien", LOTR_COLLECTION, "Vol. 1",
           "Estante A", "notas aqui");

  seedBook(BOOK3_ID, "O Senhor dos Aneis - As Duas Torres", "J.R.R. Tolkien", LOTR_COLLECTION, "Vol. 2", "Estante A",
           "Segundo volume da saga.");

  seedBook(BOOK2_ID, "Fundacao", "Isaac Asimov", "", "", "Estante B");

  seedReadings(BOOK1_ID, "reading", 0,
               {{"2026-05-10", "21:00", 80}, {"2026-05-15", "20:30", 195}, {"2026-05-22", "22:00", 287}});

  seedReadings(BOOK2_ID, "finished", 0,
               {{"2026-04-01", "19:00", 120}, {"2026-04-08", "21:30", 255}, {"2026-04-14", "20:00", 380}});

  seedCollectionNote(LOTR_COLLECTION, "Colecao principal de Tolkien; conferir volumes e ordem de leitura.");

  BookCatalog::rebuild(BookStore::DIR_PATH);
  LOG_INF("SEED", "Simulator sample data ready");
}

#endif  // SIMULATOR
