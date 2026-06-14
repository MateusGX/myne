#pragma once

#include <BookStore.h>
#include <ReadingLog.h>

#include <string>

#include "../Activity.h"

class ReadingEditActivity final : public Activity {
  enum class Field : uint8_t { Status = 0, Date = 1, Position = 2, SyncTime = 3, Type = 4 };
  static constexpr int FIELD_COUNT = 5;

  enum class SyncState : uint8_t { None, Syncing, Success, Failed };

  PhysicalBook book;
  ReadingLog readingLog;
  Reading reading;

  Field selectedField = Field::Status;
  ReadingStatus originalStatus = ReadingStatus::Reading;  // set in onEnter for dirty check
  int dateSubField = 2;                                   // 0=year, 1=month, 2=day, 3=hour, 4=minute
  int logYear = 2025;
  int logMonth = 1;
  int logDay = 1;
  int logHour = 0;
  int logMinute = 0;
  bool hasTime = false;
  int logPosition = 0;
  bool dirty = false;
  bool wifiTurnedOn = false;
  SyncState syncState = SyncState::None;

  void initDate();
  void adjustDate(int dir);
  void dateToBuffer(char* buf, size_t n) const;
  void adjustField(int dir);
  void logSession();
  void startWifiSync();
  void onWifiConnected(bool success);

 public:
  explicit ReadingEditActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, PhysicalBook book,
                               Reading reading)
      : Activity("ReadingEdit", renderer, mappedInput), book(std::move(book)), reading(std::move(reading)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
