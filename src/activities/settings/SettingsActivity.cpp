#include "SettingsActivity.h"

#include <GfxRenderer.h>
#include <Logging.h>

#include <algorithm>

#include "ButtonRemapActivity.h"
#include "LanguageSelectActivity.h"
#include "MappedInputManager.h"
#include "MyneSettings.h"
#ifndef SIMULATOR
#include "OtaUpdateActivity.h"
#include "SdFirmwareUpdateActivity.h"
#endif
#include "SettingsActivityUI.h"
#include "SettingsList.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

const StrId SettingsActivity::categoryNames[categoryCount] = {StrId::STR_CAT_DISPLAY, StrId::STR_CAT_CONTROLS,
                                                              StrId::STR_CAT_SYSTEM};

void SettingsActivity::rebuildSettingsLists() {
  displaySettings.clear();
  controlsSettings.clear();
  systemSettings.clear();

  for (auto& setting : getSettingsList()) {
    if (setting.category == StrId::STR_NONE_OPT) continue;
    if (setting.category == StrId::STR_CAT_DISPLAY) {
      displaySettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_CONTROLS) {
      controlsSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_SYSTEM) {
      systemSettings.push_back(setting);
    }
  }

  // Append device-only ACTION items
  controlsSettings.insert(controlsSettings.begin(),
                          SettingInfo::Action(StrId::STR_REMAP_FRONT_BUTTONS, SettingAction::RemapFrontButtons));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_WIFI_NETWORKS, SettingAction::Network));
#ifndef SIMULATOR
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CHECK_UPDATES, SettingAction::CheckForUpdates));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_SD_FIRMWARE_UPDATE, SettingAction::SdFirmwareUpdate));
#endif
  systemSettings.push_back(SettingInfo::Action(StrId::STR_LANGUAGE, SettingAction::Language));

  switch (selectedCategoryIndex) {
    case 0:
      currentSettings = &displaySettings;
      break;
    case 1:
      currentSettings = &controlsSettings;
      break;
    case 2:
      currentSettings = &systemSettings;
      break;
  }
  settingsCount = static_cast<int>(currentSettings->size());
}

void SettingsActivity::onEnter() {
  Activity::onEnter();

  selectedCategoryIndex = 0;
  selectedSettingIndex = 0;

  rebuildSettingsLists();

  requestUpdate();
}

void SettingsActivity::onExit() {
  Activity::onExit();
  UITheme::getInstance().reload();
}

void SettingsActivity::loop() {
  bool hasChangedCategory = false;

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selectedSettingIndex == 0) {
      selectedCategoryIndex = (selectedCategoryIndex < categoryCount - 1) ? (selectedCategoryIndex + 1) : 0;
      hasChangedCategory = true;
      requestUpdate();
    } else {
      toggleCurrentSetting();
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (selectedSettingIndex > 0) {
      selectedSettingIndex = 0;
      requestUpdate();
    } else {
      SETTINGS.saveToFile();
      onGoHome();
    }
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedSettingIndex = ButtonNavigator::nextIndex(selectedSettingIndex, settingsCount + 1);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedSettingIndex = ButtonNavigator::previousIndex(selectedSettingIndex, settingsCount + 1);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, &hasChangedCategory] {
    hasChangedCategory = true;
    selectedCategoryIndex = ButtonNavigator::nextIndex(selectedCategoryIndex, categoryCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, &hasChangedCategory] {
    hasChangedCategory = true;
    selectedCategoryIndex = ButtonNavigator::previousIndex(selectedCategoryIndex, categoryCount);
    requestUpdate();
  });

  if (hasChangedCategory) {
    selectedSettingIndex = (selectedSettingIndex == 0) ? 0 : 1;
    switch (selectedCategoryIndex) {
      case 0:
        currentSettings = &displaySettings;
        break;
      case 1:
        currentSettings = &controlsSettings;
        break;
      case 2:
        currentSettings = &systemSettings;
        break;
    }
    settingsCount = static_cast<int>(currentSettings->size());
  }
}

void SettingsActivity::toggleCurrentSetting() {
  int selectedSetting = selectedSettingIndex - 1;
  if (selectedSetting < 0 || selectedSetting >= settingsCount) {
    return;
  }

  const auto& setting = (*currentSettings)[selectedSetting];

  if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
    const bool currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = !currentValue;
  } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
    const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
    const uint8_t total = !setting.enumStringValues.empty() ? static_cast<uint8_t>(setting.enumStringValues.size())
                                                            : static_cast<uint8_t>(setting.enumValues.size());
    SETTINGS.*(setting.valuePtr) = (currentValue + 1) % total;
  } else if (setting.type == SettingType::ENUM && setting.valueGetter && setting.valueSetter) {
    const uint8_t totalValues = setting.enumStringValues.empty()
                                    ? static_cast<uint8_t>(setting.enumValues.size())
                                    : static_cast<uint8_t>(setting.enumStringValues.size());
    const uint8_t cur = setting.valueGetter();
    setting.valueSetter((cur + 1) % totalValues);
  } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
    const int8_t currentValue = SETTINGS.*(setting.valuePtr);
    if (currentValue + setting.valueRange.step > setting.valueRange.max) {
      SETTINGS.*(setting.valuePtr) = setting.valueRange.min;
    } else {
      SETTINGS.*(setting.valuePtr) = currentValue + setting.valueRange.step;
    }
  } else if (setting.type == SettingType::ACTION) {
    auto resultHandler = [this](const ActivityResult&) { SETTINGS.saveToFile(); };

    switch (setting.action) {
      case SettingAction::RemapFrontButtons:
        startActivityForResult(std::make_unique<ButtonRemapActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::Network:
        startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, false), resultHandler);
        break;
#ifndef SIMULATOR
      case SettingAction::CheckForUpdates:
        startActivityForResult(std::make_unique<OtaUpdateActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::SdFirmwareUpdate:
        startActivityForResult(std::make_unique<SdFirmwareUpdateActivity>(renderer, mappedInput), resultHandler);
        break;
#endif
      case SettingAction::Language:
        startActivityForResult(std::make_unique<LanguageSelectActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::None:
        break;
      default:
        break;
    }
    return;
  } else {
    return;
  }

  SETTINGS.saveToFile();
}

void SettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});

  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  SettingsActivityUI::hero(renderer, Rect{SettingsActivityUI::PAD, heroY, pageWidth - SettingsActivityUI::PAD * 2, 104},
                           tr(STR_SETTINGS_TITLE), I18N.get(categoryNames[selectedCategoryIndex]), MYNE_VERSION);

  const int catY = heroY + 124;
  const int catW = (pageWidth - SettingsActivityUI::PAD * 2 - SettingsActivityUI::GAP * 2) / categoryCount;
  for (int i = 0; i < categoryCount; ++i) {
    SettingsActivityUI::choice(renderer,
                               Rect{SettingsActivityUI::PAD + i * (catW + SettingsActivityUI::GAP), catY, catW, 68},
                               I18N.get(categoryNames[i]), "", selectedCategoryIndex == i && selectedSettingIndex == 0);
  }

  const auto& settings = *currentSettings;
  auto valueForSetting = [&settings](int i) {
    const auto& setting = settings[i];
    std::string valueText;
    if (setting.type == SettingType::ACTION) {
      valueText = tr(STR_OPEN);
    } else if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
      const bool value = SETTINGS.*(setting.valuePtr);
      valueText = value ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
    } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
      const uint8_t value = SETTINGS.*(setting.valuePtr);
      if (!setting.enumStringValues.empty() && value < setting.enumStringValues.size()) {
        valueText = setting.enumStringValues[value];
      } else if (!setting.enumValues.empty() && value < setting.enumValues.size()) {
        valueText = I18N.get(setting.enumValues[value]);
      }
    } else if (setting.type == SettingType::ENUM && setting.valueGetter) {
      const uint8_t value = setting.valueGetter();
      if (!setting.enumStringValues.empty() && value < setting.enumStringValues.size()) {
        valueText = setting.enumStringValues[value];
      } else if (value < setting.enumValues.size()) {
        valueText = I18N.get(setting.enumValues[value]);
      }
    } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
      valueText = std::to_string(SETTINGS.*(setting.valuePtr));
    }
    return valueText;
  };

  const int listTop = catY + 84;
  const int listBottom = pageHeight - metrics.buttonHintsHeight - 20;
  constexpr int rowH = 58;
  constexpr int rowGap = 10;
  const int maxRows = std::max(1, (listBottom - listTop + rowGap) / (rowH + rowGap));
  int start = 0;
  const int listIndex = selectedSettingIndex - 1;
  if (listIndex >= maxRows) start = listIndex - maxRows + 1;
  const int end = std::min(settingsCount, start + maxRows);
  for (int i = start; i < end; ++i) {
    const bool selected = selectedSettingIndex == i + 1;
    const int rowY = listTop + (i - start) * (rowH + rowGap);
    const std::string value = valueForSetting(i);
    SettingsActivityUI::option(renderer,
                               Rect{SettingsActivityUI::PAD, rowY, pageWidth - SettingsActivityUI::PAD * 2, rowH},
                               I18N.get(settings[i].nameId), value.c_str(), selected);
  }

  const auto confirmLabel = (selectedSettingIndex == 0)
                                ? I18N.get(categoryNames[(selectedCategoryIndex + 1) % categoryCount])
                                : tr(STR_TOGGLE);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
