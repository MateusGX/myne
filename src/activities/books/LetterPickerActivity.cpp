#include "LetterPickerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "BookListUI.h"
#include "BooksActivityUI.h"
#include "LetterBooksActivity.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void LetterPickerActivity::loadLetters() {
  letterCount = 0;
  selectorIndex = 0;

  uint16_t idx[27] = {};
  if (!BookCatalog::readLetterIndex(idx)) return;

  for (int i = 0; i < 27; ++i) {
    if (idx[i] == 0) continue;
    const char letter = (i < 26) ? static_cast<char>('A' + i) : '#';
    letters[letterCount++] = {letter, static_cast<int>(idx[i])};
  }
}

void LetterPickerActivity::openSelected() {
  if (letterCount == 0) return;
  const char letter = letters[selectorIndex].letter;
  startActivityForResult(std::make_unique<LetterBooksActivity>(renderer, mappedInput, letter),
                         [this](const ActivityResult&) { requestUpdate(); });
}

void LetterPickerActivity::onEnter() {
  Activity::onEnter();
  loadLetters();
  requestUpdate();
}

void LetterPickerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && letterCount > 0) {
    openSelected();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && letterCount <= 0) {
    onGoHome();
    return;
  }
  if (letterCount == 0) return;

  const int pageItems = BookListUI::pageItemsForSections(renderer);
  buttonNavigator.onNextRelease([this] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, letterCount);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, letterCount);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(selectorIndex, letterCount, pageItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(selectorIndex, letterCount, pageItems);
    requestUpdate();
  });
}

void LetterPickerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});

  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  char detail[40];
  snprintf(detail, sizeof(detail), "%d %s", letterCount,
           letterCount == 1 ? tr(STR_SECTION_SINGULAR) : tr(STR_SECTION_PLURAL));
  BooksActivityUI::hero(
      renderer, Rect{BooksActivityUI::PAD, heroY, pageWidth - BooksActivityUI::PAD * 2, BooksActivityUI::HERO_H},
      tr(STR_PHYSICAL_BOOKS), "A-Z", letterCount > 0 ? detail : nullptr);

  const int contentTop = heroY + BooksActivityUI::HERO_H + 18;

  if (letterCount == 0) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_BOOKS));
  } else {
    const int pageItems = BookListUI::pageItemsForSections(renderer);
    const int pageStart = (selectorIndex / pageItems) * pageItems;
    for (int i = pageStart; i < letterCount && i < pageStart + pageItems; ++i) {
      const int ry = contentTop + (i - pageStart) * BookListUI::kSectionRowHeight;
      BookListUI::drawSectionRow(
          renderer, Rect{BookListUI::kPad, ry, pageWidth - BookListUI::kPad * 2, BookListUI::kSectionRowHeight - 4},
          letters[i].letter, letters[i].count, i + 1, i == selectorIndex);
    }
  }

  const auto btnLabels = mappedInput.mapLabels(tr(STR_HOME), letterCount > 0 ? tr(STR_SELECT) : "", "", "");
  GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);
  renderer.displayBuffer();
}
