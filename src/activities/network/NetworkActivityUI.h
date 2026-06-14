#pragma once

#include <GfxRenderer.h>

#include <algorithm>

#include "../books/BooksActivityUI.h"
#include "components/MyneUI.h"
#include "fontIds.h"

namespace NetworkActivityUI {

constexpr int PAD = BooksActivityUI::PAD;
constexpr int INNER = BooksActivityUI::INNER;
constexpr int GAP = BooksActivityUI::GAP;
constexpr int RADIUS = BooksActivityUI::RADIUS;

using BooksActivityUI::hero;
using BooksActivityUI::panel;
using BooksActivityUI::text;

inline void metric(const GfxRenderer& r, Rect rect, const char* label, const char* value, bool selected = false) {
  panel(r, rect, selected);
  text(r, SMALL_FONT_ID, rect.x + INNER, rect.y + 14, label, rect.width - INNER * 2, EpdFontFamily::BOLD);
  text(r, UI_10_FONT_ID, rect.x + INNER, rect.y + 42, value, rect.width - INNER * 2, EpdFontFamily::BOLD);
}

inline void choice(const GfxRenderer& r, Rect rect, const char* title, const char* detail, bool selected = false) {
  panel(r, rect, selected);
  const int x = rect.x + INNER + (selected ? 8 : 0);
  text(r, UI_10_FONT_ID, x, rect.y + 18, title, rect.width - INNER * 2 - (selected ? 8 : 0), EpdFontFamily::BOLD);
  text(r, SMALL_FONT_ID, x, rect.y + 54, detail, rect.width - INNER * 2 - (selected ? 8 : 0));
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

inline void signalBars(const GfxRenderer& r, int x, int y, int rssi) {
  int bars = 1;
  if (rssi >= -55)
    bars = 4;
  else if (rssi >= -65)
    bars = 3;
  else if (rssi >= -75)
    bars = 2;
  constexpr int bw = 5;
  constexpr int gap = 3;
  for (int i = 0; i < 4; ++i) {
    const int h = 6 + i * 4;
    const int bx = x + i * (bw + gap);
    const int by = y + 20 - h;
    if (i < bars)
      r.fillRect(bx, by, bw, h, true);
    else
      r.drawRect(bx, by, bw, h, true);
  }
}

}  // namespace NetworkActivityUI
