#pragma once

#include <functional>

#include "components/MyneUI.h"

class UITheme {
  static UITheme instance;
  MyneUI theme;

 public:
  static UITheme& getInstance() { return instance; }

  const ThemeMetrics& getMetrics() const { return MyneUIMetrics::values; }
  const MyneUI& getTheme() const { return theme; }
  void reload() {}

  static int getNumberOfItemsPerPage(const GfxRenderer& renderer, bool hasHeader, bool hasTabBar, bool hasButtonHints,
                                     bool hasSubtitle, int extraReservedHeight = 0);
  static std::string getCoverThumbPath(std::string coverBmpPath, int coverHeight);
  static UIIcon getFileIcon(const std::string& filename);
  static int getStatusBarHeight();
  static int getProgressBarHeight();
};

#define GUI UITheme::getInstance().getTheme()
