#include "NetworkModeSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "NetworkActivityUI.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int MENU_ITEM_COUNT = 2;
}  // namespace

void NetworkModeSelectionActivity::onEnter() {
  Activity::onEnter();

  // Reset selection
  selectedIndex = 0;

  // Trigger first update
  requestUpdate();
}

void NetworkModeSelectionActivity::onExit() { Activity::onExit(); }

void NetworkModeSelectionActivity::loop() {
  // Handle back button - cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
    return;
  }

  // Handle confirm button - select current option
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    NetworkMode mode = NetworkMode::JOIN_NETWORK;
    if (selectedIndex == 1) {
      mode = NetworkMode::CREATE_HOTSPOT;
    }
    onModeSelected(mode);
    return;
  }

  // Handle navigation
  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, MENU_ITEM_COUNT);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, MENU_ITEM_COUNT);
    requestUpdate();
  });
}

void NetworkModeSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});

  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  NetworkActivityUI::hero(renderer, Rect{NetworkActivityUI::PAD, heroY, pageWidth - NetworkActivityUI::PAD * 2, 104},
                          tr(STR_NETWORK), tr(STR_NETWORK), tr(STR_SELECT));

  const int contentY = heroY + 124;
  const int cardW = pageWidth - NetworkActivityUI::PAD * 2;
  const int cardH = 116;
  NetworkActivityUI::choice(renderer, Rect{NetworkActivityUI::PAD, contentY, cardW, cardH}, tr(STR_JOIN_NETWORK),
                            tr(STR_JOIN_DESC), selectedIndex == 0);
  NetworkActivityUI::choice(renderer,
                            Rect{NetworkActivityUI::PAD, contentY + cardH + NetworkActivityUI::GAP, cardW, cardH},
                            tr(STR_CREATE_HOTSPOT), tr(STR_HOTSPOT_DESC), selectedIndex == 1);

  const int footerY = pageHeight - metrics.buttonHintsHeight - 84;
  if (footerY > contentY + cardH * 2 + NetworkActivityUI::GAP) {
    NetworkActivityUI::stateCard(renderer,
                                 Rect{NetworkActivityUI::PAD, footerY, pageWidth - NetworkActivityUI::PAD * 2, 64},
                                 selectedIndex == 0 ? tr(STR_WIFI_NETWORKS) : tr(STR_HOTSPOT_MODE));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void NetworkModeSelectionActivity::onModeSelected(NetworkMode mode) {
  setResult(NetworkModeResult{mode});
  finish();
}

void NetworkModeSelectionActivity::onCancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
