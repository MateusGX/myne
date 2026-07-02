#include "OtaUpdateActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <WiFi.h>

#include <cstddef>
#include <cstdio>
#include <string>

#include "MappedInputManager.h"
#include "SettingsActivityUI.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/OtaUpdater.h"

namespace {
std::string formatBytes(std::size_t bytes) {
  char buf[24];
  if (bytes >= 1024 * 1024) {
    std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  } else if (bytes >= 1024) {
    std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
  } else {
    std::snprintf(buf, sizeof(buf), "%u B", static_cast<unsigned>(bytes));
  }
  return buf;
}

void drawUpdateProgress(const GfxRenderer& renderer, const Rect& card, int percent, const char* title,
                        const char* detail, const ThemeMetrics& metrics) {
  SettingsActivityUI::panel(renderer, card, true);
  const int x = card.x + SettingsActivityUI::INNER + 8;
  const int w = card.width - SettingsActivityUI::INNER * 2 - 8;

  SettingsActivityUI::text(renderer, UI_10_FONT_ID, x, card.y + 26, title, w, EpdFontFamily::BOLD);
  GUI.drawProgressBar(renderer, Rect{x, card.y + 78, w, metrics.progressBarHeight}, percent, 100);

  if (detail && detail[0] != '\0') {
    renderer.drawCenteredText(SMALL_FONT_ID, card.y + 142, detail);
  }
}
}  // namespace

void OtaUpdateActivity::onWifiSelectionComplete(const bool success) {
  if (!success) {
    LOG_ERR("OTA", "WiFi connection failed, exiting");
    finish();
    return;
  }

  LOG_DBG("OTA", "WiFi connected, checking for update");

  {
    RenderLock lock(*this);
    state = CHECKING_FOR_UPDATE;
  }
  requestUpdateAndWait();

  const auto res = updater.checkForUpdate();
  if (res != OtaUpdater::OK) {
    LOG_DBG("OTA", "Update check failed: %d", res);
    {
      RenderLock lock(*this);
      state = FAILED;
    }
    return;
  }

  if (!updater.isUpdateNewer()) {
    LOG_DBG("OTA", "No new update available");
    {
      RenderLock lock(*this);
      state = NO_UPDATE;
    }
    return;
  }

  {
    RenderLock lock(*this);
    state = WAITING_CONFIRMATION;
  }
}

void OtaUpdateActivity::onEnter() {
  Activity::onEnter();

  // Turn on WiFi immediately
  LOG_DBG("OTA", "Turning on WiFi...");
  WiFi.mode(WIFI_STA);

  // Launch WiFi selection subactivity
  LOG_DBG("OTA", "Launching WifiSelectionActivity...");
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void OtaUpdateActivity::onExit() {
  Activity::onExit();

  // Turn off wifi
  WiFi.disconnect(false);  // false = don't erase credentials, send disconnect frame
  delay(100);              // Allow disconnect frame to be sent
  WiFi.mode(WIFI_OFF);
  delay(100);  // Allow WiFi hardware to fully power down
}

void OtaUpdateActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  float updaterProgress = 0;
  if (state == UPDATE_IN_PROGRESS) {
    LOG_DBG("OTA", "Update progress: %d / %d", updater.getProcessedSize(), updater.getTotalSize());
    updaterProgress = updater.getTotalSize() > 0
                          ? static_cast<float>(updater.getProcessedSize()) / static_cast<float>(updater.getTotalSize())
                          : 0;
    // Only update every 2% at the most
    if (static_cast<int>(updaterProgress * 50) == lastUpdaterPercentage / 2) {
      return;
    }
    lastUpdaterPercentage = static_cast<int>(updaterProgress * 100);
  }

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});
  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  SettingsActivityUI::hero(renderer, Rect{SettingsActivityUI::PAD, heroY, pageWidth - SettingsActivityUI::PAD * 2, 104},
                           tr(STR_SETTINGS_TITLE), tr(STR_UPDATE), MYNE_VERSION);

  const int contentY = heroY + 124;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 16;
  const int cardW = pageWidth - SettingsActivityUI::PAD * 2;
  const int stateCardH = contentBottom > contentY ? contentBottom - contentY : 160;
  const Rect stateCard{SettingsActivityUI::PAD, contentY, cardW, stateCardH};

  if (state == CHECKING_FOR_UPDATE) {
    SettingsActivityUI::stateCard(renderer, stateCard, tr(STR_CHECKING_UPDATE));
  } else if (state == WAITING_CONFIRMATION) {
    const Rect notice{SettingsActivityUI::PAD, contentY, cardW, 68};
    SettingsActivityUI::panel(renderer, notice, true);
    SettingsActivityUI::text(renderer, UI_10_FONT_ID, notice.x + SettingsActivityUI::INNER + 8, notice.y + 18,
                             tr(STR_NEW_UPDATE), notice.width - SettingsActivityUI::INNER * 2 - 8,
                             EpdFontFamily::BOLD);

    const int versionY = notice.y + notice.height + SettingsActivityUI::GAP;
    const int rowH = 58;
    SettingsActivityUI::option(
        renderer, Rect{SettingsActivityUI::PAD, versionY, cardW, rowH},
        tr(STR_CURRENT_VERSION), MYNE_VERSION);
    SettingsActivityUI::option(
        renderer, Rect{SettingsActivityUI::PAD, versionY + rowH + SettingsActivityUI::GAP, cardW, rowH},
        tr(STR_NEW_VERSION), updater.getLatestVersion().c_str());

    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_UPDATE), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == UPDATE_IN_PROGRESS) {
    const Rect progressCard{SettingsActivityUI::PAD, contentY, cardW, 184};
    const std::string bytes = formatBytes(updater.getProcessedSize()) + " / " + formatBytes(updater.getTotalSize());
    drawUpdateProgress(renderer, progressCard, static_cast<int>(updaterProgress * 100), tr(STR_UPDATING),
                       bytes.c_str(), metrics);
  } else if (state == NO_UPDATE) {
    SettingsActivityUI::stateCard(renderer, stateCard, tr(STR_NO_UPDATE), MYNE_VERSION);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == FAILED) {
    SettingsActivityUI::stateCard(renderer, stateCard, tr(STR_UPDATE_FAILED));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == FINISHED) {
    SettingsActivityUI::stateCard(renderer, stateCard, tr(STR_UPDATE_COMPLETE), tr(STR_POWER_ON_HINT));
  }

  renderer.displayBuffer();
}

void OtaUpdateActivity::loop() {
  if (state == WAITING_CONFIRMATION) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      LOG_DBG("OTA", "New update available, starting download...");
      {
        RenderLock lock(*this);
        state = UPDATE_IN_PROGRESS;
      }
      requestUpdateAndWait();
      const auto res = updater.installUpdate(
          [](void* ctx) {
            // immediate=true notifies the render task directly. The default deferred path only
            // sets a flag consumed at the end of ActivityManager::loop(), which never runs while
            // installUpdate() blocks this task.
            static_cast<OtaUpdateActivity*>(ctx)->requestUpdate(true);
          },
          this);

      if (res != OtaUpdater::OK) {
        LOG_DBG("OTA", "Update failed: %d", res);
        {
          RenderLock lock(*this);
          state = FAILED;
        }
        requestUpdate();
        return;
      }

      {
        RenderLock lock(*this);
        state = FINISHED;
      }
      requestUpdateAndWait();
      // Hold the completion screen briefly so the user sees it, then restart.
      delay(3000);
      {
        RenderLock lock(*this);
        state = SHUTTING_DOWN;
      }
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }

    return;
  }

  if (state == FAILED || state == NO_UPDATE) {
    if (mappedInput.wasReleasedGroup(MappedInputManager::ButtonGroup::BottomLeft)) {
      finish();
    }
    return;
  }

  if (state == SHUTTING_DOWN) {
    ESP.restart();
  }
}
