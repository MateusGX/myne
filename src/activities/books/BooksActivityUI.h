#pragma once

#include <GfxRenderer.h>

#include <algorithm>

#include "components/MyneUI.h"
#include "fontIds.h"

namespace BooksActivityUI {

constexpr int PAD    = 20;
constexpr int INNER  = 16;
constexpr int GAP    = 12;
constexpr int RADIUS = 8;
constexpr int HERO_H = 104;

inline void panel(const GfxRenderer& r, Rect rect, bool selected = false) {
  r.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, RADIUS, true);
  if (selected) {
    r.fillRoundedRect(rect.x, rect.y, 8, rect.height, RADIUS,
                      true, false, true, false, Color::Black);
  }
}

inline void text(const GfxRenderer& r, int font, int x, int y, const char* value, int maxW,
                 EpdFontFamily::Style style = EpdFontFamily::REGULAR,
                 bool black = true) {
  const auto safe = r.truncatedText(font, value, std::max(12, maxW), style);
  r.drawText(font, x, y, safe.c_str(), black, style);
}

inline void hero(const GfxRenderer& r, Rect rect, const char* eyebrow, const char* title,
                 const char* detail = nullptr, int rightReserve = 0) {
  panel(r, rect, true);
  text(r, SMALL_FONT_ID, rect.x + INNER, rect.y + 14, eyebrow,
       rect.width - INNER * 2 - rightReserve, EpdFontFamily::BOLD);
  text(r, UI_10_FONT_ID, rect.x + INNER, rect.y + 40, title,
       rect.width - INNER * 2 - rightReserve, EpdFontFamily::BOLD);
  if (detail && detail[0] != '\0') {
    text(r, SMALL_FONT_ID, rect.x + INNER, rect.y + 67, detail,
         rect.width - INNER * 2 - rightReserve);
  }
}

}  // namespace BooksActivityUI
