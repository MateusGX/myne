#include "MyneUI.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "MyneSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Internal constants (from LyraTheme)
namespace {
constexpr int hPaddingInSelection = 8;
constexpr int cornerRadius = 6;
constexpr int topHintButtonY = 345;
constexpr int popupMarginX = 16;
constexpr int popupMarginY = 12;
constexpr int popupAccentWidth = 8;
constexpr int popupAccentGap = 12;
constexpr int maxListValueWidth = 200;
constexpr int mainMenuIconSize = 32;
constexpr int listIconSize = 24;
}  // namespace

// ---------------------------------------------------------------------------
// Static battery helpers (from BaseTheme)
// ---------------------------------------------------------------------------

void MyneUI::drawBatteryOutline(const GfxRenderer& renderer, int x, int y, int battWidth, int rectHeight) {
  // Top line
  renderer.drawLine(x + 1, y, x + battWidth - 3, y);
  // Bottom line
  renderer.drawLine(x + 1, y + rectHeight - 1, x + battWidth - 3, y + rectHeight - 1);
  // Left line
  renderer.drawLine(x, y + 1, x, y + rectHeight - 2);
  // Battery end
  renderer.drawLine(x + battWidth - 2, y + 1, x + battWidth - 2, y + rectHeight - 2);
  renderer.drawPixel(x + battWidth - 1, y + 3);
  renderer.drawPixel(x + battWidth - 1, y + rectHeight - 4);
  renderer.drawLine(x + battWidth - 0, y + 4, x + battWidth - 0, y + rectHeight - 5);
}

void MyneUI::drawBatteryLightningBolt(const GfxRenderer& renderer, int boltX, int boltY) {
  // Draw lightning bolt (white/inverted on black fill for visibility)
  renderer.drawLine(boltX + 4, boltY + 0, boltX + 5, boltY + 0, false);
  renderer.drawLine(boltX + 3, boltY + 1, boltX + 4, boltY + 1, false);
  renderer.drawLine(boltX + 2, boltY + 2, boltX + 5, boltY + 2, false);
  renderer.drawLine(boltX + 3, boltY + 3, boltX + 4, boltY + 3, false);
  renderer.drawLine(boltX + 2, boltY + 4, boltX + 3, boltY + 4, false);
  renderer.drawLine(boltX + 1, boltY + 5, boltX + 4, boltY + 5, false);
  renderer.drawLine(boltX + 2, boltY + 6, boltX + 3, boltY + 6, false);
  renderer.drawLine(boltX + 1, boltY + 7, boltX + 2, boltY + 7, false);
}

// ---------------------------------------------------------------------------
// fillBatteryIcon — Lyra version (segmented bars instead of proportional fill)
// ---------------------------------------------------------------------------

void MyneUI::fillBatteryIcon(const GfxRenderer& renderer, Rect rect, uint16_t percentage) const {
  const bool charging = gpio.isUsbConnected();

  if (charging) {
    // Solid fill when charging so lightning bolt is visible
    renderer.fillRect(rect.x + 2, rect.y + 2, rect.width - 5, rect.height - 4);
    drawBatteryLightningBolt(renderer, rect.x + 4, rect.y + 2);
  } else {
    if (percentage > 10) {
      renderer.fillRect(rect.x + 2, rect.y + 2, 3, rect.height - 4);
    }
    if (percentage > 40) {
      renderer.fillRect(rect.x + 6, rect.y + 2, 3, rect.height - 4);
    }
    if (percentage > 70) {
      renderer.fillRect(rect.x + 10, rect.y + 2, 3, rect.height - 4);
    }
  }
}

// ---------------------------------------------------------------------------
// Battery left/right — BaseTheme version (unchanged, calls fillBatteryIcon)
// ---------------------------------------------------------------------------

void MyneUI::drawBatteryLeft(const GfxRenderer& renderer, Rect rect, const bool showPercentage) const {
  // Left aligned: icon on left, percentage on right (reader mode)
  const uint16_t percentage = powerManager.getBatteryPercentage();
  const int y = rect.y + 6;

  if (showPercentage) {
    const auto percentageText = std::to_string(percentage) + "%";
    renderer.drawText(SMALL_FONT_ID, rect.x + batteryPercentSpacing + rect.width, rect.y, percentageText.c_str());
  }

  const Rect iconRect{rect.x, y, rect.width, rect.height};
  drawBatteryOutline(renderer, rect.x, y, rect.width, rect.height);
  fillBatteryIcon(renderer, iconRect, percentage);
}

void MyneUI::drawBatteryRight(const GfxRenderer& renderer, Rect rect, const bool showPercentage) const {
  // Right aligned: percentage on left, icon on right (UI headers)
  const uint16_t percentage = powerManager.getBatteryPercentage();
  const int y = rect.y + 6;

  if (showPercentage) {
    const auto percentageText = std::to_string(percentage) + "%";
    const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, percentageText.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x - textWidth - batteryPercentSpacing, rect.y, percentageText.c_str());
  }

  const Rect iconRect{rect.x, y, rect.width, rect.height};
  drawBatteryOutline(renderer, rect.x, y, rect.width, rect.height);
  fillBatteryIcon(renderer, iconRect, percentage);
}

// ---------------------------------------------------------------------------
// drawProgressBar — BaseTheme version
// ---------------------------------------------------------------------------

void MyneUI::drawProgressBar(const GfxRenderer& renderer, Rect rect, const size_t current, const size_t total) const {
  if (total == 0) {
    return;
  }

  // Use 64-bit arithmetic to avoid overflow for large files
  const int percent = static_cast<int>((static_cast<uint64_t>(current) * 100) / total);

  LOG_DBG("UI", "Drawing progress bar: current=%u, total=%u, percent=%d", current, total, percent);
  // Draw outline
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);

  // Draw filled portion
  const int fillWidth = (rect.width - 4) * percent / 100;
  if (fillWidth > 0) {
    renderer.fillRect(rect.x + 2, rect.y + 2, fillWidth, rect.height - 4);
  }

  // Draw percentage text centered below bar
  const std::string percentText = std::to_string(percent) + "%";
  renderer.drawCenteredText(UI_10_FONT_ID, rect.y + rect.height + 15, percentText.c_str());
}

// ---------------------------------------------------------------------------
// drawHeader — battery-only strip
// ---------------------------------------------------------------------------

void MyneUI::drawHeader(const GfxRenderer& renderer, Rect rect) const {
  const bool showPercentage = SETTINGS.hideBatteryPercentage != MyneSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  const int batteryX = rect.x + rect.width - 12 - MyneUIMetrics::values.batteryWidth;
  drawBatteryRight(renderer,
                   Rect{batteryX, rect.y + (rect.height - MyneUIMetrics::values.batteryHeight) / 2,
                        MyneUIMetrics::values.batteryWidth, MyneUIMetrics::values.batteryHeight},
                   showPercentage);
}

// ---------------------------------------------------------------------------
// drawPageTitle
// ---------------------------------------------------------------------------

void MyneUI::drawPageTitle(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle) const {
  const int pad = MyneUIMetrics::values.contentSidePadding;
  const int lh12 = renderer.getLineHeight(UI_12_FONT_ID);
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);
  const int textH = rect.height - 1;  // 1px for bottom border

  if (title) {
    const int availW =
        subtitle ? rect.width - pad * 3 - renderer.getTextWidth(SMALL_FONT_ID, subtitle) : rect.width - pad * 2;
    const auto t = renderer.truncatedText(UI_12_FONT_ID, title, availW, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, rect.x + pad, rect.y + (textH - lh12) / 2, t.c_str(), true, EpdFontFamily::BOLD);
  }

  if (subtitle) {
    const int sw = renderer.getTextWidth(SMALL_FONT_ID, subtitle);
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - pad - sw, rect.y + (textH - lhSm) / 2, subtitle, true);
  }

  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

// ---------------------------------------------------------------------------
// drawSubHeader — Lyra version
// ---------------------------------------------------------------------------

void MyneUI::drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label, const char* rightLabel) const {
  int currentX = rect.x + MyneUIMetrics::values.contentSidePadding;
  int rightSpace = MyneUIMetrics::values.contentSidePadding;
  if (rightLabel) {
    auto truncatedRightLabel =
        renderer.truncatedText(SMALL_FONT_ID, rightLabel, maxListValueWidth, EpdFontFamily::REGULAR);
    int rightLabelWidth = renderer.getTextWidth(SMALL_FONT_ID, truncatedRightLabel.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - MyneUIMetrics::values.contentSidePadding - rightLabelWidth,
                      rect.y + 7, truncatedRightLabel.c_str());
    rightSpace += rightLabelWidth + hPaddingInSelection;
  }

  auto truncatedLabel = renderer.truncatedText(
      UI_10_FONT_ID, label, rect.width - MyneUIMetrics::values.contentSidePadding - rightSpace, EpdFontFamily::REGULAR);
  renderer.drawText(UI_10_FONT_ID, currentX, rect.y + 6, truncatedLabel.c_str(), true, EpdFontFamily::REGULAR);

  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
}

// ---------------------------------------------------------------------------
// drawTabBar — Lyra version
// ---------------------------------------------------------------------------

void MyneUI::drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs, bool selected) const {
  int currentX = rect.x + MyneUIMetrics::values.contentSidePadding;

  if (selected) {
    renderer.fillRectDither(rect.x, rect.y, rect.width, rect.height, Color::LightGray);
  }

  for (const auto& tab : tabs) {
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, tab.label, EpdFontFamily::REGULAR);

    if (tab.selected) {
      if (selected) {
        renderer.fillRoundedRect(currentX, rect.y + 1, textWidth + 2 * hPaddingInSelection, rect.height - 4,
                                 cornerRadius, Color::Black);
      } else {
        renderer.fillRectDither(currentX, rect.y, textWidth + 2 * hPaddingInSelection, rect.height - 3,
                                Color::LightGray);
        renderer.drawLine(currentX, rect.y + rect.height - 3, currentX + textWidth + 2 * hPaddingInSelection,
                          rect.y + rect.height - 3, 2, true);
      }
    }

    renderer.drawText(UI_10_FONT_ID, currentX + hPaddingInSelection, rect.y + 6, tab.label, !(tab.selected && selected),
                      EpdFontFamily::REGULAR);

    currentX += textWidth + MyneUIMetrics::values.tabSpacing + 2 * hPaddingInSelection;
  }

  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
}

// ---------------------------------------------------------------------------
// drawList — Lyra version
// ---------------------------------------------------------------------------

void MyneUI::drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                      const std::function<std::string(int index)>& rowTitle,
                      const std::function<std::string(int index)>& rowSubtitle,
                      const std::function<UIIcon(int index)>& rowIcon,
                      const std::function<std::string(int index)>& rowValue, bool highlightValue,
                      const std::function<bool(int index)>& rowDimmed) const {
  int rowHeight =
      (rowSubtitle != nullptr) ? MyneUIMetrics::values.listWithSubtitleRowHeight : MyneUIMetrics::values.listRowHeight;
  int pageItems = rect.height / rowHeight;

  const int totalPages = (itemCount + pageItems - 1) / pageItems;
  if (totalPages > 1) {
    const int scrollAreaHeight = rect.height;

    // Draw scroll bar
    const int scrollBarHeight = (scrollAreaHeight * pageItems) / itemCount;
    const int currentPage = selectedIndex / pageItems;
    const int scrollBarY = rect.y + ((scrollAreaHeight - scrollBarHeight) * currentPage) / (totalPages - 1);
    const int scrollBarX = rect.x + rect.width - MyneUIMetrics::values.scrollBarRightOffset;
    renderer.drawLine(scrollBarX, rect.y, scrollBarX, rect.y + scrollAreaHeight, true);
    renderer.fillRect(scrollBarX - MyneUIMetrics::values.scrollBarWidth, scrollBarY,
                      MyneUIMetrics::values.scrollBarWidth, scrollBarHeight, true);
  }

  // Draw selection
  int contentWidth =
      rect.width -
      (totalPages > 1 ? (MyneUIMetrics::values.scrollBarWidth + MyneUIMetrics::values.scrollBarRightOffset) : 1);
  if (selectedIndex >= 0) {
    renderer.fillRoundedRect(MyneUIMetrics::values.contentSidePadding, rect.y + selectedIndex % pageItems * rowHeight,
                             contentWidth - MyneUIMetrics::values.contentSidePadding * 2, rowHeight, cornerRadius,
                             Color::LightGray);
  }

  int textX = rect.x + MyneUIMetrics::values.contentSidePadding + hPaddingInSelection;
  int textWidth = contentWidth - MyneUIMetrics::values.contentSidePadding * 2 - hPaddingInSelection * 2;
  int iconSize = 0;
  if (rowIcon != nullptr) {
    iconSize = (rowSubtitle != nullptr) ? mainMenuIconSize : listIconSize;
    textX += iconSize + hPaddingInSelection;
    textWidth -= iconSize + hPaddingInSelection;
  }

  // Draw all items
  const auto pageStartIndex = selectedIndex / pageItems * pageItems;
  int iconY = (rowSubtitle != nullptr) ? 16 : 10;
  for (int i = pageStartIndex; i < itemCount && i < pageStartIndex + pageItems; i++) {
    const int itemY = rect.y + (i % pageItems) * rowHeight;
    int rowTextWidth = textWidth;

    // Draw name
    int valueWidth = 0;
    std::string valueText;
    if (rowValue != nullptr) {
      valueText = rowValue(i);
      valueText = renderer.truncatedText(UI_10_FONT_ID, valueText.c_str(), maxListValueWidth);
      valueWidth = renderer.getTextWidth(UI_10_FONT_ID, valueText.c_str()) + hPaddingInSelection;
      rowTextWidth -= valueWidth;
    }

    auto itemName = rowTitle(i);
    auto item = renderer.truncatedText(UI_10_FONT_ID, itemName.c_str(), rowTextWidth);
    renderer.drawText(UI_10_FONT_ID, textX, itemY + 7, item.c_str(), true);

    // Apply checkerboard dither to create gray text effect for dimmed items
    if (rowDimmed && rowDimmed(i) && i != selectedIndex) {
      const int titleWidth = renderer.getTextWidth(UI_10_FONT_ID, item.c_str());
      const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
      for (int py = itemY + 7; py < itemY + 7 + lineH; py++)
        for (int px = textX; px < textX + titleWidth; px++)
          if ((px + py) % 2 == 0) renderer.drawPixel(px, py, false);
    }

    if (rowIcon != nullptr) {
      UIIcon icon = rowIcon(i);
      const uint8_t* iconBitmap = iconForName(icon, iconSize);
      int drawnSize = iconSize;
      if (iconBitmap == nullptr && iconSize != listIconSize) {
        iconBitmap = iconForName(icon, listIconSize);
        drawnSize = listIconSize;
      }
      if (iconBitmap != nullptr) {
        renderer.drawIcon(iconBitmap, rect.x + MyneUIMetrics::values.contentSidePadding + hPaddingInSelection,
                          itemY + iconY, drawnSize, drawnSize);
      }
    }

    if (rowSubtitle != nullptr) {
      std::string subtitleText = rowSubtitle(i);
      auto subtitle = renderer.truncatedText(SMALL_FONT_ID, subtitleText.c_str(), rowTextWidth);
      renderer.drawText(SMALL_FONT_ID, textX, itemY + 30, subtitle.c_str(), true);
    }

    // Draw value
    if (!valueText.empty()) {
      if (i == selectedIndex && highlightValue) {
        renderer.fillRoundedRect(
            contentWidth - MyneUIMetrics::values.contentSidePadding - hPaddingInSelection - valueWidth, itemY,
            valueWidth + hPaddingInSelection, rowHeight, cornerRadius, Color::Black);
      }

      int valueY = itemY + 6;
      if (rowSubtitle != nullptr) {
        valueY = itemY + 16;
      }
      renderer.drawText(UI_10_FONT_ID, rect.x + contentWidth - MyneUIMetrics::values.contentSidePadding - valueWidth,
                        valueY, valueText.c_str(), !(i == selectedIndex && highlightValue));
    }
  }
}

// ---------------------------------------------------------------------------
// drawButtonHints — Lyra version
// ---------------------------------------------------------------------------

void MyneUI::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                             const char* btn4) const {
  const GfxRenderer::Orientation orig = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);
  const int hintH = MyneUIMetrics::values.buttonHintsHeight;
  const int hintTop = H - hintH;

  static constexpr int SIDE_PAD = 12;
  static constexpr int MID_GAP = 16;
  static constexpr int V_PAD = 5;
  static constexpr int BOTTOM_PAD = 10;

  const int pillH = hintH - V_PAD - BOTTOM_PAD;
  const int pillY = hintTop + V_PAD;
  const int pillW = (W - SIDE_PAD * 2 - MID_GAP) / 2;
  const int leftX = SIDE_PAD;
  const int rightX = SIDE_PAD + pillW + MID_GAP;
  const int textY = pillY + (pillH - lhSm) / 2;

  const char* labels[4] = {btn1, btn2, btn3, btn4};

  auto drawPill = [&](int px, int ia, int ib) {
    const char* la = labels[ia];
    const char* lb = labels[ib];
    const bool hasA = la && la[0];
    const bool hasB = lb && lb[0];
    if (!hasA && !hasB) return;

    renderer.drawRoundedRect(px, pillY, pillW, pillH, 1, cornerRadius, true);

    if (hasA && hasB) {
      const int divX = px + pillW / 2;
      renderer.drawLine(divX, pillY + 4, divX, pillY + pillH - 4);
      const int tw1 = renderer.getTextWidth(SMALL_FONT_ID, la);
      renderer.drawText(SMALL_FONT_ID, px + pillW / 4 - tw1 / 2, textY, la, true);
      const int tw2 = renderer.getTextWidth(SMALL_FONT_ID, lb);
      renderer.drawText(SMALL_FONT_ID, px + 3 * pillW / 4 - tw2 / 2, textY, lb, true);
    } else {
      const char* label = hasA ? la : lb;
      const int tw = renderer.getTextWidth(SMALL_FONT_ID, label);
      renderer.drawText(SMALL_FONT_ID, px + pillW / 2 - tw / 2, textY, label, true);
    }
  };

  drawPill(leftX, 0, 1);
  drawPill(rightX, 2, 3);

  renderer.setOrientation(orig);
}

// ---------------------------------------------------------------------------
// drawSideButtonHints — Lyra version (X4 only: both buttons stacked on the right side)
// ---------------------------------------------------------------------------

void MyneUI::drawSideButtonHints(const GfxRenderer& renderer, const char* topBtn, const char* bottomBtn) const {
  constexpr int buttonWidth = MyneUIMetrics::values.sideButtonHintsWidth;
  constexpr int buttonHeight = 78;
  constexpr int GAP = 5;

  const int x = renderer.getScreenWidth() - buttonWidth;

  auto drawHint = [&](int rectY, int textY, const char* label) {
    if (label == nullptr || label[0] == '\0') return;

    renderer.drawRoundedRect(x, rectY, buttonWidth, buttonHeight, 1, cornerRadius, true, false, true, false, true);

    const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, label);
    renderer.drawTextRotated90CW(SMALL_FONT_ID, x, textY + (buttonHeight + textWidth) / 2, label);
  };

  drawHint(topHintButtonY, topHintButtonY + GAP, topBtn);
  drawHint(topHintButtonY + buttonHeight + GAP, topHintButtonY + buttonHeight + GAP, bottomBtn);
}

// ---------------------------------------------------------------------------
// drawButtonMenu — Lyra version
// ---------------------------------------------------------------------------

void MyneUI::drawButtonMenu(const GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                            const std::function<std::string(int index)>& buttonLabel,
                            const std::function<UIIcon(int index)>& rowIcon) const {
  for (int i = 0; i < buttonCount; ++i) {
    int tileWidth = rect.width - MyneUIMetrics::values.contentSidePadding * 2;
    Rect tileRect = Rect{rect.x + MyneUIMetrics::values.contentSidePadding,
                         rect.y + i * (MyneUIMetrics::values.menuRowHeight + MyneUIMetrics::values.menuSpacing),
                         tileWidth, MyneUIMetrics::values.menuRowHeight};

    const bool selected = selectedIndex == i;

    if (selected) {
      renderer.fillRoundedRect(tileRect.x, tileRect.y, tileRect.width, tileRect.height, cornerRadius, Color::LightGray);
    }

    std::string labelStr = buttonLabel(i);
    const char* label = labelStr.c_str();
    int textX = tileRect.x + 16;
    const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
    const int textY = tileRect.y + (MyneUIMetrics::values.menuRowHeight - lineHeight) / 2;

    if (rowIcon != nullptr) {
      UIIcon icon = rowIcon(i);
      const uint8_t* iconBitmap = iconForName(icon, mainMenuIconSize);
      if (iconBitmap != nullptr) {
        renderer.drawIcon(iconBitmap, textX, textY + 3, mainMenuIconSize, mainMenuIconSize);
        textX += mainMenuIconSize + hPaddingInSelection + 2;
      }
    }

    renderer.drawText(UI_12_FONT_ID, textX, textY, label, true);
  }
}

// ---------------------------------------------------------------------------
// drawIconGrid — 2D icon+label grid
// ---------------------------------------------------------------------------

void MyneUI::drawIconGrid(const GfxRenderer& renderer, Rect rect, int count, int selectedIndex,
                          const std::function<std::string(int index)>& labelFn,
                          const std::function<UIIcon(int index)>& iconFn) const {
  if (count <= 0) return;

  constexpr int cols = 2;
  constexpr int rows = 3;
  constexpr int iconSize = 64;
  constexpr int cellPadding = 12;
  constexpr int cellRadius = 10;
  constexpr int iconLabelGap = 22;

  const int cellW = rect.width / cols;
  // const int cellH = rect.height / rows;
  const int cellH = cellW;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int contentH = iconSize + iconLabelGap + lineHeight;

  for (int i = 0; i < count; i++) {
    const int col = i % cols;
    const int row = i / cols;

    const int cellX = rect.x + col * cellW;
    const int cellY = rect.y + row * cellH;

    const int innerX = cellX + cellPadding;
    const int innerY = cellY + cellPadding;
    const int innerW = cellW - cellPadding * 2;
    const int innerH = cellH - cellPadding * 2;

    const bool selected = (i == selectedIndex);

    if (selected) {
      renderer.fillRoundedRect(innerX, innerY, innerW, innerH, cellRadius, Color::LightGray);
      renderer.drawRoundedRect(innerX, innerY, innerW, innerH, 2, cellRadius, true);
    } else {
      renderer.drawRoundedRect(innerX, innerY, innerW, innerH, 1, cellRadius, true);
    }

    // Center icon+label block vertically in cell
    const int blockY = innerY + (innerH - contentH) / 2;

    // Draw icon
    const uint8_t* bmp = iconForName(iconFn(i), iconSize);
    if (bmp) {
      const int iconX = innerX + (innerW - iconSize) / 2;
      renderer.drawIcon(bmp, iconX, blockY, iconSize, iconSize);
    }

    // Draw label centered within the cell
    const std::string label = labelFn(i);
    const int textW = renderer.getTextWidth(UI_12_FONT_ID, label.c_str(), EpdFontFamily::BOLD);
    const int textX = innerX + (innerW - textW) / 2;
    const int textY = blockY + iconSize + iconLabelGap;
    renderer.drawText(UI_12_FONT_ID, textX, textY, label.c_str(), true, EpdFontFamily::BOLD);
  }

  // Draw empty cells with a barely-visible dithered background
  for (int i = count; i < cols * rows; i++) {
    const int col = i % cols;
    const int row = i / cols;

    const int innerX = rect.x + col * cellW + cellPadding;
    const int innerY = rect.y + row * cellH + cellPadding;
    const int innerW = cellW - cellPadding * 2;
    const int innerH = cellH - cellPadding * 2;

    renderer.fillRoundedRect(innerX, innerY, innerW, innerH, cellRadius, Color::LightGray);
  }
}

// ---------------------------------------------------------------------------
// drawPopup — Lyra version
// ---------------------------------------------------------------------------

Rect MyneUI::drawPopup(const GfxRenderer& renderer, const char* message) const {
  const int maxWidth = renderer.getScreenWidth() - MyneUIMetrics::values.contentSidePadding * 2;
  const int maxTextWidth = std::max(12, maxWidth - popupMarginX * 2 - popupAccentWidth - popupAccentGap);
  const auto text = renderer.truncatedText(UI_12_FONT_ID, message, maxTextWidth, EpdFontFamily::REGULAR);
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, text.c_str(), EpdFontFamily::REGULAR);
  const int textHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int w = textWidth + popupMarginX * 2 + popupAccentWidth + popupAccentGap;
  const int h = textHeight + popupMarginY * 2;
  const int x = MyneUIMetrics::values.contentSidePadding;
  const int y = MyneUIMetrics::values.contentSidePadding;

  renderer.fillRoundedRect(x, y, w, h, cornerRadius, Color::White);
  renderer.drawRoundedRect(x, y, w, h, 1, cornerRadius, true);
  renderer.fillRoundedRect(x, y, popupAccentWidth, h, cornerRadius, true, false, true, false, Color::Black);

  const int textX = x + popupMarginX + popupAccentWidth + popupAccentGap;
  const int textY = y + popupMarginY - 2;
  renderer.drawText(UI_12_FONT_ID, textX, textY, text.c_str(), true, EpdFontFamily::REGULAR);
  renderer.displayBuffer();

  return Rect{x, y, w, h};
}

// ---------------------------------------------------------------------------
// fillPopupProgress — Lyra version
// ---------------------------------------------------------------------------

void MyneUI::fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, const int progress) const {
  constexpr int barHeight = 6;

  const int barWidth = layout.width - popupMarginX * 2 - popupAccentWidth - popupAccentGap;
  const int barX = layout.x + popupMarginX + popupAccentWidth + popupAccentGap;
  const int barY = layout.y + layout.height - popupMarginY / 2 - barHeight / 2 - 1;

  renderer.fillRoundedRect(barX, barY, barWidth, barHeight, barHeight / 2, Color::White);
  renderer.drawRoundedRect(barX, barY, barWidth, barHeight, 1, barHeight / 2, true);

  const int fillWidth = std::max(0, std::min(barWidth - 2, (barWidth - 2) * progress / 100));
  if (fillWidth > 0) {
    renderer.fillRoundedRect(barX + 1, barY + 1, fillWidth, barHeight - 2, (barHeight - 2) / 2, Color::Black);
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

// ---------------------------------------------------------------------------
// drawStatusBar — BaseTheme version
// ---------------------------------------------------------------------------

void MyneUI::drawStatusBar(GfxRenderer& renderer, const float bookProgress, const int currentPage, const int pageCount,
                           std::string title, const int paddingBottom, const int textYOffset) const {
  auto metrics = UITheme::getInstance().getMetrics();
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);

  // Draw Progress Text
  const auto screenHeight = renderer.getScreenHeight();
  auto textY = screenHeight - UITheme::getInstance().getStatusBarHeight() - orientedMarginBottom - paddingBottom - 4;
  int progressTextWidth = 0;

  if (SETTINGS.statusBarBookProgressPercentage || SETTINGS.statusBarChapterPageCount) {
    // Right aligned text for progress counter
    char progressStr[32];

    if (SETTINGS.statusBarBookProgressPercentage && SETTINGS.statusBarChapterPageCount) {
      snprintf(progressStr, sizeof(progressStr), "%d/%d  %.0f%%", currentPage, pageCount, bookProgress);
    } else if (SETTINGS.statusBarBookProgressPercentage) {
      snprintf(progressStr, sizeof(progressStr), "%.0f%%", bookProgress);
    } else {
      snprintf(progressStr, sizeof(progressStr), "%d/%d", currentPage, pageCount);
    }

    progressTextWidth = renderer.getTextWidth(SMALL_FONT_ID, progressStr);
    renderer.drawText(
        SMALL_FONT_ID,
        renderer.getScreenWidth() - metrics.statusBarHorizontalMargin - orientedMarginRight - progressTextWidth, textY,
        progressStr);
  }

  // Draw Progress Bar
  if (SETTINGS.statusBarProgressBar != MyneSettings::STATUS_BAR_PROGRESS_BAR::HIDE_PROGRESS) {
    const int progressBarMaxWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
    const int progressBarY = renderer.getScreenHeight() - orientedMarginBottom -
                             ((SETTINGS.statusBarProgressBarThickness + 1) * 2) - paddingBottom;
    size_t progress;
    if (SETTINGS.statusBarProgressBar == MyneSettings::STATUS_BAR_PROGRESS_BAR::BOOK_PROGRESS) {
      progress = static_cast<size_t>(bookProgress);
    } else {
      // Chapter progress
      progress = (pageCount > 0) ? (static_cast<float>(currentPage) / pageCount) * 100 : 0;
    }
    const int barWidth = progressBarMaxWidth * progress / 100;
    renderer.fillRect(orientedMarginLeft, progressBarY, barWidth, ((SETTINGS.statusBarProgressBarThickness + 1) * 2),
                      true);
  }

  // Draw Battery
  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage == MyneSettings::HIDE_BATTERY_PERCENTAGE::HIDE_NEVER;
  if (SETTINGS.statusBarBattery) {
    GUI.drawBatteryLeft(renderer,
                        Rect{metrics.statusBarHorizontalMargin + orientedMarginLeft + 1, textY, metrics.batteryWidth,
                             metrics.batteryHeight},
                        showBatteryPercentage);
  }

  // Draw Title
  if (!title.empty()) {
    textY -= textYOffset;
    // Centered chapter title text
    // Page width minus existing content with 30px padding on each side
    const int rendererableScreenWidth =
        renderer.getScreenWidth() - (metrics.statusBarHorizontalMargin * 2) - orientedMarginLeft - orientedMarginRight;

    const int batterySize = SETTINGS.statusBarBattery ? (showBatteryPercentage ? 50 : 20) : 0;
    const int titleMarginLeft = batterySize + 30;
    const int titleMarginRight = progressTextWidth + 30;

    // Attempt to center title on the screen, but if title is too wide then later we will center it within the
    // available space.
    int titleMarginLeftAdjusted = std::max(titleMarginLeft, titleMarginRight);
    int availableTitleSpace = rendererableScreenWidth - 2 * titleMarginLeftAdjusted;

    int titleWidth;
    titleWidth = renderer.getTextWidth(SMALL_FONT_ID, title.c_str());
    if (titleWidth > availableTitleSpace) {
      // Not enough space to center on the screen, center it within the remaining space instead
      availableTitleSpace = rendererableScreenWidth - titleMarginLeft - titleMarginRight;
      titleMarginLeftAdjusted = titleMarginLeft;
    }
    if (titleWidth > availableTitleSpace) {
      title = renderer.truncatedText(SMALL_FONT_ID, title.c_str(), availableTitleSpace);
      titleWidth = renderer.getTextWidth(SMALL_FONT_ID, title.c_str());
    }

    renderer.drawText(SMALL_FONT_ID,
                      titleMarginLeftAdjusted + metrics.statusBarHorizontalMargin + orientedMarginLeft +
                          (availableTitleSpace - titleWidth) / 2,
                      textY, title.c_str());
  }
}

// ---------------------------------------------------------------------------
// drawHelpText — BaseTheme version
// ---------------------------------------------------------------------------

void MyneUI::drawHelpText(const GfxRenderer& renderer, Rect rect, const char* label) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  auto truncatedLabel =
      renderer.truncatedText(SMALL_FONT_ID, label, rect.width - metrics.contentSidePadding * 2, EpdFontFamily::REGULAR);
  renderer.drawCenteredText(SMALL_FONT_ID, rect.y, truncatedLabel.c_str());
}

// ---------------------------------------------------------------------------
// drawTextField — BaseTheme version
// ---------------------------------------------------------------------------

void MyneUI::drawTextField(const GfxRenderer& renderer, Rect rect, const int textWidth, bool cursorMode,
                           int contentStartX, int contentWidth) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int lineY = rect.y + rect.height + lineHeight + metrics.verticalSpacing;
  const int thickness = cursorMode ? 3 : 1;
  if (contentWidth > 0) {
    renderer.drawLine(rect.x + contentStartX, lineY, rect.x + contentStartX + contentWidth, lineY, thickness, true);
  } else {
    const int hPadding = 6;
    const int lineW = textWidth + hPadding * 2;
    renderer.drawLine(rect.x + (rect.width - lineW) / 2, lineY, rect.x + (rect.width + lineW) / 2, lineY, thickness,
                      true);
  }
}

// ---------------------------------------------------------------------------
// drawCarouselMenuCard — icon + label card for home screen carousel
// ---------------------------------------------------------------------------

void MyneUI::drawCarouselMenuCard(const GfxRenderer& renderer, Rect rect, const CarouselMenuCard& card) const {
  constexpr int iconSize = 64;
  constexpr int cardRadius = 14;
  constexpr int sidePad = 28;
  constexpr int labelH = 18;
  constexpr int labelDescGap = 8;

  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 2, cardRadius, true);

  // Separator divides card: upper icon zone (65%) / lower label zone (35%)
  const int sepY = rect.y + rect.height * 65 / 100;

  // Icon centered in upper zone
  const int upperH = sepY - rect.y;
  const uint8_t* bmp = iconForName(card.icon, iconSize);
  if (bmp) {
    renderer.drawIcon(bmp, rect.x + (rect.width - iconSize) / 2, rect.y + (upperH - iconSize) / 2, iconSize, iconSize);
  }

  // Separator line
  renderer.drawLine(rect.x + sidePad, sepY, rect.x + rect.width - sidePad, sepY);

  // Label anchored near top of lower zone; description below
  const int labelY = sepY + 20;
  const int labelW = renderer.getTextWidth(UI_12_FONT_ID, card.label, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, rect.x + (rect.width - labelW) / 2, labelY, card.label, true, EpdFontFamily::BOLD);

  if (card.description != nullptr && card.description[0] != '\0') {
    const int textW = rect.width - sidePad * 2;
    const auto desc = renderer.truncatedText(SMALL_FONT_ID, card.description, textW);
    const int descW = renderer.getTextWidth(SMALL_FONT_ID, desc.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x + (rect.width - descW) / 2, labelY + labelH + labelDescGap, desc.c_str(),
                      true);
  }
}

// ---------------------------------------------------------------------------
// drawCarouselBookCard — editorial book card for home screen carousel
// ---------------------------------------------------------------------------

void MyneUI::drawCarouselBookCard(GfxRenderer& renderer, Rect rect, const CarouselBookCard& card) const {
  constexpr int cardRadius = 14;
  constexpr int sidePad = 28;

  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 2, cardRadius, true);

  if (card.title == nullptr) {
    // No book: mirror menu card layout with BookHeart icon
    constexpr int iconSize = 64;
    constexpr int labelH = 18;
    constexpr int labelDescGap = 8;
    const int sepY = rect.y + rect.height * 65 / 100;
    const int upperH = sepY - rect.y;
    const uint8_t* bmp = iconForName(UIIcon::BookHeartIcon, iconSize);
    if (bmp) {
      renderer.drawIcon(bmp, rect.x + (rect.width - iconSize) / 2, rect.y + (upperH - iconSize) / 2, iconSize,
                        iconSize);
    }
    renderer.drawLine(rect.x + sidePad, sepY, rect.x + rect.width - sidePad, sepY);
    const int labelY = sepY + 20;
    const int labelW = renderer.getTextWidth(UI_12_FONT_ID, card.sectionLabel, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, rect.x + (rect.width - labelW) / 2, labelY, card.sectionLabel, true,
                      EpdFontFamily::BOLD);
    if (card.description != nullptr && card.description[0] != '\0') {
      const int textW = rect.width - sidePad * 2;
      const auto desc = renderer.truncatedText(SMALL_FONT_ID, card.description, textW);
      const int descW = renderer.getTextWidth(SMALL_FONT_ID, desc.c_str());
      renderer.drawText(SMALL_FONT_ID, rect.x + (rect.width - descW) / 2, labelY + labelH + labelDescGap, desc.c_str(),
                        true);
    }
    return;
  }

  // Editorial text layout — no cover placeholder
  const int textX = rect.x + sidePad;
  const int textW = rect.width - sidePad * 2;
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);
  const int lhTitle = renderer.getLineHeight(UI_12_FONT_ID);

  // Section label + top separator
  constexpr int labelTopPad = 28;
  renderer.drawText(SMALL_FONT_ID, textX, rect.y + labelTopPad, card.sectionLabel, true, EpdFontFamily::BOLD);
  const int topSepY = rect.y + labelTopPad + lhSm + 12;
  renderer.drawLine(rect.x + sidePad, topSepY, rect.x + rect.width - sidePad, topSepY);

  // Bottom footer: progress/date anchored near card bottom
  const int botSepY = rect.y + rect.height * 80 / 100;
  renderer.drawLine(rect.x + sidePad, botSepY, rect.x + rect.width - sidePad, botSepY);
  const int rowY = botSepY + 16;
  if (card.progress != nullptr) {
    renderer.drawText(SMALL_FONT_ID, textX, rowY, card.progress, true);
  }
  if (card.date != nullptr) {
    const int dateW = renderer.getTextWidth(SMALL_FONT_ID, card.date);
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - sidePad - dateW, rowY, card.date, true);
  }
  if (card.status != nullptr) {
    const int statusY = rowY + lhSm + 6;
    renderer.drawText(SMALL_FONT_ID, textX, statusY, card.status, true);
  }

  // Title + author — vertically centered in the middle zone
  const auto lines = renderer.wrappedText(UI_12_FONT_ID, card.title, textW, 3, EpdFontFamily::BOLD);
  const bool hasAuthor = (card.author != nullptr && card.author[0] != '\0');
  const int blockH = (int)lines.size() * lhTitle + (hasAuthor ? (8 + lhSm) : 0);
  const int midTop = topSepY + 20;
  const int midBot = botSepY - 20;
  int curY = midTop + ((midBot - midTop) - blockH) / 2;
  if (curY < midTop) curY = midTop;

  for (const auto& line : lines) {
    renderer.drawText(UI_12_FONT_ID, textX, curY, line.c_str(), true, EpdFontFamily::BOLD);
    curY += lhTitle;
  }
  if (hasAuthor) {
    curY += 8;
    char authorBuf[44];
    snprintf(authorBuf, sizeof(authorBuf), "by %s", card.author);
    const auto truncated = renderer.truncatedText(SMALL_FONT_ID, authorBuf, textW);
    renderer.drawText(SMALL_FONT_ID, textX, curY, truncated.c_str(), true);
  }
}

// ---------------------------------------------------------------------------
// drawKeyboardKey — BaseTheme version
// ---------------------------------------------------------------------------

void MyneUI::drawKeyboardKey(const GfxRenderer& renderer, Rect rect, const char* label, const bool isSelected,
                             const char* secondaryLabel, const KeyboardKeyType keyType,
                             const bool inactiveSelection) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int cr = metrics.keyboardKeyCornerRadius;

  if (isSelected) {
    if (inactiveSelection) {
      if (cr > 0) {
        renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, cr, Color::LightGray);
      } else {
        renderer.drawRect(rect.x, rect.y, rect.width, rect.height, 2, true);
      }
    } else if (keyType == KeyboardKeyType::Disabled) {
      if (cr > 0) {
        renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, cr, Color::LightGray);
      } else {
        renderer.fillRectDither(rect.x, rect.y, rect.width, rect.height, Color::LightGray);
      }
    } else {
      if (cr > 0) {
        renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, cr, Color::Black);
      } else {
        renderer.fillRect(rect.x, rect.y, rect.width, rect.height, true);
      }
    }
  } else if (keyType == KeyboardKeyType::Shift || keyType == KeyboardKeyType::Mode || keyType == KeyboardKeyType::Del ||
             keyType == KeyboardKeyType::Space || keyType == KeyboardKeyType::Ok ||
             keyType == KeyboardKeyType::Disabled) {
    if (cr > 0) {
      renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, cr, true);
    } else {
      renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
    }
  }

  const bool invert = isSelected && !inactiveSelection;

  if (keyType == KeyboardKeyType::Space) {
    const int lineHalfWidth = rect.width * 3 / 10;
    const int centerX = rect.x + rect.width / 2;
    const int lineY = rect.y + rect.height / 2 + 3;
    renderer.drawLine(centerX - lineHalfWidth, lineY, centerX + lineHalfWidth, lineY, 3, !invert);
    return;
  }

  if (keyType == KeyboardKeyType::Del) {
    const int centerX = rect.x + rect.width / 2;
    const int centerY = rect.y + rect.height / 2;
    const int arrowLen = rect.width / 4;
    const int arrowHead = arrowLen / 2;
    renderer.drawLine(centerX - arrowLen / 2, centerY, centerX + arrowLen / 2, centerY, 3, !invert);
    renderer.drawLine(centerX - arrowLen / 2, centerY, centerX - arrowLen / 2 + arrowHead, centerY - arrowHead, 3,
                      !invert);
    renderer.drawLine(centerX - arrowLen / 2, centerY, centerX - arrowLen / 2 + arrowHead, centerY + arrowHead, 3,
                      !invert);
    return;
  }

  const bool hasSecondary = secondaryLabel != nullptr && secondaryLabel[0] != '\0';
  const int itemWidth = renderer.getTextWidth(UI_12_FONT_ID, label);
  const int textX = rect.x + (rect.width - itemWidth) / 2;
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;

  renderer.drawText(UI_12_FONT_ID, textX, textY, label, !invert);

  if (hasSecondary) {
    const int secWidth = renderer.getTextWidth(SMALL_FONT_ID, secondaryLabel);
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - secWidth - 1, rect.y, secondaryLabel, !invert);
  }
}
