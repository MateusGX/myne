#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include <BookStore.h>
#include <ReadingLog.h>
#include "util/ButtonNavigator.h"

class BookReadingsActivity final : public Activity {
  PhysicalBook book;
  ReadingLog readingLog;
  std::vector<Reading> readings;
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool confirmingDelete = false;

  void loadReadings();
  void openSelected();
  void createNew();
  void deleteSelected();

 public:
  explicit BookReadingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                PhysicalBook book)
      : Activity("BookReadings", renderer, mappedInput), book(std::move(book)) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
