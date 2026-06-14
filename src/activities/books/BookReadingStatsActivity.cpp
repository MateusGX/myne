#include "BookReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "BooksActivityUI.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kPad = 20;
constexpr int kCardR = 8;
constexpr int kInner = 16;

void drawCard(const GfxRenderer& renderer, Rect r, bool gray = false) {
  if (gray) renderer.fillRoundedRect(r.x, r.y, r.width, r.height, kCardR, Color::LightGray);
  renderer.drawRoundedRect(r.x, r.y, r.width, r.height, 1, kCardR, true);
}

void drawPillRight(const GfxRenderer& renderer, Rect r, const char* text, bool filled = true) {
  const int lh = renderer.getLineHeight(SMALL_FONT_ID);
  const int tw = renderer.getTextWidth(SMALL_FONT_ID, text, EpdFontFamily::BOLD);
  const int pw = std::min(r.width, tw + 20);
  const int x = r.x + r.width - pw;
  if (filled) {
    renderer.fillRoundedRect(x, r.y, pw, lh + 10, 5, Color::Black);
    renderer.drawText(SMALL_FONT_ID, x + (pw - tw) / 2, r.y + 5, text, false, EpdFontFamily::BOLD);
  } else {
    renderer.drawRoundedRect(x, r.y, pw, lh + 10, 1, 5, true);
    renderer.drawText(SMALL_FONT_ID, x + (pw - tw) / 2, r.y + 5, text, true, EpdFontFamily::BOLD);
  }
}

void drawMetricCard(const GfxRenderer& renderer, Rect r, const char* label, const char* value,
                    const char* detail = nullptr, bool selected = false) {
  renderer.drawRoundedRect(r.x, r.y, r.width, r.height, selected ? 2 : 1, kCardR, true);
  if (selected) renderer.fillRoundedRect(r.x, r.y, 8, r.height, kCardR, true, false, true, false, Color::Black);

  const int lh12 = renderer.getLineHeight(UI_12_FONT_ID);
  renderer.drawText(SMALL_FONT_ID, r.x + kInner, r.y + 14, label, true, EpdFontFamily::BOLD);
  const auto safeValue = renderer.truncatedText(UI_12_FONT_ID, value, r.width - kInner * 2, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, r.x + kInner, r.y + 36, safeValue.c_str(), true, EpdFontFamily::BOLD);
  if (detail && detail[0]) {
    const auto safeDetail = renderer.truncatedText(SMALL_FONT_ID, detail, r.width - kInner * 2);
    const int lineY = std::max(r.y + 36 + lh12 + 8, r.y + r.height - 28);
    renderer.drawLine(r.x + kInner, lineY, r.x + r.width - kInner, lineY);
    renderer.drawText(SMALL_FONT_ID, r.x + kInner, lineY + 8, safeDetail.c_str(), true);
  }
}

void drawNumberMetric(const GfxRenderer& renderer, Rect r, const char* label, int value, const char* detail = nullptr,
                      bool selected = false) {
  char num[12];
  snprintf(num, sizeof(num), "%d", value);
  drawMetricCard(renderer, r, label, num, detail, selected);
}

const char* shortMonth(uint8_t month) {
  switch (month) {
    case 1:
      return tr(STR_MONTH_SHORT_JAN);
    case 2:
      return tr(STR_MONTH_SHORT_FEB);
    case 3:
      return tr(STR_MONTH_SHORT_MAR);
    case 4:
      return tr(STR_MONTH_SHORT_APR);
    case 5:
      return tr(STR_MONTH_SHORT_MAY);
    case 6:
      return tr(STR_MONTH_SHORT_JUN);
    case 7:
      return tr(STR_MONTH_SHORT_JUL);
    case 8:
      return tr(STR_MONTH_SHORT_AUG);
    case 9:
      return tr(STR_MONTH_SHORT_SEP);
    case 10:
      return tr(STR_MONTH_SHORT_OCT);
    case 11:
      return tr(STR_MONTH_SHORT_NOV);
    case 12:
      return tr(STR_MONTH_SHORT_DEC);
  }
  return "";
}

void formatMonthYear(char* out, size_t outLen, int16_t year, uint8_t month) {
  snprintf(out, outLen, "%s '%02d", shortMonth(month), static_cast<int>(year % 100));
}

void drawViewDots(const GfxRenderer& renderer, int centerX, int y, int selected) {
  for (int i = 0; i < 2; ++i) {
    const int x = centerX - 10 + i * 20;
    if (i == selected) {
      renderer.fillRoundedRect(x - 5, y - 3, 10, 6, 3, Color::Black);
    } else {
      renderer.drawRoundedRect(x - 3, y - 3, 6, 6, 1, 3, true);
    }
  }
}

void drawEmptyState(const GfxRenderer& renderer, Rect r) {
  drawCard(renderer, r);
  const int cx = r.x + r.width / 2;
  const int y = r.y + std::max(28, r.height / 2 - 18);
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_NO_SESSIONS), true, EpdFontFamily::BOLD);
}

void drawMonthRows(const GfxRenderer& renderer, Rect r,
                   const std::vector<BookReadingStatsActivity::MonthBucket>& buckets, int startIndex, int maxRows) {
  if (buckets.empty()) return;

  const int n = std::min(maxRows, static_cast<int>(buckets.size()) - startIndex);
  if (n <= 0) return;

  int maxVal = 1;
  for (int i = startIndex; i < startIndex + n; ++i) {
    maxVal = std::max(maxVal, static_cast<int>(buckets[static_cast<size_t>(i)].count));
  }

  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);
  const int rowGap = 8;
  const int rowH = std::min(52, std::max(38, (r.height - rowGap * (n - 1)) / n));
  const int labelW = 86;
  const int countW = 36;
  const int barX = r.x + labelW;
  const int barW = r.width - labelW - countW - 10;
  if (barW <= 12) return;

  for (int i = 0; i < n; ++i) {
    const auto& b = buckets[static_cast<size_t>(startIndex + i)];
    const int y = r.y + i * (rowH + rowGap);
    const int midY = y + (rowH - lhSm) / 2;

    char month[16];
    formatMonthYear(month, sizeof(month), b.year, b.month);
    renderer.drawText(SMALL_FONT_ID, r.x, midY, month, true, EpdFontFamily::BOLD);

    const int trackY = y + rowH / 2 - 6;
    renderer.drawRoundedRect(barX, trackY, barW, 12, 1, 6, true);
    const int fillW = std::max(8, (barW - 4) * static_cast<int>(b.count) / maxVal);
    renderer.fillRoundedRect(barX + 2, trackY + 2, fillW, 8, 4, Color::LightGray);

    char cnt[8];
    snprintf(cnt, sizeof(cnt), "%d", static_cast<int>(b.count));
    const int cw = renderer.getTextWidth(SMALL_FONT_ID, cnt, EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, r.x + r.width - cw, midY, cnt, true, EpdFontFamily::BOLD);
  }
}
}  // namespace

// ---------------------------------------------------------------------------
// Data loading
// ---------------------------------------------------------------------------

void BookReadingStatsActivity::loadData() {
  totalReadings = 0;
  totalSessions = 0;
  firstDate[0] = '\0';
  lastDate[0] = '\0';
  monthBuckets.clear();

  const auto readings = readingLog.loadForBook(book.id);
  totalReadings = static_cast<int>(readings.size());

  for (const auto& r : readings) {
    for (const auto& s : r.sessions) {
      ++totalSessions;
      if (s.date.size() < 10) continue;

      if (firstDate[0] == '\0' || s.date < firstDate) {
        strncpy(firstDate, s.date.c_str(), 10);
        firstDate[10] = '\0';
      }
      if (s.date > lastDate) {
        strncpy(lastDate, s.date.c_str(), 10);
        lastDate[10] = '\0';
      }

      int y = 0, m = 0;
      if (sscanf(s.date.c_str(), "%d-%d", &y, &m) != 2 || m < 1 || m > 12) continue;

      bool found = false;
      for (auto& e : monthBuckets) {
        if (e.year == static_cast<int16_t>(y) && e.month == static_cast<uint8_t>(m)) {
          ++e.count;
          found = true;
          break;
        }
      }
      if (!found) monthBuckets.push_back({static_cast<int16_t>(y), static_cast<uint8_t>(m), 1});
    }
  }

  std::sort(monthBuckets.begin(), monthBuckets.end(), [](const MonthBucket& a, const MonthBucket& b) {
    return a.year != b.year ? a.year < b.year : a.month < b.month;
  });
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void BookReadingStatsActivity::onEnter() {
  Activity::onEnter();
  currentView = View::Summary;
  loadData();
  requestUpdate();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void BookReadingStatsActivity::cycleView() {
  currentView = currentView == View::Summary ? View::Timeline : View::Summary;
  requestUpdate();
}

void BookReadingStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.popActivity();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      mappedInput.wasReleased(MappedInputManager::Button::Left) ||
      mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cycleView();
  }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void BookReadingStatsActivity::renderSummary(int contentTop, int W, int H) {
  const auto& m = UITheme::getInstance().getMetrics();
  const int CW = W - kPad * 2;
  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
  const std::string titleStr =
      book.volume.empty() ? book.title : book.title + " " + tr(STR_BOOK_VOLUME_SHORT) + " " + book.volume;

  int y = contentTop;

  // Hero
  {
    const Rect heroRect{kPad, y, CW, BooksActivityUI::HERO_H};
    BooksActivityUI::hero(renderer, heroRect, tr(STR_READING_STATS), titleStr.c_str(), nullptr, 46);
    drawPillRight(renderer, Rect{kPad + kInner, y + 9, CW - kInner * 2, 22}, "01");
    drawViewDots(renderer, kPad + CW - 36, y + BooksActivityUI::HERO_H - 16, 0);
    y += BooksActivityUI::HERO_H + 12;
  }

  // Metrics
  const int gap = 10;
  const int metricW = (CW - gap) / 2;
  drawNumberMetric(renderer, Rect{kPad, y, metricW, 76}, tr(STR_STATS_READINGS), totalReadings, nullptr, true);
  drawNumberMetric(renderer, Rect{kPad + metricW + gap, y, metricW, 76}, tr(STR_STATS_SESSIONS), totalSessions);
  y += 88;

  // Date range
  {
    const int infoH = 68;
    char range[44] = "-";
    if (firstDate[0] != '\0') {
      snprintf(range, sizeof(range), "%s - %s", firstDate, lastDate);
    }
    drawMetricCard(renderer, Rect{kPad, y, CW, infoH}, tr(STR_LOG_DATE), range);
    y += infoH + 14;
  }

  const int panelH = H - y - m.buttonHintsHeight - 14;
  if (monthBuckets.empty()) {
    drawEmptyState(renderer, Rect{kPad, y, CW, panelH});
    return;
  }

  drawCard(renderer, Rect{kPad, y, CW, panelH});
  renderer.drawText(SMALL_FONT_ID, kPad + kInner, y + 16, tr(STR_STATS_SESSIONS), true, EpdFontFamily::BOLD);
  char totalLabel[32];
  snprintf(totalLabel, sizeof(totalLabel), "%d %s", totalSessions, tr(STR_STATS_SESSIONS));
  const int tw = renderer.getTextWidth(SMALL_FONT_ID, totalLabel);
  renderer.drawText(SMALL_FONT_ID, kPad + CW - kInner - tw, y + 16, totalLabel, true);

  const int maxRows = std::min(5, std::max(1, (panelH - 58) / 48));
  const int start = std::max(0, static_cast<int>(monthBuckets.size()) - maxRows);
  drawMonthRows(renderer, Rect{kPad + kInner, y + 48, CW - kInner * 2, panelH - 64}, monthBuckets, start, maxRows);
}

void BookReadingStatsActivity::renderTimeline(int contentTop, int W, int H) {
  const auto& m = UITheme::getInstance().getMetrics();
  const int CW = W - kPad * 2;
  int y = contentTop;

  {
    char summary[64];
    snprintf(summary, sizeof(summary), "%d %s · %d %s", totalReadings, tr(STR_STATS_READINGS), totalSessions,
             tr(STR_STATS_SESSIONS));
    const Rect heroRect{kPad, y, CW, BooksActivityUI::HERO_H};
    BooksActivityUI::hero(renderer, heroRect, tr(STR_STATS_SESSIONS), summary, nullptr, 46);
    drawPillRight(renderer, Rect{kPad + kInner, y + 9, CW - kInner * 2, 22}, "02");
    drawViewDots(renderer, kPad + CW - 36, y + BooksActivityUI::HERO_H - 16, 1);
    y += BooksActivityUI::HERO_H + 12;
  }

  const int panelH = H - y - m.buttonHintsHeight - 14;
  if (monthBuckets.empty()) {
    drawEmptyState(renderer, Rect{kPad, y, CW, panelH});
    return;
  }

  drawCard(renderer, Rect{kPad, y, CW, panelH});
  renderer.drawText(SMALL_FONT_ID, kPad + kInner, y + 16, tr(STR_LOG_DATE), true, EpdFontFamily::BOLD);

  char range[44] = "-";
  if (firstDate[0] != '\0') {
    snprintf(range, sizeof(range), "%s - %s", firstDate, lastDate);
  }
  const int rw = renderer.getTextWidth(SMALL_FONT_ID, range);
  renderer.drawText(SMALL_FONT_ID, kPad + CW - kInner - rw, y + 16, range, true);

  const int rowsTop = y + 58;
  const int rowsH = panelH - 78;
  const int maxRows = std::min(8, std::max(1, rowsH / 50));
  const int start = std::max(0, static_cast<int>(monthBuckets.size()) - maxRows);
  drawMonthRows(renderer, Rect{kPad + kInner, rowsTop, CW - kInner * 2, rowsH}, monthBuckets, start, maxRows);
}

void BookReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const auto& m = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, m.topPadding, W, m.headerHeight});
  const int contentTop = m.topPadding + m.headerHeight + 8;

  if (currentView == View::Summary) {
    renderSummary(contentTop, W, H);
  } else {
    renderTimeline(contentTop, W, H);
  }

  const auto btnLabels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_STATS_CHANGE_VIEW), tr(STR_PREV), tr(STR_NEXT));
  GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);

  renderer.displayBuffer();
}
