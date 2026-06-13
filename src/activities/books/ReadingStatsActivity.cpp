#include "ReadingStatsActivity.h"

#include <BookStore.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <ReadingLog.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "BooksActivityUI.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

static constexpr StrId MONTH_STRIDS[12] = {
    StrId::STR_MONTH_JAN, StrId::STR_MONTH_FEB, StrId::STR_MONTH_MAR, StrId::STR_MONTH_APR,
    StrId::STR_MONTH_MAY, StrId::STR_MONTH_JUN, StrId::STR_MONTH_JUL, StrId::STR_MONTH_AUG,
    StrId::STR_MONTH_SEP, StrId::STR_MONTH_OCT, StrId::STR_MONTH_NOV, StrId::STR_MONTH_DEC};

static constexpr StrId MONTH_SHORT_STRIDS[12] = {
    StrId::STR_MONTH_SHORT_JAN, StrId::STR_MONTH_SHORT_FEB, StrId::STR_MONTH_SHORT_MAR,
    StrId::STR_MONTH_SHORT_APR, StrId::STR_MONTH_SHORT_MAY, StrId::STR_MONTH_SHORT_JUN,
    StrId::STR_MONTH_SHORT_JUL, StrId::STR_MONTH_SHORT_AUG, StrId::STR_MONTH_SHORT_SEP,
    StrId::STR_MONTH_SHORT_OCT, StrId::STR_MONTH_SHORT_NOV, StrId::STR_MONTH_SHORT_DEC};

static int daysInMonth(int year, int month) {
  static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
  return days[month - 1];
}

static const char* statusLabel(int s) {
  switch (s) {
    case 0: return tr(STR_STATUS_WANT_TO_READ);
    case 1: return tr(STR_STATUS_READING);
    case 2: return tr(STR_STATUS_PAUSED);
    case 3: return tr(STR_STATUS_FINISHED);
    case 4: return tr(STR_STATUS_DROPPED);
  }
  return "";
}

namespace {
constexpr int kPad = 20;
constexpr int kInner = 16;
constexpr int kGap = 12;
constexpr int kHeroH = 110;
constexpr int kViewCount = 3;


void drawPageDots(const GfxRenderer& r, int x, int y, int active) {
  for (int i = 0; i < kViewCount; ++i) {
    const int dx = x + i * 22;
    if (i == active) {
      r.fillRoundedRect(dx, y, 16, 6, 3, Color::Black);
    } else {
      r.drawRoundedRect(dx + 5, y, 6, 6, 1, 3, true);
    }
  }
}

void drawViewBadge(const GfxRenderer& r, Rect bounds, int index) {
  char text[8];
  snprintf(text, sizeof(text), "%d/%d", index + 1, kViewCount);
  const int tw = r.getTextWidth(SMALL_FONT_ID, text, EpdFontFamily::BOLD);
  const int w = tw + 22;
  const int h = r.getLineHeight(SMALL_FONT_ID) + 10;
  const int x = bounds.x + bounds.width - kInner - w;
  const int y = bounds.y + 18;
  r.fillRoundedRect(x, y, w, h, 5, Color::Black);
  r.drawText(SMALL_FONT_ID, x + (w - tw) / 2, y + 5, text, false, EpdFontFamily::BOLD);
}

void drawHero(const GfxRenderer& r, Rect bounds, const char* title, const char* subtitle,
              int activeView) {
  BooksActivityUI::hero(r, bounds, tr(STR_READING_STATS), title, subtitle, 86);
  drawViewBadge(r, bounds, activeView);
  drawPageDots(r, bounds.x + kInner, bounds.y + bounds.height - 16, activeView);
}

void drawKpi(const GfxRenderer& r, Rect rect, const char* label, int value,
             const char* detail = nullptr, bool selected = false) {
  BooksActivityUI::panel(r, rect, selected);
  const int textX = rect.x + kInner;
  char valueText[16];
  snprintf(valueText, sizeof(valueText), "%d", value);
  r.drawText(UI_12_FONT_ID, textX, rect.y + 18, valueText, true, EpdFontFamily::BOLD);
  BooksActivityUI::text(r, SMALL_FONT_ID, textX, rect.y + 58, label, rect.width - kInner * 2,
               EpdFontFamily::BOLD);
  if (detail && detail[0] != '\0') {
    BooksActivityUI::text(r, SMALL_FONT_ID, textX, rect.y + rect.height - 36, detail,
                 rect.width - kInner * 2);
  }
}

void drawCompactStat(const GfxRenderer& r, Rect rect, const char* label, int value,
                     bool selected = false) {
  BooksActivityUI::panel(r, rect, selected);
  BooksActivityUI::text(r, SMALL_FONT_ID, rect.x + 12, rect.y + 11, label, rect.width - 58,
               EpdFontFamily::BOLD);
  char valueText[12];
  snprintf(valueText, sizeof(valueText), "%d", value);
  const int tw = r.getTextWidth(UI_10_FONT_ID, valueText, EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, rect.x + rect.width - 12 - tw, rect.y + 24,
             valueText, true, EpdFontFamily::BOLD);
}

void drawInlineStat(const GfxRenderer& r, Rect rect, const char* label, int value) {
  BooksActivityUI::text(r, SMALL_FONT_ID, rect.x, rect.y + 6, label, rect.width - 54,
               EpdFontFamily::BOLD);
  char valueText[12];
  snprintf(valueText, sizeof(valueText), "%d", value);
  const int tw = r.getTextWidth(UI_10_FONT_ID, valueText, EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, rect.x + rect.width - tw, rect.y + 13,
             valueText, true, EpdFontFamily::BOLD);
}

void drawSectionTitle(const GfxRenderer& r, int x, int y, const char* title,
                      const char* right = nullptr) {
  int titleW = r.getScreenWidth() - kPad - kInner - x;
  if (right && right[0] != '\0') {
    const int rw = r.getTextWidth(SMALL_FONT_ID, right, EpdFontFamily::BOLD);
    titleW -= rw + 16;
    r.drawText(SMALL_FONT_ID, r.getScreenWidth() - kPad - kInner - rw, y,
               right, true, EpdFontFamily::BOLD);
  }
  BooksActivityUI::text(r, SMALL_FONT_ID, x, y, title, titleW, EpdFontFamily::BOLD);
}

void drawStatusRow(const GfxRenderer& r, Rect rect, const char* label, int value, int maxValue) {
  constexpr int labelW = 130;
  constexpr int valueW = 30;
  const int barX = rect.x + labelW;
  const int barW = rect.width - labelW - valueW - 10;
  const int barY = rect.y + 11;
  BooksActivityUI::text(r, SMALL_FONT_ID, rect.x, rect.y + 5, label, labelW - 8,
               EpdFontFamily::BOLD);
  r.drawRoundedRect(barX, barY, barW, 10, 1, 5, true);
  if (value > 0) {
    const int fillW = std::max(6, (barW - 4) * value / std::max(1, maxValue));
    r.fillRoundedRect(barX + 2, barY + 2, fillW, 6, 3, Color::Black);
  }
  char count[8];
  snprintf(count, sizeof(count), "%d", value);
  const int cw = r.getTextWidth(SMALL_FONT_ID, count, EpdFontFamily::BOLD);
  r.drawText(SMALL_FONT_ID, rect.x + rect.width - cw, rect.y + 5,
             count, true, EpdFontFamily::BOLD);
}

void drawMonthCell(const GfxRenderer& r, Rect rect, const char* month, int value, int maxValue) {
  const bool selected = value > 0;
  const int leftPad = selected ? 22 : 10;
  BooksActivityUI::panel(r, rect, selected);
  BooksActivityUI::text(r, SMALL_FONT_ID, rect.x + leftPad, rect.y + 9, month, rect.width - leftPad - 10,
               EpdFontFamily::BOLD);
  char count[8];
  snprintf(count, sizeof(count), "%d", value);
  const int cw = r.getTextWidth(UI_10_FONT_ID, count, EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, rect.x + rect.width - 10 - cw, rect.y + 24,
             count, true, EpdFontFamily::BOLD);
  if (value > 0) {
    const int trackW = rect.width - leftPad - 10;
    const int fillW = std::max(7, trackW * value / std::max(1, maxValue));
    r.drawRoundedRect(rect.x + leftPad, rect.y + rect.height - 14, trackW, 6, 1, 3, true);
    r.fillRoundedRect(rect.x + leftPad, rect.y + rect.height - 14, fillW, 6, 3, Color::Black);
  }
}

void drawActivityStrip(const GfxRenderer& r, Rect rect, const int* values, int count, int maxValue) {
  const int gap = 3;
  const int cellW = std::max(4, (rect.width - gap * (count - 1)) / count);
  for (int i = 0; i < count; ++i) {
    const int x = rect.x + i * (cellW + gap);
    const int v = values[i];
    r.drawRoundedRect(x, rect.y, cellW, rect.height, 1, 2, true);
    if (v > 0) {
      const int fillH = std::max(4, (rect.height - 4) * v / std::max(1, maxValue));
      r.fillRoundedRect(x + 2, rect.y + rect.height - fillH - 2,
                        std::max(1, cellW - 4), fillH, 2, Color::Black);
    }
  }
}

void drawDayChip(const GfxRenderer& r, Rect rect, int day, int value, bool selected) {
  BooksActivityUI::panel(r, rect, selected);
  const int leftPad = selected ? 22 : 10;
  char dayText[8];
  snprintf(dayText, sizeof(dayText), "%02d", day);
  r.drawText(SMALL_FONT_ID, rect.x + leftPad, rect.y + 9, dayText, true, EpdFontFamily::BOLD);
  char valueText[8];
  snprintf(valueText, sizeof(valueText), "%d", value);
  const int tw = r.getTextWidth(UI_10_FONT_ID, valueText, EpdFontFamily::BOLD);
  r.drawText(UI_10_FONT_ID, rect.x + rect.width - 10 - tw, rect.y + 24,
             valueText, true, EpdFontFamily::BOLD);
}

void drawEmpty(const GfxRenderer& r, Rect rect) {
  BooksActivityUI::panel(r, rect);
  const int lh = r.getLineHeight(UI_10_FONT_ID);
  r.drawCenteredText(UI_10_FONT_ID, rect.y + (rect.height - lh) / 2,
                     tr(STR_NO_SESSIONS), true, EpdFontFamily::BOLD);
}
}  // namespace

void ReadingStatsActivity::loadAll() {
  stats = {};
  viewYear = 2024;
  viewMonth = 1;

  StatsCache cache;
  if (!readingLog.loadStatsSummary(cache)) {
    readingLog.rebuildStats(BookStore::DIR_PATH);
    if (!readingLog.loadStatsSummary(cache)) return;
  }

  stats.totalBooks = cache.totalBooks;
  stats.totalReadings = cache.totalReadings;
  stats.totalSessions = cache.totalSessions;
  for (int i = 0; i < 5; ++i) stats.byStatus[i] = cache.byStatus[i];
  for (int i = 0; i < 2; ++i) stats.byTracking[i] = cache.byTracking[i];
  viewYear = cache.latestYear;
  viewMonth = cache.latestMonth;
}

void ReadingStatsActivity::loadPeriodData() {
  memset(periodData, 0, sizeof(periodData));
  memset(yearMonthData, 0, sizeof(yearMonthData));

  if (currentView == View::Year) {
    readingLog.loadStatsYear(viewYear, yearMonthData);
  } else if (currentView == View::Month) {
    readingLog.loadStatsMonth(viewYear, viewMonth, periodData);
  }
}

void ReadingStatsActivity::cycleView() {
  currentView = static_cast<View>((static_cast<uint8_t>(currentView) + 1) % VIEW_COUNT);
  loadPeriodData();
  requestUpdate();
}

void ReadingStatsActivity::prevPeriod() {
  if (currentView == View::Year) {
    --viewYear;
  } else if (currentView == View::Month) {
    if (--viewMonth < 1) {
      viewMonth = 12;
      --viewYear;
    }
  }
  loadPeriodData();
  requestUpdate();
}

void ReadingStatsActivity::nextPeriod() {
  if (currentView == View::Year) {
    ++viewYear;
  } else if (currentView == View::Month) {
    if (++viewMonth > 12) {
      viewMonth = 1;
      ++viewYear;
    }
  }
  loadPeriodData();
  requestUpdate();
}

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  currentView = View::Overview;
  loadAll();
  requestUpdate();
}

void ReadingStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.popActivity();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    cycleView();
    return;
  }
  if (currentView != View::Overview) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      prevPeriod();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      nextPeriod();
      return;
    }
  }
}

void ReadingStatsActivity::renderOverview(int y, int pageWidth, int pageHeight) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentW = pageWidth - kPad * 2;

  const int heroCardH = 126;
  BooksActivityUI::panel(renderer, Rect{kPad, y, contentW, heroCardH}, true);
  BooksActivityUI::text(renderer, SMALL_FONT_ID, kPad + kInner, y + 18, "Library",
               contentW - kInner * 2, EpdFontFamily::BOLD);
  char sessions[16];
  snprintf(sessions, sizeof(sessions), "%d", stats.totalSessions);
  renderer.drawText(UI_12_FONT_ID, kPad + kInner, y + 45, sessions, true, EpdFontFamily::BOLD);
  BooksActivityUI::text(renderer, SMALL_FONT_ID, kPad + kInner, y + 88, tr(STR_STATS_SESSIONS),
               contentW / 2, EpdFontFamily::BOLD);

  const int splitX = kPad + contentW / 2 + 8;
  const int rightX = splitX + kInner;
  const int rightW = kPad + contentW - kInner - rightX;
  renderer.drawLine(splitX, y + 18, splitX, y + heroCardH - 18);
  renderer.drawLine(rightX, y + heroCardH / 2, rightX + rightW, y + heroCardH / 2);
  drawInlineStat(renderer, Rect{rightX, y + 22, rightW, 42}, tr(STR_STATS_BOOKS),
                 stats.totalBooks);
  drawInlineStat(renderer, Rect{rightX, y + 78, rightW, 42}, tr(STR_STATS_READINGS),
                 stats.totalReadings);
  y += heroCardH + kGap;

  const int statW = (contentW - kGap * 2) / 3;
  drawCompactStat(renderer, Rect{kPad, y, statW, 62}, tr(STR_READ_BY_PAGE),
                  stats.byTracking[0], true);
  drawCompactStat(renderer, Rect{kPad + statW + kGap, y, statW, 62},
                  tr(STR_READ_BY_CHAPTER), stats.byTracking[1]);
  const int finished = stats.byStatus[3];
  drawCompactStat(renderer, Rect{kPad + (statW + kGap) * 2, y, statW, 62},
                  tr(STR_STATUS_FINISHED), finished);
  y += 62 + kGap;

  const int panelH = pageHeight - y - metrics.buttonHintsHeight - 14;
  BooksActivityUI::panel(renderer, Rect{kPad, y, contentW, panelH});
  drawSectionTitle(renderer, kPad + kInner, y + 18, tr(STR_STATS_BY_STATUS));

  int maxStatus = 1;
  for (int i = 0; i < 5; ++i) maxStatus = std::max(maxStatus, stats.byStatus[i]);

  int rowY = y + 54;
  for (int i = 0; i < 5 && rowY + 32 < y + panelH; ++i) {
    drawStatusRow(renderer, Rect{kPad + kInner, rowY, contentW - kInner * 2, 32},
                  statusLabel(i), stats.byStatus[i], maxStatus);
    rowY += 42;
  }
}

void ReadingStatsActivity::renderMonth(int y, int pageWidth, int pageHeight) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentW = pageWidth - kPad * 2;
  const int innerW = contentW - kInner * 2;
  const int numDays = daysInMonth(viewYear, viewMonth);

  int total = 0;
  int activeDays = 0;
  int peakValue = 0;
  int peakDay = 1;
  for (int i = 0; i < numDays; ++i) {
    total += periodData[i];
    if (periodData[i] > 0) ++activeDays;
    if (periodData[i] > peakValue) {
      peakValue = periodData[i];
      peakDay = i + 1;
    }
  }

  char period[48];
  snprintf(period, sizeof(period), "%s %d", I18N.get(MONTH_STRIDS[viewMonth - 1]), viewYear);
  const int panelH = pageHeight - y - metrics.buttonHintsHeight - 14;
  BooksActivityUI::panel(renderer, Rect{kPad, y, contentW, panelH});
  char totalText[24];
  snprintf(totalText, sizeof(totalText), "%d %s", total, tr(STR_STATS_SESSIONS));
  drawSectionTitle(renderer, kPad + kInner, y + 18, period, totalText);

  if (total == 0) {
    drawEmpty(renderer, Rect{kPad + kInner, y + 62, innerW, 160});
    return;
  }

  const int gridX = kPad + kInner;
  const int gridY = y + 58;
  const int bigW = (innerW * 2 - kGap) / 3;
  const int sideW = innerW - bigW - kGap;
  drawKpi(renderer, Rect{gridX, gridY, bigW, 124}, tr(STR_STATS_SESSIONS),
          total, period, true);
  drawCompactStat(renderer, Rect{gridX + bigW + kGap, gridY, sideW, 56},
                  "Active", activeDays);
  char peakDetail[16];
  snprintf(peakDetail, sizeof(peakDetail), "%02d/%02d", peakDay, viewMonth);
  drawCompactStat(renderer, Rect{gridX + bigW + kGap, gridY + 68, sideW, 56},
                  peakDetail, peakValue);

  const int stripY = gridY + 124 + 18;
  drawSectionTitle(renderer, gridX, stripY, "Month activity");
  drawActivityStrip(renderer, Rect{gridX, stripY + 30, innerW, 46},
                    periodData, numDays, std::max(1, peakValue));

  const int chipsY = stripY + 96;
  drawSectionTitle(renderer, gridX, chipsY, "Active days");
  const int chipW = (innerW - kGap * 2) / 3;
  constexpr int chipH = 62;
  int drawn = 0;
  for (int day = 1; day <= numDays && drawn < 6; ++day) {
    if (periodData[day - 1] == 0) continue;
    const int col = drawn % 3;
    const int row = drawn / 3;
    drawDayChip(renderer,
                Rect{gridX + col * (chipW + kGap), chipsY + 30 + row * (chipH + kGap),
                     chipW, chipH},
                day, periodData[day - 1], periodData[day - 1] == peakValue);
    ++drawn;
  }
}

void ReadingStatsActivity::renderYear(int y, int pageWidth, int pageHeight) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentW = pageWidth - kPad * 2;
  const int innerW = contentW - kInner * 2;

  int total = 0;
  int activeMonths = 0;
  int peakValue = 0;
  int peakMonth = 0;
  for (int i = 0; i < 12; ++i) {
    total += yearMonthData[i];
    if (yearMonthData[i] > 0) ++activeMonths;
    if (yearMonthData[i] > peakValue) {
      peakValue = yearMonthData[i];
      peakMonth = i;
    }
  }

  const int panelH = pageHeight - y - metrics.buttonHintsHeight - 14;
  BooksActivityUI::panel(renderer, Rect{kPad, y, contentW, panelH});
  char yearText[12];
  snprintf(yearText, sizeof(yearText), "%d", viewYear);
  char totalText[24];
  snprintf(totalText, sizeof(totalText), "%d %s", total, tr(STR_STATS_SESSIONS));
  drawSectionTitle(renderer, kPad + kInner, y + 18, yearText, totalText);

  if (total == 0) {
    drawEmpty(renderer, Rect{kPad + kInner, y + 62, innerW, 160});
    return;
  }

  const int gridX = kPad + kInner;
  const int topY = y + 58;
  const int summaryH = 112;
  BooksActivityUI::panel(renderer, Rect{gridX, topY, innerW, summaryH}, true);

  char totalValue[16];
  snprintf(totalValue, sizeof(totalValue), "%d", total);
  renderer.drawText(UI_12_FONT_ID, gridX + kInner, topY + 22, totalValue,
                    true, EpdFontFamily::BOLD);
  BooksActivityUI::text(renderer, SMALL_FONT_ID, gridX + kInner, topY + 66,
               tr(STR_STATS_SESSIONS), innerW / 2 - kInner * 2, EpdFontFamily::BOLD);

  const int splitX = gridX + innerW / 2 + 8;
  const int rightX = splitX + kInner;
  const int rightW = gridX + innerW - kInner - rightX;
  renderer.drawLine(splitX, topY + 18, splitX, topY + summaryH - 18);
  renderer.drawLine(rightX, topY + summaryH / 2, rightX + rightW, topY + summaryH / 2);
  drawInlineStat(renderer, Rect{rightX, topY + 14, rightW, 42}, "Months", activeMonths);
  drawInlineStat(renderer, Rect{rightX, topY + 70, rightW, 42},
                 I18N.get(MONTH_SHORT_STRIDS[peakMonth]), peakValue);

  const int monthsY = topY + summaryH + 18;
  drawSectionTitle(renderer, gridX, monthsY, "Year map");
  const int tileW = (innerW - kGap * 2) / 3;
  const int tileH = 64;
  for (int i = 0; i < 12; ++i) {
    const int col = i % 3;
    const int row = i / 3;
    drawMonthCell(renderer,
                  Rect{gridX + col * (tileW + kGap), monthsY + 30 + row * (tileH + kGap),
                       tileW, tileH},
                  I18N.get(MONTH_SHORT_STRIDS[i]), yearMonthData[i],
                  std::max(1, peakValue));
  }
}

void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});

  char title[48];
  char subtitle[48] = "";
  if (currentView == View::Overview) {
    snprintf(title, sizeof(title), "Overview");
    snprintf(subtitle, sizeof(subtitle), "%d books - %d readings",
             stats.totalBooks, stats.totalReadings);
  } else if (currentView == View::Month) {
    snprintf(title, sizeof(title), "%s %d", I18N.get(MONTH_STRIDS[viewMonth - 1]), viewYear);
    snprintf(subtitle, sizeof(subtitle), "Daily reading activity");
  } else {
    snprintf(title, sizeof(title), "%d", viewYear);
    snprintf(subtitle, sizeof(subtitle), "Annual reading map");
  }

  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  drawHero(renderer, Rect{kPad, heroY, pageWidth - kPad * 2, kHeroH},
           title, subtitle, static_cast<int>(currentView));

  const int contentY = heroY + kHeroH + 14;
  switch (currentView) {
    case View::Overview: renderOverview(contentY, pageWidth, pageHeight); break;
    case View::Month: renderMonth(contentY, pageWidth, pageHeight); break;
    case View::Year: renderYear(contentY, pageWidth, pageHeight); break;
  }

  const bool hasPeriod = currentView != View::Overview;
  const auto btnLabels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_STATS_CHANGE_VIEW),
                                               hasPeriod ? tr(STR_PREV) : "",
                                               hasPeriod ? tr(STR_NEXT) : "");
  GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);
  renderer.displayBuffer();
}
