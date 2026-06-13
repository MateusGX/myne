#pragma once

#include <I18n.h>

#include <cstdio>
#include <vector>

#include "MyneSettings.h"
#include "activities/settings/SettingsActivity.h"

namespace {

inline std::vector<std::string> buildTimezoneLabels() {
  std::vector<std::string> labels;
  labels.reserve(27);
  for (int h = -12; h <= 14; ++h) {
    char buf[10];
    if (h > 0) snprintf(buf, sizeof(buf), "UTC+%d", h);
    else if (h < 0) snprintf(buf, sizeof(buf), "UTC%d", h);
    else snprintf(buf, sizeof(buf), "UTC+0");
    labels.push_back(buf);
  }
  return labels;
}

}  // namespace

inline std::vector<SettingInfo> getSettingsList() {
  static const std::vector<SettingInfo> baseList = [] {
    std::vector<SettingInfo> v = {
        // --- Display ---
        SettingInfo::Enum(StrId::STR_SLEEP_SCREEN, &MyneSettings::sleepScreen,
                          {StrId::STR_DARK, StrId::STR_LIGHT, StrId::STR_CUSTOM, StrId::STR_COVER, StrId::STR_NONE_OPT,
                           StrId::STR_COVER_CUSTOM},
                          "sleepScreen", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_SLEEP_COVER_MODE, &MyneSettings::sleepScreenCoverMode,
                          {StrId::STR_FIT, StrId::STR_CROP}, "sleepScreenCoverMode", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_SLEEP_COVER_FILTER, &MyneSettings::sleepScreenCoverFilter,
                          {StrId::STR_NONE_OPT, StrId::STR_FILTER_CONTRAST, StrId::STR_INVERTED},
                          "sleepScreenCoverFilter", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_HIDE_BATTERY, &MyneSettings::hideBatteryPercentage,
                          {StrId::STR_NEVER, StrId::STR_IN_READER, StrId::STR_ALWAYS}, "hideBatteryPercentage",
                          StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(
            StrId::STR_REFRESH_FREQ, &MyneSettings::refreshFrequency,
            {StrId::STR_PAGES_1, StrId::STR_PAGES_5, StrId::STR_PAGES_10, StrId::STR_PAGES_15, StrId::STR_PAGES_30},
            "refreshFrequency", StrId::STR_CAT_DISPLAY),
        SettingInfo::Toggle(StrId::STR_SUNLIGHT_FADING_FIX, &MyneSettings::fadingFix, "fadingFix",
                            StrId::STR_CAT_DISPLAY),

        // --- System ---
        SettingInfo::Enum(StrId::STR_TIME_TO_SLEEP, &MyneSettings::sleepTimeout,
                          {StrId::STR_MIN_1, StrId::STR_MIN_5, StrId::STR_MIN_10, StrId::STR_MIN_15, StrId::STR_MIN_30},
                          "sleepTimeout", StrId::STR_CAT_SYSTEM),
        SettingInfo::Toggle(StrId::STR_SHOW_HIDDEN_FILES, &MyneSettings::showHiddenFiles, "showHiddenFiles",
                            StrId::STR_CAT_SYSTEM),
        SettingInfo::EnumStrings(StrId::STR_TIMEZONE_OFFSET, &MyneSettings::timezoneOffset,
                                 buildTimezoneLabels(), "timezoneOffset", StrId::STR_CAT_SYSTEM),
    };
    return v;
  }();

  return baseList;
}
