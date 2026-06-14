#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "icons/Icons.h"

class GfxRenderer;

struct Rect {
  int x;
  int y;
  int width;
  int height;

  explicit Rect(int x = 0, int y = 0, int width = 0, int height = 0) : x(x), y(y), width(width), height(height) {}
};

struct TabInfo {
  const char* label;
  bool selected;
};

struct ThemeMetrics {
  int batteryWidth;
  int batteryHeight;

  int topPadding;
  int batteryBarHeight;
  int headerHeight;
  int pageTitleHeight;
  int verticalSpacing;

  int contentSidePadding;
  int listRowHeight;
  int listWithSubtitleRowHeight;
  int menuRowHeight;
  int menuSpacing;

  int tabSpacing;
  int tabBarHeight;

  int scrollBarWidth;
  int scrollBarRightOffset;

  int homeTopPadding;
  int homeCoverHeight;
  int homeCoverTileHeight;
  int homeRecentBooksCount;
  bool homeContinueReadingInMenu;
  int homeMenuTopOffset;

  int buttonHintsHeight;
  int sideButtonHintsWidth;

  int progressBarHeight;
  int progressBarMarginTop;
  int statusBarHorizontalMargin;
  int statusBarVerticalMargin;

  int keyboardKeyWidth;
  int keyboardKeyHeight;
  int keyboardKeySpacing;
  int keyboardBottomKeyHeight;
  int keyboardBottomKeySpacing;
  bool keyboardBottomAligned;
  bool keyboardCenteredText;
  int keyboardVerticalOffset;
  int keyboardTextFieldWidthPercent;
  int keyboardWidthPercent;
  int keyboardKeyCornerRadius;
};

enum class KeyboardKeyType { Normal, Shift, Mode, Space, Del, Ok, Disabled };

// Lyra metrics — the single concrete metrics used by MyneUI
namespace MyneUIMetrics {
constexpr ThemeMetrics values = {.batteryWidth = 16,
                                 .batteryHeight = 12,
                                 .topPadding = 5,
                                 .batteryBarHeight = 40,
                                 .headerHeight = 40,
                                 .pageTitleHeight = 52,
                                 .verticalSpacing = 16,
                                 .contentSidePadding = 20,
                                 .listRowHeight = 40,
                                 .listWithSubtitleRowHeight = 60,
                                 .menuRowHeight = 64,
                                 .menuSpacing = 8,
                                 .tabSpacing = 8,
                                 .tabBarHeight = 40,
                                 .scrollBarWidth = 4,
                                 .scrollBarRightOffset = 5,
                                 .homeTopPadding = 56,
                                 .homeCoverHeight = 226,
                                 .homeCoverTileHeight = 242,
                                 .homeRecentBooksCount = 1,
                                 .homeContinueReadingInMenu = false,
                                 .homeMenuTopOffset = 16,
                                 .buttonHintsHeight = 40,
                                 .sideButtonHintsWidth = 30,
                                 .progressBarHeight = 16,
                                 .progressBarMarginTop = 1,
                                 .statusBarHorizontalMargin = 5,
                                 .statusBarVerticalMargin = 19,
                                 .keyboardKeyWidth = 31,
                                 .keyboardKeyHeight = 40,
                                 .keyboardKeySpacing = 0,
                                 .keyboardBottomKeyHeight = 35,
                                 .keyboardBottomKeySpacing = 5,
                                 .keyboardBottomAligned = true,
                                 .keyboardCenteredText = false,
                                 .keyboardVerticalOffset = -7,
                                 .keyboardTextFieldWidthPercent = 85,
                                 .keyboardWidthPercent = 90,
                                 .keyboardKeyCornerRadius = 6};
}

// Flat concrete UI class — no inheritance, no virtual methods.
// Merges BaseTheme + LyraTheme with Lyra's overrides taking precedence.
class MyneUI {
 public:
  // Component drawing methods
  void drawProgressBar(const GfxRenderer& renderer, Rect rect, size_t current, size_t total) const;
  void drawBatteryLeft(const GfxRenderer& renderer, Rect rect, bool showPercentage = true) const;
  void drawBatteryRight(const GfxRenderer& renderer, Rect rect, bool showPercentage = true) const;
  void fillBatteryIcon(const GfxRenderer& renderer, Rect rect, uint16_t percentage) const;  // Lyra version
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const;  // Lyra version
  void drawSideButtonHints(const GfxRenderer& renderer, const char* topBtn,
                           const char* bottomBtn) const;  // Lyra version
  void drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                const std::function<std::string(int index)>& rowTitle,
                const std::function<std::string(int index)>& rowSubtitle = nullptr,
                const std::function<UIIcon(int index)>& rowIcon = nullptr,
                const std::function<std::string(int index)>& rowValue = nullptr, bool highlightValue = false,
                const std::function<bool(int index)>& rowDimmed = nullptr) const;  // Lyra version
  void drawHeader(const GfxRenderer& renderer, Rect rect) const;                   // battery-only strip
  void drawPageTitle(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr) const;
  void drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                     const char* rightLabel = nullptr) const;  // Lyra version
  void drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                  bool selected) const;  // Lyra version
  void drawButtonMenu(const GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const;  // Lyra version
  void drawIconGrid(const GfxRenderer& renderer, Rect rect, int count, int selectedIndex,
                    const std::function<std::string(int index)>& labelFn,
                    const std::function<UIIcon(int index)>& iconFn) const;

  // --- Carousel cards ---

  struct CarouselMenuCard {
    UIIcon icon;
    const char* label;
    const char* description;
  };

  struct CarouselBookCard {
    const char* sectionLabel;  // e.g. tr(STR_LAST_READ)
    const char* description;   // shown when no book is loaded
    const char* title;         // nullptr = no book
    const char* author;        // nullptr if no book
    const char* progress;      // "p. 42" or nullptr
    const char* date;          // "Jan '26" or nullptr
    const char* status;        // e.g. tr(STR_STATUS_READING) or nullptr
  };

  void drawCarouselMenuCard(const GfxRenderer& renderer, Rect rect, const CarouselMenuCard& card) const;
  void drawCarouselBookCard(GfxRenderer& renderer, Rect rect, const CarouselBookCard& card) const;
  Rect drawPopup(const GfxRenderer& renderer, const char* message) const;                       // Lyra version
  void fillPopupProgress(const GfxRenderer& renderer, const Rect& layout, int progress) const;  // Lyra version
  void drawStatusBar(GfxRenderer& renderer, float bookProgress, int currentPage, int pageCount, std::string title,
                     int paddingBottom = 0, int textYOffset = 0) const;                // BaseTheme version
  void drawHelpText(const GfxRenderer& renderer, Rect rect, const char* label) const;  // BaseTheme version
  void drawTextField(const GfxRenderer& renderer, Rect rect, int textWidth, bool cursorMode = false,
                     int contentStartX = 0, int contentWidth = 0) const;  // BaseTheme version
  void drawKeyboardKey(const GfxRenderer& renderer, Rect rect, const char* label, bool isSelected,
                       const char* secondaryLabel = nullptr, KeyboardKeyType keyType = KeyboardKeyType::Normal,
                       bool inactiveSelection = false) const;  // BaseTheme version

  // Shared static helpers for battery drawing
  static constexpr int batteryPercentSpacing = 4;
  static void drawBatteryOutline(const GfxRenderer& renderer, int x, int y, int battWidth, int rectHeight);
  static void drawBatteryLightningBolt(const GfxRenderer& renderer, int boltX, int boltY);
};
