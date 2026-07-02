#include "TimezoneSelectActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "MyneSettings.h"
#include "SettingsActivityUI.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

std::vector<std::string> buildTimezoneLabels() {
  std::vector<std::string> labels;
  labels.reserve(27);
  for (int h = -12; h <= 14; ++h) {
    char buf[10];
    if (h > 0)
      snprintf(buf, sizeof(buf), "UTC+%d", h);
    else if (h < 0)
      snprintf(buf, sizeof(buf), "UTC%d", h);
    else
      snprintf(buf, sizeof(buf), "UTC+0");
    labels.push_back(buf);
  }
  return labels;
}

}  // namespace

void TimezoneSelectActivity::onEnter() {
  Activity::onEnter();

  // Set current selection based on current timezone offset (index == value)
  selectedIndex = std::min<int>(SETTINGS.timezoneOffset, totalItems - 1);

  requestUpdate();
}

void TimezoneSelectActivity::onExit() { Activity::onExit(); }

void TimezoneSelectActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  // Handle navigation
  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, totalItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, totalItems);
    requestUpdate();
  });
}

void TimezoneSelectActivity::handleSelection() {
  SETTINGS.timezoneOffset = static_cast<uint8_t>(selectedIndex);
  SETTINGS.saveToFile();

  // Return to previous page
  onBack();
}

void TimezoneSelectActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});
  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  SettingsActivityUI::hero(renderer, Rect{SettingsActivityUI::PAD, heroY, pageWidth - SettingsActivityUI::PAD * 2, 104},
                           tr(STR_SETTINGS_TITLE), tr(STR_TIMEZONE_OFFSET), tr(STR_SELECT));

  // Current timezone marker
  const auto labels = buildTimezoneLabels();
  const uint8_t currentOffset = SETTINGS.timezoneOffset;
  const int listTop = heroY + 124;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - 20;
  constexpr int rowH = 58;
  constexpr int rowGap = 10;
  const int maxRows = std::max(1, (listBottom - listTop + rowGap) / (rowH + rowGap));
  int start = 0;
  if (selectedIndex >= maxRows) start = selectedIndex - maxRows + 1;
  const int end = std::min(static_cast<int>(totalItems), start + maxRows);
  for (int i = start; i < end; ++i) {
    const char* state = (i == currentOffset) ? tr(STR_SELECTED) : "";
    SettingsActivityUI::option(renderer,
                               Rect{SettingsActivityUI::PAD, listTop + (i - start) * (rowH + rowGap),
                                    pageWidth - SettingsActivityUI::PAD * 2, rowH},
                               labels[i].c_str(), state, i == selectedIndex);
  }

  // Button hints
  const auto labelsHint = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labelsHint.btn1, labelsHint.btn2, labelsHint.btn3, labelsHint.btn4);

  renderer.displayBuffer();
}
