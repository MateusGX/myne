#pragma once

#include "../Activity.h"

class HomeActivity final : public Activity {
  int selectorIndex = 0;

  // Last-read book data, loaded once in onEnter()
  bool hasLastRead = false;
  char lastReadTitle[33] = {};
  char lastReadAuthor[21] = {};
  char lastReadProgress[16] = {};  // "p. 42" or "ch. 3"
  char lastReadDate[18] = {};      // "Jan '26 21:00"
  char lastReadStatus[16] = {};

  void loadLastRead();

  void onSettingsOpen();
  void onFileTransferOpen();
  void onPhysicalBooksOpen();
  void onReadingStatsOpen();
  void onLastReadOpen();

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Home", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
