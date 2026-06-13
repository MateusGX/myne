#include "BookReadingsActivity.h"

#include <BookStore.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "BooksActivityUI.h"
#include "MappedInputManager.h"
#include "ReadingEditActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

static const char* statusLabel(ReadingStatus s);

namespace {
constexpr int kReadRowH = 108;
constexpr int kCardR    = 8;
constexpr int kPad      = BooksActivityUI::PAD;

void drawEmptyState(const GfxRenderer& renderer, Rect rect) {
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, kCardR, true);
  const int cx = rect.x + rect.width / 2;
  const int titleY = rect.y + std::max(28, rect.height / 2 - 28);
  renderer.drawCenteredText(UI_10_FONT_ID, titleY, tr(STR_NO_READINGS), true,
                            EpdFontFamily::BOLD);
}

void drawReadingRow(const GfxRenderer& renderer, Rect row, const Reading& r, int itemNumber,
                    bool selected) {
  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);

  if (selected) renderer.fillRoundedRect(row.x, row.y, row.width, row.height, kCardR, Color::LightGray);
  renderer.drawRoundedRect(row.x, row.y, row.width, row.height, selected ? 2 : 1, kCardR, true);
  if (selected) {
    renderer.fillRoundedRect(row.x, row.y, 8, row.height, kCardR, true, false, true, false, Color::Black);
  }

  char num[4];
  snprintf(num, sizeof(num), "%02d", itemNumber);
  const int numW = renderer.getTextWidth(UI_10_FONT_ID, num, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, row.x + 26 - numW / 2, row.y + 20, num, true,
                    EpdFontFamily::BOLD);
  renderer.drawLine(row.x + 54, row.y + 16, row.x + 54, row.y + row.height - 16);

  const int textX = row.x + 72;
  const int textW = row.width - (textX - row.x) - 16;
  auto title = renderer.truncatedText(UI_10_FONT_ID, statusLabel(r.status), textW,
                                      EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, textX, row.y + 16, title.c_str(), true,
                    EpdFontFamily::BOLD);

  char sub[32];
  if (r.sessions.empty()) {
    snprintf(sub, sizeof(sub), "%s", tr(STR_NO_SESSIONS));
  } else {
    const char* unit = (r.readingType == ReadingType::Chapter) ? "ch." : "p.";
    snprintf(sub, sizeof(sub), "%s %d", unit, r.lastPosition());
  }
  renderer.drawText(SMALL_FONT_ID, textX, row.y + 16 + lh10 + 8, sub, true);

  const auto date = renderer.truncatedText(SMALL_FONT_ID,
                                           r.lastDate().empty() ? "-" : r.lastDate().c_str(),
                                           textW / 2);
  const int dw = renderer.getTextWidth(SMALL_FONT_ID, date.c_str());
  renderer.drawText(SMALL_FONT_ID, row.x + row.width - 16 - dw,
                    row.y + row.height - lhSm - 16, date.c_str(), true);
}
}  // namespace

static void renderStatsProgress(void* raw, int done, int total) {
  if (total > 0 && done > 0 && done < total) {
    if ((done * 10 / total) == ((done - 1) * 10 / total)) return;
  }
  auto& r     = *static_cast<GfxRenderer*>(raw);
  const int W = r.getScreenWidth();
  const int H = r.getScreenHeight();

  r.clearScreen();

  r.drawCenteredText(UI_10_FONT_ID, H / 2 - 48, tr(STR_UPDATING));

  constexpr int BAR_W = 320, BAR_H = 14, BAR_R = 7;
  const int bx = (W - BAR_W) / 2;
  const int by = H / 2 - BAR_H / 2;
  r.drawRoundedRect(bx, by, BAR_W, BAR_H, 1, BAR_R, true);
  if (total > 0 && done > 0) {
    const int fill = (BAR_W - 2) * std::min(done, total) / total;
    if (fill > 0) r.fillRect(bx + 1, by + 1, fill, BAR_H - 2, Color::Black);
  }

  char cnt[16];
  snprintf(cnt, sizeof(cnt), "%d / %d", done, total);
  const int cw = r.getTextWidth(SMALL_FONT_ID, cnt);
  r.drawText(SMALL_FONT_ID, (W - cw) / 2, H / 2 + 24, cnt, true);

  r.displayBuffer();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char* statusLabel(ReadingStatus s) {
  switch (s) {
    case ReadingStatus::WantToRead: return tr(STR_STATUS_WANT_TO_READ);
    case ReadingStatus::Reading:    return tr(STR_STATUS_READING);
    case ReadingStatus::Paused:     return tr(STR_STATUS_PAUSED);
    case ReadingStatus::Finished:   return tr(STR_STATUS_FINISHED);
    case ReadingStatus::Dropped:    return tr(STR_STATUS_DROPPED);
  }
  return "";
}

// ── Data ──────────────────────────────────────────────────────────────────────

void BookReadingsActivity::loadReadings() {
  readings = readingLog.loadForBook(book.id);
}

// ── Actions ───────────────────────────────────────────────────────────────────

void BookReadingsActivity::openSelected() {
  if (readings.empty()) return;
  const Reading& r = readings[static_cast<size_t>(selectorIndex)];
  startActivityForResult(
      std::make_unique<ReadingEditActivity>(renderer, mappedInput, book, r),
      [this](const ActivityResult&) {
        loadReadings();
        requestUpdate();
      });
}

void BookReadingsActivity::createNew() {
  Reading r;
  r.id = ReadingLog::newId();
  r.status = ReadingStatus::Reading;
  startActivityForResult(
      std::make_unique<ReadingEditActivity>(renderer, mappedInput, book, std::move(r)),
      [this](const ActivityResult&) {
        loadReadings();
        if (selectorIndex >= static_cast<int>(readings.size()) && selectorIndex > 0) {
          selectorIndex = static_cast<int>(readings.size()) - 1;
        }
        requestUpdate();
      });
}

void BookReadingsActivity::deleteSelected() {
  if (readings.empty()) return;
  readings.erase(readings.begin() + selectorIndex);
  if (selectorIndex >= static_cast<int>(readings.size()) && selectorIndex > 0) {
    --selectorIndex;
  }
  readingLog.saveForBook(book.id, readings);
  renderStatsProgress(&renderer, 0, 0);  // show screen immediately, total resolved inside rebuildStats
  readingLog.rebuildStats(BookStore::DIR_PATH, renderStatsProgress, &renderer);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void BookReadingsActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  confirmingDelete = false;
  loadReadings();
  requestUpdate();
}

// ── Input ─────────────────────────────────────────────────────────────────────

void BookReadingsActivity::loop() {
  // ── Delete confirmation mode ─────────────────────────────────────────────
  if (confirmingDelete) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      confirmingDelete = false;
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      deleteSelected();
      confirmingDelete = false;
      requestUpdate();
    }
    return;
  }

  // ── Normal mode ───────────────────────────────────────────────────────────
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.popActivity();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && readings.empty()) {
    activityManager.popActivity();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && !readings.empty()) {
    openSelected();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    createNew();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) && !readings.empty()) {
    confirmingDelete = true;
    requestUpdate();
    return;
  }
   if (mappedInput.wasReleased(MappedInputManager::Button::Right) && readings.empty()) {
    createNew();
    return;
  }

  if (readings.empty()) return;

  const int total = static_cast<int>(readings.size());
  buttonNavigator.onNextRelease([this, total] {
    selectorIndex = (selectorIndex + 1) % total;
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, total] {
    selectorIndex = (selectorIndex + total - 1) % total;
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, total] {
    selectorIndex = (selectorIndex + 1) % total;
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, total] {
    selectorIndex = (selectorIndex + total - 1) % total;
    requestUpdate();
  });
}

// ── Render ────────────────────────────────────────────────────────────────────

void BookReadingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const std::string titleStr = book.volume.empty()
      ? book.title
      : book.title + " " + tr(STR_BOOK_VOLUME_SHORT) + " " + book.volume;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});

  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  char detail[40];
  snprintf(detail, sizeof(detail), "%d %s", static_cast<int>(readings.size()), tr(STR_READINGS));
  BooksActivityUI::hero(renderer,
                        Rect{BooksActivityUI::PAD, heroY,
                             pageWidth - BooksActivityUI::PAD * 2, BooksActivityUI::HERO_H},
                        tr(STR_READINGS), titleStr.c_str(), detail);

  const int contentTop = heroY + BooksActivityUI::HERO_H + 16;
  const int contentH   = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (readings.empty()) {
    const int emptyH = std::min(188, std::max(150, contentH - 20));
    drawEmptyState(renderer, Rect{kPad, contentTop, pageWidth - kPad * 2, emptyH});
  } else {
    const int total = static_cast<int>(readings.size());
    const int pageItems = std::max(1, contentH / kReadRowH);
    const int pageStart = (selectorIndex / pageItems) * pageItems;
    for (int i = pageStart; i < total && i < pageStart + pageItems; ++i) {
      const int ry = contentTop + (i - pageStart) * kReadRowH;
      drawReadingRow(renderer,
                     Rect{kPad, ry, pageWidth - kPad * 2, kReadRowH - 4},
                     readings[static_cast<size_t>(i)], i + 1, i == selectorIndex);
    }
  }

  // Button hints change during delete confirmation
  if (confirmingDelete) {
    GUI.drawPopup(renderer, tr(STR_DELETE_READING_CONFIRM));
    const auto btnLabels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_DELETE), "", "");
    GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);
  } else {
    const bool hasItem = !readings.empty();
    const auto btnLabels = mappedInput.mapLabels(
        tr(STR_BACK),
        hasItem ? tr(STR_SELECT) : "",
        tr(STR_NEW_READING),
        hasItem ? tr(STR_DELETE) : "");
    GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);
  }

  renderer.displayBuffer();
}
