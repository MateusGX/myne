#include "LetterBooksActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdlib>

#include "BookListUI.h"
#include "BooksActivityUI.h"
#include "CollectionBooksActivity.h"
#include "MappedInputManager.h"
#include "PhysicalBookDetailActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

// ── Page buffer ───────────────────────────────────────────────────────────────

void LetterBooksActivity::ensureBuf() {
  if (!buf_ || pageItems_ <= 0) return;
  const int wantStart = (selIdx_ / pageItems_) * pageItems_;
  if (wantStart == bufStart_ && bufCount_ > 0) return;
  bufStart_ = wantStart;
  bufCount_ = BookCatalog::readLetterPage(letter_, bufStart_, pageItems_, buf_);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void LetterBooksActivity::onEnter() {
  Activity::onEnter();

  pageItems_ = BookListUI::pageItemsForLetter(renderer);
  totalCount_ = BookCatalog::letterCount(letter_);
  selIdx_ = 0;
  bufStart_ = -1;
  bufCount_ = 0;

  if (pageItems_ > 0)
    buf_ = static_cast<BookCatalog::Entry*>(malloc(static_cast<size_t>(pageItems_) * sizeof(BookCatalog::Entry)));

  requestUpdate();
}

void LetterBooksActivity::onExit() {
  free(buf_);
  buf_ = nullptr;
  bufStart_ = -1;
  bufCount_ = 0;
  Activity::onExit();
}

// ── Actions ───────────────────────────────────────────────────────────────────

void LetterBooksActivity::openSelected() {
  if (!buf_ || totalCount_ == 0) return;
  const int bi = selIdx_ - bufStart_;
  if (bi < 0 || bi >= bufCount_) return;

  const auto& e = buf_[bi];

  if (e.isCollection) {
    startActivityForResult(std::make_unique<CollectionBooksActivity>(renderer, mappedInput, e.id, e.count, e.title),
                           [this](const ActivityResult&) { requestUpdate(); });
    return;
  }

  PhysicalBook book;
  BookStore bookStore;
  const bool found = bookStore.get(e.id, book) && !book.title.empty();

  if (found) {
    startActivityForResult(std::make_unique<PhysicalBookDetailActivity>(renderer, mappedInput, std::move(book)),
                           [this](const ActivityResult&) { requestUpdate(); });
  }
}

// ── Input ─────────────────────────────────────────────────────────────────────

void LetterBooksActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.popActivity();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && totalCount_ > 0) {
    openSelected();
    return;
  }
  if (totalCount_ == 0) return;

  const int pageItems = pageItems_;
  buttonNavigator.onNextRelease([this] {
    selIdx_ = ButtonNavigator::nextIndex(selIdx_, totalCount_);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    selIdx_ = ButtonNavigator::previousIndex(selIdx_, totalCount_);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, pageItems] {
    selIdx_ = ButtonNavigator::nextPageIndex(selIdx_, totalCount_, pageItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, pageItems] {
    selIdx_ = ButtonNavigator::previousPageIndex(selIdx_, totalCount_, pageItems);
    requestUpdate();
  });
}

// ── Render ────────────────────────────────────────────────────────────────────

void LetterBooksActivity::render(RenderLock&&) {
  ensureBuf();

  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});

  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  char letterTitle[2] = {letter_, '\0'};
  char detail[40];
  snprintf(detail, sizeof(detail), "%d %s", totalCount_,
           totalCount_ == 1 ? tr(STR_ENTRY_SINGULAR) : tr(STR_ENTRY_PLURAL));
  BooksActivityUI::hero(
      renderer, Rect{BooksActivityUI::PAD, heroY, pageWidth - BooksActivityUI::PAD * 2, BooksActivityUI::HERO_H},
      tr(STR_PHYSICAL_BOOKS), letterTitle, totalCount_ > 0 ? detail : nullptr);

  const int contentTop = heroY + BooksActivityUI::HERO_H + 18;

  if (totalCount_ == 0) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_BOOKS));
  } else {
    for (int i = bufStart_; i < bufStart_ + bufCount_; ++i) {
      const int bi = i - bufStart_;
      const int ry = contentTop + bi * BookListUI::kRowHeight;
      BookListUI::drawEntryRow(renderer,
                               Rect{BookListUI::kPad, ry, pageWidth - BookListUI::kPad * 2, BookListUI::kRowHeight - 4},
                               buf_[bi], i + 1, i == selIdx_);
    }
  }

  const bool onCollection = totalCount_ > 0 && buf_ && bufStart_ >= 0 && (selIdx_ - bufStart_) >= 0 &&
                            (selIdx_ - bufStart_) < bufCount_ && buf_[selIdx_ - bufStart_].isCollection;
  const char* confirmHint = totalCount_ == 0 ? "" : (onCollection ? tr(STR_SELECT) : tr(STR_OPEN));
  const auto btnLabels = mappedInput.mapLabels(tr(STR_BACK), confirmHint, "", "");
  GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);
  renderer.displayBuffer();
}
