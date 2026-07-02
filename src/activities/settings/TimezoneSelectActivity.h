#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

#include "../Activity.h"
#include "components/UITheme.h"
#include "util/ButtonNavigator.h"

class MappedInputManager;

/**
 * Activity for selecting the device timezone offset
 */
class TimezoneSelectActivity final : public Activity {
 public:
  explicit TimezoneSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TimezoneSelect", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void handleSelection();

  void onBack() { finish(); }
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  // Matches MyneSettings::timezoneOffset's range: 0 = UTC-12 .. 26 = UTC+14.
  static constexpr uint8_t totalItems = 27;
};
