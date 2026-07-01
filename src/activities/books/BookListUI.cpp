#include "BookListUI.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "components/UITheme.h"
#include "fontIds.h"

namespace BookListUI {
namespace {
constexpr int kCardR = 7;
constexpr int kChevronSize = 24;
}  // namespace

int pageItemsForLetter(const GfxRenderer& renderer) {
  const auto& m = UITheme::getInstance().getMetrics();
  const int top = m.topPadding + m.headerHeight + 8 + 104 + 18;
  const int h = renderer.getScreenHeight() - top - m.buttonHintsHeight - m.verticalSpacing;
  return std::max(1, h / kRowHeight);
}

int pageItemsForSections(const GfxRenderer& renderer) {
  const auto& m = UITheme::getInstance().getMetrics();
  const int top = m.topPadding + m.headerHeight + 8 + 104 + 18;
  const int h = renderer.getScreenHeight() - top - m.buttonHintsHeight - m.verticalSpacing;
  return std::max(1, h / kSectionRowHeight);
}

int pageItemsForCollection(const GfxRenderer& renderer) {
  const auto& m = UITheme::getInstance().getMetrics();
  const int top = m.topPadding + m.headerHeight + 8 + 104 + 18;
  const int h = renderer.getScreenHeight() - top - m.buttonHintsHeight - m.verticalSpacing;
  return std::max(1, h / kRowHeight);
}

void drawSectionRow(const GfxRenderer& renderer, Rect row, char letter, int count, int itemNumber, bool selected) {
  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawRoundedRect(row.x, row.y, row.width, row.height, selected ? 2 : 1, kCardR, true);
  if (selected) {
    renderer.fillRoundedRect(row.x, row.y, 8, row.height, kCardR, true, false, true, false, Color::Black);
  }

  char num[4];
  snprintf(num, sizeof(num), "%02d", itemNumber);
  const int numW = renderer.getTextWidth(UI_10_FONT_ID, num, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, row.x + 22 - numW / 2, row.y + (row.height - lh10) / 2, num, true,
                    EpdFontFamily::BOLD);
  renderer.drawLine(row.x + 46, row.y + 12, row.x + 46, row.y + row.height - 12);

  const char letterBuf[2] = {letter, '\0'};
  const int textX = row.x + 62;
  renderer.drawText(UI_10_FONT_ID, textX, row.y + 12, letterBuf, true, EpdFontFamily::BOLD);

  char countBuf[40];
  snprintf(countBuf, sizeof(countBuf), "%d %s", count, count == 1 ? tr(STR_ENTRY_SINGULAR) : tr(STR_ENTRY_PLURAL));
  renderer.drawText(SMALL_FONT_ID, textX, row.y + 12 + lh10 + 6, countBuf, true);

  if (const uint8_t* chevron = iconForName(UIIcon::ChevronRightIcon, kChevronSize)) {
    renderer.drawIcon(chevron, row.x + row.width - 18 - kChevronSize, row.y + (row.height - kChevronSize) / 2,
                      kChevronSize, kChevronSize);
  }
}

void drawEntryRow(const GfxRenderer& renderer, Rect row, const BookCatalog::Entry& entry, int itemNumber,
                  bool selected) {
  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);

  renderer.drawRoundedRect(row.x, row.y, row.width, row.height, selected ? 2 : 1, kCardR, true);
  if (selected) {
    renderer.fillRoundedRect(row.x, row.y, 8, row.height, kCardR, true, false, true, false, Color::Black);
  }

  char num[4];
  snprintf(num, sizeof(num), "%02d", itemNumber);
  const int numW = renderer.getTextWidth(UI_10_FONT_ID, num, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, row.x + 22 - numW / 2, row.y + 18, num, true, EpdFontFamily::BOLD);
  renderer.drawLine(row.x + 46, row.y + 16, row.x + 46, row.y + row.height - 16);

  const int textX = row.x + 62;
  const int valueW = 96;
  const int titleW = row.width - (textX - row.x) - 16;
  const int metaW = row.width - (textX - row.x) - valueW - 20;
  std::string title = entry.title;
  if (entry.volume[0]) {
    title += " ";
    title += tr(STR_BOOK_VOLUME_SHORT);
    title += " ";
    title += entry.volume;
  }
  const auto titleLines = renderer.wrappedText(UI_10_FONT_ID, title.c_str(), titleW, 2, EpdFontFamily::BOLD);
  int y = row.y + 14;
  for (const auto& line : titleLines) {
    renderer.drawText(UI_10_FONT_ID, textX, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += lh10 + 1;
  }

  char sub[64] = {};
  if (entry.isCollection) {
    if (entry.expectedCount > 0) {
      snprintf(sub, sizeof(sub), "%d/%d %s", entry.count, entry.expectedCount,
               entry.expectedCount == 1 ? tr(STR_BOOK_SINGULAR) : tr(STR_BOOK_PLURAL));
    } else {
      snprintf(sub, sizeof(sub), "%d %s", entry.count, entry.count == 1 ? tr(STR_BOOK_SINGULAR) : tr(STR_BOOK_PLURAL));
    }
  } else {
    snprintf(sub, sizeof(sub), "%s", entry.author);
  }
  const auto subtitle = renderer.truncatedText(SMALL_FONT_ID, sub, metaW);
  renderer.drawText(SMALL_FONT_ID, textX, y + 4, subtitle.c_str(), true);

  if (entry.isCollection) {
    if (const uint8_t* chevron = iconForName(UIIcon::ChevronRightIcon, kChevronSize)) {
      renderer.drawIcon(chevron, row.x + row.width - 12 - kChevronSize,
                        row.y + row.height - 16 - lhSm / 2 - kChevronSize / 2, kChevronSize, kChevronSize);
    }
  } else {
    const auto value = renderer.truncatedText(SMALL_FONT_ID, entry.location, valueW);
    const int vw = renderer.getTextWidth(SMALL_FONT_ID, value.c_str());
    renderer.drawText(SMALL_FONT_ID, row.x + row.width - 12 - vw, row.y + row.height - lhSm - 16, value.c_str(), true);
  }
}

}  // namespace BookListUI
