#pragma once

#include <GfxRenderer.h>

#include <algorithm>

#include "components/MyneUI.h"
#include "fontIds.h"

namespace SettingsActivityUI {
constexpr int PAD = 20;
constexpr int INNER = 16;
constexpr int GAP = 12;
constexpr int RADIUS = 8;

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
                 const char* detail = nullptr) {
  panel(r, rect, true);
  text(r, SMALL_FONT_ID, rect.x + INNER, rect.y + 14, eyebrow, rect.width - INNER * 2,
       EpdFontFamily::BOLD);
  text(r, UI_10_FONT_ID, rect.x + INNER, rect.y + 40, title, rect.width - INNER * 2,
       EpdFontFamily::BOLD);
  if (detail && detail[0] != '\0') {
    text(r, SMALL_FONT_ID, rect.x + INNER, rect.y + 68, detail, rect.width - INNER * 2);
  }
}

inline void option(const GfxRenderer& r, Rect rect, const char* title, const char* value,
                   bool selected = false) {
  panel(r, rect, selected);
  const int x = rect.x + INNER + (selected ? 8 : 0);
  const int rightW = value && value[0] != '\0' ? 112 : 0;
  text(r, UI_10_FONT_ID, x, rect.y + 14, title,
       rect.width - INNER * 2 - rightW - (selected ? 8 : 0), EpdFontFamily::BOLD);
  if (value && value[0] != '\0') {
    const auto safe = r.truncatedText(SMALL_FONT_ID, value, rightW, EpdFontFamily::BOLD);
    const int tw = r.getTextWidth(SMALL_FONT_ID, safe.c_str(), EpdFontFamily::BOLD);
    r.drawText(SMALL_FONT_ID, rect.x + rect.width - INNER - tw, rect.y + 20,
               safe.c_str(), true, EpdFontFamily::BOLD);
  }
}

inline void choice(const GfxRenderer& r, Rect rect, const char* title, const char* detail,
                   bool selected = false) {
  panel(r, rect, selected);
  const int x = rect.x + INNER + (selected ? 8 : 0);
  text(r, UI_10_FONT_ID, x, rect.y + 16, title, rect.width - INNER * 2 - (selected ? 8 : 0),
       EpdFontFamily::BOLD);
  if (detail && detail[0] != '\0') {
    text(r, SMALL_FONT_ID, x, rect.y + 50, detail, rect.width - INNER * 2 - (selected ? 8 : 0));
  }
}

inline void stateCard(const GfxRenderer& r, Rect rect, const char* title, const char* detail = nullptr) {
  panel(r, rect);
  const int titleH = r.getLineHeight(UI_10_FONT_ID);
  const int detailH = r.getLineHeight(SMALL_FONT_ID);
  std::vector<std::string> detailLines;
  if (detail && detail[0] != '\0') {
    detailLines = r.wrappedText(SMALL_FONT_ID, detail, rect.width - INNER * 2, 2);
  }
  const int contentH = titleH + (detailLines.empty() ? 0 : 12 + static_cast<int>(detailLines.size()) * detailH);
  int y = rect.y + rect.height / 2 - contentH / 2;
  r.drawCenteredText(UI_10_FONT_ID, y, title, true, EpdFontFamily::BOLD);
  y += titleH + 12;
  for (const auto& line : detailLines) {
    r.drawCenteredText(SMALL_FONT_ID, y, line.c_str(), true);
    y += detailH;
  }
}
}  // namespace SettingsActivityUI
