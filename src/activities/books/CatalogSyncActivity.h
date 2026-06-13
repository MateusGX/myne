#pragma once

#include "../Activity.h"

// Runs BookCatalog::rebuild() + ReadingLog::rebuildStats() with a live progress screen.
// Launched automatically at boot or after exiting the network activity when the sync
// flag file (BookCatalog::SYNC_FLAG_PATH) is present.  Navigates home when done.
class CatalogSyncActivity final : public Activity {
  enum class State { SYNCING, DONE, FAILED };

  State         state_            = State::SYNCING;
  int           processedCount_   = 0;
  int           lastRenderedCount = -1;
  unsigned long doneAt_           = 0;

  void doSync();

 public:
  explicit CatalogSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("CatalogSync", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
