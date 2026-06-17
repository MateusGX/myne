#include "CatalogSyncActivity.h"

#include <Arduino.h>
#include <BookCatalog.h>
#include <BookStore.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <ReadingLog.h>
#include <esp_task_wdt.h>

#include <algorithm>
#include <cstdio>

#include "BooksActivityUI.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "components/icons/Icons.h"
#include "fontIds.h"

namespace {
constexpr int kPad = 20;
constexpr int kCardR = 8;
constexpr int kInner = 20;

void drawPanel(const GfxRenderer& renderer, Rect rect, bool gray = true) {
  if (gray) {
    renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, kCardR, Color::LightGray);
  }
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, kCardR, true);
}

void drawPill(const GfxRenderer& renderer, Rect rect, const char* label, bool filled) {
  const int lh = renderer.getLineHeight(SMALL_FONT_ID);
  const auto safe = renderer.truncatedText(SMALL_FONT_ID, label, rect.width - 18, EpdFontFamily::BOLD);
  const int tw = renderer.getTextWidth(SMALL_FONT_ID, safe.c_str(), EpdFontFamily::BOLD);
  const int pillW = std::min(rect.width, tw + 18);
  const int pillX = rect.x + rect.width - pillW;

  if (filled) {
    renderer.fillRoundedRect(pillX, rect.y, pillW, lh + 10, 5, Color::Black);
    renderer.drawText(SMALL_FONT_ID, pillX + (pillW - tw) / 2, rect.y + 5, safe.c_str(), false, EpdFontFamily::BOLD);
  } else {
    renderer.drawRoundedRect(pillX, rect.y, pillW, lh + 10, 1, 5, true);
    renderer.drawText(SMALL_FONT_ID, pillX + (pillW - tw) / 2, rect.y + 5, safe.c_str(), true, EpdFontFamily::BOLD);
  }
}

void drawActivityBar(const GfxRenderer& renderer, Rect rect, int processedCount, bool complete) {
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, rect.height / 2, true);

  if (complete) {
    renderer.fillRoundedRect(rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4, std::max(1, rect.height / 2 - 2),
                             Color::Black);
    return;
  }

  constexpr int dashCount = 16;
  const int innerX = rect.x + 4;
  const int innerW = rect.width - 8;
  const int step = std::max(1, innerW / dashCount);
  for (int i = 0; i < dashCount; ++i) {
    if ((processedCount + i) % 4 == 0 || (processedCount + i) % 4 == 1) {
      const int x = innerX + i * step;
      renderer.fillRoundedRect(x, rect.y + 3, std::max(3, step - 4), rect.height - 6, 2, Color::Black);
    }
  }
}

void drawBookIcon(const GfxRenderer& renderer, int x, int y) {
  constexpr int iconSize = 64;
  if (const uint8_t* bmp = iconForName(UIIcon::LibraryBigIcon, iconSize)) {
    renderer.drawIcon(bmp, x, y, iconSize, iconSize);
    return;
  }
  renderer.drawRoundedRect(x + 8, y + 4, 48, 56, 1, 6, true);
  renderer.drawLine(x + 20, y + 16, x + 42, y + 16, 2, true);
  renderer.drawLine(x + 20, y + 30, x + 38, y + 30, 2, true);
}
}  // namespace

void CatalogSyncActivity::onEnter() {
  Activity::onEnter();
  {
    RenderLock lock(*this);
    state_ = State::SYNCING;
    processedCount_ = 0;
    lastRenderedCount = -1;
  }
  requestUpdateAndWait();  // Show "Rebuilding catalog..." before blocking
  doSync();
  // state_ is now DONE or FAILED; show result screen
  {
    RenderLock lock(*this);
    lastRenderedCount = -1;  // Force render even if count didn't change
  }
  requestUpdateAndWait();
  doneAt_ = millis();
}

void CatalogSyncActivity::doSync() {
  auto progressCb = +[](int processed, void* ctx) {
    auto* self = static_cast<CatalogSyncActivity*>(ctx);
    self->processedCount_ = processed;
    self->requestUpdate(true);
    esp_task_wdt_reset();
  };

  const bool ok = BookCatalog::rebuild(BookStore::DIR_PATH, progressCb, this);
  if (ok) {
    ReadingLog{}.rebuildStats(BookStore::DIR_PATH);
    Storage.remove(BookCatalog::SYNC_FLAG_PATH);
  }

  RenderLock lock(*this);
  state_ = ok ? State::DONE : State::FAILED;
}

void CatalogSyncActivity::loop() {
  if (state_ == State::DONE) {
    if (millis() - doneAt_ > 1500 || mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }
  if (state_ == State::FAILED) {
    if (mappedInput.wasReleasedGroup(MappedInputManager::ButtonGroup::BottomLeft)) {
      onGoHome();
    }
  }
}

void CatalogSyncActivity::render(RenderLock&&) {
  if (state_ == State::SYNCING) {
    if (processedCount_ == lastRenderedCount) return;
    lastRenderedCount = processedCount_;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, W, metrics.headerHeight});

  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  BooksActivityUI::hero(renderer,
                        Rect{BooksActivityUI::PAD, heroY, W - BooksActivityUI::PAD * 2, BooksActivityUI::HERO_H},
                        tr(STR_PHYSICAL_BOOKS), tr(STR_CATALOG_SYNC));

  const int titleTop = heroY + BooksActivityUI::HERO_H + 14;
  const int cardW = W - kPad * 2;
  const int cardH = std::min(330, H - titleTop - metrics.buttonHintsHeight - 34);
  const int cardX = kPad;
  const int cardY = titleTop;
  const int iconSize = 64;
  const int iconX = cardX + kInner;
  const int iconY = cardY + 36;
  const int textX = iconX + iconSize + 22;
  const int textW = cardW - (textX - cardX) - kInner;
  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);
  const int barX = cardX + kInner;
  const int barW = cardW - kInner * 2;
  const int barH = 16;
  const int barY = cardY + cardH - 82;

  drawPanel(renderer, Rect{cardX, cardY, cardW, cardH});
  drawBookIcon(renderer, iconX, iconY);

  switch (state_) {
    case State::SYNCING: {
      drawPill(renderer, Rect{textX, cardY + 26, textW, lhSm + 10}, tr(STR_SYNCING_BOOKS), true);

      const auto title = renderer.truncatedText(UI_10_FONT_ID, tr(STR_SYNCING_BOOKS), textW, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, textX, cardY + 70, title.c_str(), true, EpdFontFamily::BOLD);

      char count[48];
      snprintf(count, sizeof(count), "%d", processedCount_);
      renderer.drawText(UI_12_FONT_ID, textX, cardY + 70 + lh10 + 18, count, true, EpdFontFamily::BOLD);

      const int countW = renderer.getTextWidth(UI_12_FONT_ID, count, EpdFontFamily::BOLD);
      const auto label =
          renderer.truncatedText(SMALL_FONT_ID, tr(STR_BOOKS_PROCESSED), std::max(20, textW - countW - 12));
      renderer.drawText(SMALL_FONT_ID, textX + countW + 12, cardY + 75 + lh10 + 18, label.c_str(), true);

      renderer.drawLine(cardX + kInner, barY - 22, cardX + cardW - kInner, barY - 22);
      drawActivityBar(renderer, Rect{barX, barY, barW, barH}, processedCount_, false);
      renderer.drawText(SMALL_FONT_ID, barX, barY + barH + 14, tr(STR_CATALOG_SYNC), true, EpdFontFamily::BOLD);
      break;
    }

    case State::DONE: {
      renderer.fillRoundedRect(cardX, cardY, cardW, 12, kCardR, true, true, false, false, Color::Black);
      drawPill(renderer, Rect{textX, cardY + 26, textW, lhSm + 10}, tr(STR_SYNC_COMPLETE), true);

      const auto title = renderer.truncatedText(UI_10_FONT_ID, tr(STR_SYNC_COMPLETE), textW, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, textX, cardY + 70, title.c_str(), true, EpdFontFamily::BOLD);

      char buf[48];
      snprintf(buf, sizeof(buf), "%d %s", processedCount_, tr(STR_BOOKS_PROCESSED));
      const auto safe = renderer.truncatedText(UI_10_FONT_ID, buf, textW);
      renderer.drawText(UI_10_FONT_ID, textX, cardY + 70 + lh10 + 18, safe.c_str(), true);

      renderer.drawLine(cardX + kInner, barY - 22, cardX + cardW - kInner, barY - 22);
      drawActivityBar(renderer, Rect{barX, barY, barW, barH}, processedCount_, true);
      renderer.drawText(SMALL_FONT_ID, barX, barY + barH + 14, tr(STR_SYNC_COMPLETE), true, EpdFontFamily::BOLD);
      break;
    }

    case State::FAILED: {
      drawPill(renderer, Rect{textX, cardY + 26, textW, lhSm + 10}, tr(STR_SYNC_FAILED), false);

      const auto title = renderer.truncatedText(UI_10_FONT_ID, tr(STR_SYNC_FAILED), textW, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, textX, cardY + 70, title.c_str(), true, EpdFontFamily::BOLD);

      char buf[48];
      snprintf(buf, sizeof(buf), "%d %s", processedCount_, tr(STR_BOOKS_PROCESSED));
      const auto safe = renderer.truncatedText(UI_10_FONT_ID, buf, textW);
      renderer.drawText(UI_10_FONT_ID, textX, cardY + 70 + lh10 + 18, safe.c_str(), true);

      renderer.drawLine(cardX + kInner, barY - 22, cardX + cardW - kInner, barY - 22);
      renderer.drawRoundedRect(barX, barY, barW, barH, 1, barH / 2, true);
      renderer.drawText(SMALL_FONT_ID, barX, barY + barH + 14, tr(STR_BACK), true, EpdFontFamily::BOLD);
      {
        const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
        GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      }
      break;
    }
  }

  renderer.displayBuffer();
}
