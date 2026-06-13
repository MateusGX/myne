#include "LanguageSelectActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <iterator>

#include "MyneSettings.h"
#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "SettingsActivityUI.h"
#include "components/UITheme.h"
#include "fontIds.h"

void LanguageSelectActivity::onEnter() {
  Activity::onEnter();

  // Set current selection based on current language
  const auto currentLang = static_cast<uint8_t>(I18N.getLanguage());
  const auto* begin = std::begin(SORTED_LANGUAGE_INDICES);
  const auto* end = std::end(SORTED_LANGUAGE_INDICES);
  const auto* it = std::find(begin, end, currentLang);
  selectedIndex = (it != end) ? std::distance(begin, it) : 0;

  requestUpdate();
}

void LanguageSelectActivity::onExit() { Activity::onExit(); }

void LanguageSelectActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  // Handle navigation
  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(static_cast<int>(selectedIndex), totalItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(static_cast<int>(selectedIndex), totalItems);
    requestUpdate();
  });
}

void LanguageSelectActivity::handleSelection() {
  const uint8_t langIndex = SORTED_LANGUAGE_INDICES[selectedIndex];

  {
    RenderLock lock(*this);
    I18N.setLanguage(static_cast<Language>(langIndex));
  }

  SETTINGS.language = langIndex;
  SETTINGS.saveToFile();

  // Return to previous page
  onBack();
}

void LanguageSelectActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});
  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  SettingsActivityUI::hero(renderer,
                           Rect{SettingsActivityUI::PAD, heroY,
                                pageWidth - SettingsActivityUI::PAD * 2, 104},
                           tr(STR_SETTINGS_TITLE), tr(STR_LANGUAGE), tr(STR_SELECT));

  // Current language marker
  const auto currentLang = static_cast<uint8_t>(I18N.getLanguage());
  const int listTop = heroY + 124;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - 20;
  constexpr int rowH = 58;
  constexpr int rowGap = 10;
  const int maxRows = std::max(1, (listBottom - listTop + rowGap) / (rowH + rowGap));
  int start = 0;
  if (selectedIndex >= maxRows) start = selectedIndex - maxRows + 1;
  const int end = std::min(static_cast<int>(totalItems), start + maxRows);
  for (int i = start; i < end; ++i) {
    const uint8_t langIndex = SORTED_LANGUAGE_INDICES[i];
    const char* state = langIndex == currentLang ? tr(STR_SELECTED) : "";
    SettingsActivityUI::option(
        renderer,
        Rect{SettingsActivityUI::PAD, listTop + (i - start) * (rowH + rowGap),
             pageWidth - SettingsActivityUI::PAD * 2, rowH},
        I18N.getLanguageName(static_cast<Language>(langIndex)), state, i == selectedIndex);
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
