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
#include <string>
#include <vector>

#include "BooksActivityUI.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
void drawActivityBar(const GfxRenderer& renderer, Rect rect, int processedCount) {
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, rect.height / 2, true);

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

void drawCenteredState(const GfxRenderer& renderer, Rect rect, const char* title, const char* detail,
                       bool selected = false) {
  BooksActivityUI::panel(renderer, rect, selected);
  const int titleH = renderer.getLineHeight(UI_10_FONT_ID);
  const int detailH = renderer.getLineHeight(SMALL_FONT_ID);
  std::vector<std::string> detailLines;
  if (detail && detail[0] != '\0') {
    detailLines = renderer.wrappedText(SMALL_FONT_ID, detail, rect.width - BooksActivityUI::INNER * 2, 2);
  }
  const int contentH = titleH + (detailLines.empty() ? 0 : 12 + static_cast<int>(detailLines.size()) * detailH);
  int y = rect.y + rect.height / 2 - contentH / 2;
  renderer.drawCenteredText(UI_10_FONT_ID, y, title, true, EpdFontFamily::BOLD);
  y += titleH + 12;
  for (const auto& line : detailLines) {
    renderer.drawCenteredText(SMALL_FONT_ID, y, line.c_str());
    y += detailH;
  }
}

void drawSyncingCard(const GfxRenderer& renderer, Rect rect, int processedCount) {
  BooksActivityUI::panel(renderer, rect, true);
  const int x = rect.x + BooksActivityUI::INNER + 8;
  const int w = rect.width - BooksActivityUI::INNER * 2 - 8;

  BooksActivityUI::text(renderer, UI_10_FONT_ID, x, rect.y + 26, tr(STR_SYNCING_BOOKS), w, EpdFontFamily::BOLD);

  char count[48];
  std::snprintf(count, sizeof(count), "%d", processedCount);
  renderer.drawCenteredText(UI_12_FONT_ID, rect.y + 84, count, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, rect.y + 126, tr(STR_BOOKS_PROCESSED));

  const int barH = 16;
  const int barY = rect.y + rect.height - BooksActivityUI::INNER - barH;
  drawActivityBar(renderer, Rect{x, barY, w, barH}, processedCount);
}

std::string processedText(int processedCount) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%d %s", processedCount, tr(STR_BOOKS_PROCESSED));
  return buf;
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

  const int contentY = heroY + BooksActivityUI::HERO_H + BooksActivityUI::GAP + 8;
  const int contentBottom = H - metrics.buttonHintsHeight - 16;
  const int cardW = W - BooksActivityUI::PAD * 2;
  const int cardH = contentBottom > contentY ? contentBottom - contentY : 190;
  const Rect card{BooksActivityUI::PAD, contentY, cardW, cardH};

  switch (state_) {
    case State::SYNCING: {
      drawSyncingCard(renderer, card, processedCount_);
      break;
    }

    case State::DONE: {
      const auto detail = processedText(processedCount_);
      drawCenteredState(renderer, card, tr(STR_SYNC_COMPLETE), detail.c_str());
      break;
    }

    case State::FAILED: {
      const auto detail = processedText(processedCount_);
      drawCenteredState(renderer, card, tr(STR_SYNC_FAILED), detail.c_str());
      {
        const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
        GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      }
      break;
    }
  }

  renderer.displayBuffer();
}
