#pragma once

#include <string>

#include "../Activity.h"
#include <BookStore.h>
#include <ReadingLog.h>

class PhysicalBookDetailActivity final : public Activity {
  PhysicalBook book;
  ReadingLog   readingLog;

  // Cached last/active reading — populated from compact summary binary in onEnter()
  bool          hasReading   = false;
  ReadingStatus lastStatus   = ReadingStatus::Reading;
  ReadingType   lastType     = ReadingType::Page;
  int           lastPosition = 0;
  char          lastDate[11] = {};  // "YYYY-MM-DD" + NUL

 public:
  explicit PhysicalBookDetailActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                      PhysicalBook book)
      : Activity("PhysicalBookDetail", renderer, mappedInput), book(std::move(book)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
