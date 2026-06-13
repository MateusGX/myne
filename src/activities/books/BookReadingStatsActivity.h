#pragma once

#include <cstdint>
#include <vector>

#include "../Activity.h"
#include <BookStore.h>
#include <ReadingLog.h>

class BookReadingStatsActivity final : public Activity {
  enum class View : uint8_t { Summary, Timeline };

 public:
  struct MonthBucket {
    int16_t year;
    uint8_t month;
    int16_t count;
  };

 private:
  PhysicalBook book;
  ReadingLog   readingLog;
  View         currentView = View::Summary;

  int  totalReadings = 0;
  int  totalSessions = 0;
  char firstDate[11] = {};
  char lastDate[11]  = {};
  std::vector<MonthBucket> monthBuckets;

  void loadData();
  void cycleView();
  void renderSummary(int contentTop, int pageWidth, int pageHeight);
  void renderTimeline(int contentTop, int pageWidth, int pageHeight);

 public:
  explicit BookReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                    PhysicalBook book)
      : Activity("BookReadingStats", renderer, mappedInput), book(std::move(book)) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
