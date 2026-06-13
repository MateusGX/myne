#pragma once

#include "../Activity.h"
#include <ReadingLog.h>

struct ReadingStats {
  int totalBooks    = 0;
  int totalReadings = 0;
  int totalSessions = 0;
  int byStatus[5]   = {};  // indexed by ReadingStatus (0-4)
  int byTracking[2] = {};  // 0=Page, 1=Chapter
};

class ReadingStatsActivity final : public Activity {
  enum class View : uint8_t { Overview = 0, Month = 1, Year = 2 };
  static constexpr uint8_t VIEW_COUNT = 3;

  ReadingLog   readingLog;
  ReadingStats stats;

  View currentView = View::Overview;
  int  viewYear    = 2024;
  int  viewMonth   = 1;  // 1-12

  int periodData[31]    = {};  // sessions per day
  int yearMonthData[12] = {};  // sessions per month

  void loadAll();
  void loadPeriodData();

  void cycleView();
  void prevPeriod();
  void nextPeriod();

  void renderOverview(int y, int pageWidth, int pageHeight);
  void renderMonth(int y, int pageWidth, int pageHeight);
  void renderYear(int y, int pageWidth, int pageHeight);

 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
