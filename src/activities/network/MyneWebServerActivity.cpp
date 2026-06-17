#include "MyneWebServerActivity.h"

#include <BookCatalog.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include <cstddef>

#include "../books/CatalogSyncActivity.h"
#include "MappedInputManager.h"
#include "NetworkActivityUI.h"
#include "NetworkModeSelectionActivity.h"
#include "WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/QrUtils.h"

namespace {
void goHomeOrSync(GfxRenderer& r, MappedInputManager& m) {
  if (Storage.exists(BookCatalog::SYNC_FLAG_PATH)) {
    activityManager.replaceActivity(std::make_unique<CatalogSyncActivity>(r, m));
  } else {
    activityManager.goHome();
  }
}
}  // namespace

namespace {
// AP Mode configuration
constexpr const char* AP_SSID = "Myne";
constexpr const char* AP_PASSWORD = nullptr;  // Open network for ease of use
constexpr const char* AP_HOSTNAME = "myne";
constexpr uint8_t AP_CHANNEL = 1;
constexpr uint8_t AP_MAX_CONNECTIONS = 4;

// DNS server for captive portal (redirects all DNS queries to our IP)
DNSServer* dnsServer = nullptr;
constexpr uint16_t DNS_PORT = 53;

// 0..4 bars from RSSI (dBm), with 3 dBm hysteresis on currentBars to suppress flicker.
int barsForRssi(int rssi, int currentBars) {
  static constexpr int RISE_DBM[] = {-85, -75, -65, -55};
  static constexpr int FALL_DBM[] = {-88, -78, -68, -58};
  int bars = std::clamp(currentBars, 0, 4);
  while (bars < 4 && rssi >= RISE_DBM[bars]) bars++;
  while (bars > 0 && rssi < FALL_DBM[bars - 1]) bars--;
  return bars;
}

// Wifi signal icon drawn in the top-right corner of the hero panel.
constexpr int WIFI_ICON_BAR_COUNT = 4;
constexpr int WIFI_ICON_BAR_WIDTH = 4;
constexpr int WIFI_ICON_BAR_GAP = 2;
constexpr int WIFI_ICON_HEIGHT = 14;
constexpr int WIFI_ICON_WIDTH =
    WIFI_ICON_BAR_COUNT * WIFI_ICON_BAR_WIDTH + (WIFI_ICON_BAR_COUNT - 1) * WIFI_ICON_BAR_GAP;
constexpr int WIFI_ICON_RIGHT_RESERVE = WIFI_ICON_WIDTH + NetworkActivityUI::INNER + 8;
}  // namespace

void MyneWebServerActivity::onEnter() {
  Activity::onEnter();

  LOG_DBG("WEBACT", "Free heap at onEnter: %d bytes", ESP.getFreeHeap());

  // Reset state
  state = WebServerActivityState::MODE_SELECTION;
  networkMode = NetworkMode::JOIN_NETWORK;
  isApMode = false;
  connectedIP.clear();
  connectedSSID.clear();
  lastHandleClientTime = 0;
  requestUpdate();

  // Launch network mode selection subactivity
  LOG_DBG("WEBACT", "Launching NetworkModeSelectionActivity...");
  startActivityForResult(std::make_unique<NetworkModeSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             onGoHome();
                           } else {
                             onNetworkModeSelected(std::get<NetworkModeResult>(result.data).mode);
                           }
                         });
}

void MyneWebServerActivity::onExit() {
  Activity::onExit();

  LOG_DBG("WEBACT", "Free heap at onExit start: %d bytes", ESP.getFreeHeap());

  state = WebServerActivityState::SHUTTING_DOWN;

  // Stop the web server first (before disconnecting WiFi)
  stopWebServer();

  // Stop mDNS
  MDNS.end();

  // Stop DNS server if running (AP mode)
  if (dnsServer) {
    LOG_DBG("WEBACT", "Stopping DNS server...");
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;
  }

  // Brief wait for LWIP stack to flush pending packets
  delay(50);

  // Disconnect WiFi gracefully
  if (isApMode) {
    LOG_DBG("WEBACT", "Stopping WiFi AP...");
    WiFi.softAPdisconnect(true);
  } else {
    LOG_DBG("WEBACT", "Disconnecting WiFi (graceful)...");
    WiFi.disconnect(false);  // false = don't erase credentials, send disconnect frame
  }
  delay(30);  // Allow disconnect frame to be sent

  LOG_DBG("WEBACT", "Setting WiFi mode OFF...");
  WiFi.mode(WIFI_OFF);
  delay(30);  // Allow WiFi hardware to power down

  LOG_DBG("WEBACT", "Free heap at onExit end: %d bytes", ESP.getFreeHeap());
}

void MyneWebServerActivity::onNetworkModeSelected(const NetworkMode mode) {
  const char* modeName = "Join Network";
  if (mode == NetworkMode::CREATE_HOTSPOT) {
    modeName = "Create Hotspot";
  }
  LOG_DBG("WEBACT", "Network mode selected: %s", modeName);

  networkMode = mode;
  isApMode = (mode == NetworkMode::CREATE_HOTSPOT);

  if (mode == NetworkMode::JOIN_NETWORK) {
    // STA mode - launch WiFi selection
    LOG_DBG("WEBACT", "Turning on WiFi (STA mode)...");
    WiFi.mode(WIFI_STA);

    state = WebServerActivityState::WIFI_SELECTION;
    LOG_DBG("WEBACT", "Launching WifiSelectionActivity...");
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                           [this](const ActivityResult& result) {
                             if (!result.isCancelled) {
                               const auto& wifi = std::get<WifiResult>(result.data);
                               connectedIP = wifi.ip;
                               connectedSSID = wifi.ssid;
                             }
                             onWifiSelectionComplete(!result.isCancelled);
                           });
  } else {
    // AP mode - start access point
    state = WebServerActivityState::AP_STARTING;
    requestUpdate();
    yield();
    startAccessPoint();
  }
}

void MyneWebServerActivity::onWifiSelectionComplete(const bool connected) {
  LOG_DBG("WEBACT", "WifiSelectionActivity completed, connected=%d", connected);

  if (connected) {
    isApMode = false;
    state = WebServerActivityState::AP_STARTING;
    requestUpdate();
    yield();

    // Start mDNS for hostname resolution
    if (MDNS.begin(AP_HOSTNAME)) {
      LOG_DBG("WEBACT", "mDNS started: http://%s.local/", AP_HOSTNAME);
    }

    // Start the web server
    startWebServer();
  } else {
    // User cancelled - go back to mode selection
    state = WebServerActivityState::MODE_SELECTION;

    startActivityForResult(std::make_unique<NetworkModeSelectionActivity>(renderer, mappedInput),
                           [this](const ActivityResult& result) {
                             if (result.isCancelled) {
                               onGoHome();
                             } else {
                               onNetworkModeSelected(std::get<NetworkModeResult>(result.data).mode);
                             }
                           });
  }
}

void MyneWebServerActivity::startAccessPoint() {
  LOG_DBG("WEBACT", "Starting Access Point mode...");
  LOG_DBG("WEBACT", "Free heap before AP start: %d bytes", ESP.getFreeHeap());

  // Configure and start the AP
  esp_task_wdt_reset();
  WiFi.mode(WIFI_AP);
  delay(100);

  // Start soft AP
  bool apStarted;
  if (AP_PASSWORD && strlen(AP_PASSWORD) >= 8) {
    apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
  } else {
    // Open network (no password)
    apStarted = WiFi.softAP(AP_SSID, nullptr, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
  }

  if (!apStarted) {
    LOG_ERR("WEBACT", "ERROR: Failed to start Access Point!");
    onGoHome();
    return;
  }

  esp_task_wdt_reset();
  delay(100);  // Wait for AP to fully initialize

  // Get AP IP address
  const IPAddress apIP = WiFi.softAPIP();
  char ipStr[16];
  snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
  connectedIP = ipStr;
  connectedSSID = AP_SSID;

  LOG_DBG("WEBACT", "Access Point started!");
  LOG_DBG("WEBACT", "SSID: %s", AP_SSID);
  LOG_DBG("WEBACT", "IP: %s", connectedIP.c_str());

  // Start mDNS for hostname resolution
  if (MDNS.begin(AP_HOSTNAME)) {
    LOG_DBG("WEBACT", "mDNS started: http://%s.local/", AP_HOSTNAME);
  } else {
    LOG_DBG("WEBACT", "WARNING: mDNS failed to start");
  }

  // Start DNS server for captive portal behavior
  // This redirects all DNS queries to our IP, making any domain typed resolve to us
  dnsServer = new DNSServer();
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(DNS_PORT, "*", apIP);
  LOG_DBG("WEBACT", "DNS server started for captive portal");

  LOG_DBG("WEBACT", "Free heap after AP start: %d bytes", ESP.getFreeHeap());

  // Start the web server
  startWebServer();
}

void MyneWebServerActivity::startWebServer() {
  LOG_DBG("WEBACT", "Starting web server...");

  esp_task_wdt_reset();
  // Create the web server instance
  webServer.reset(new MyneWebServer());
  webServer->setFirmwareFlashNotify(firmwareFlashCallback, this);
  webServer->begin();
  esp_task_wdt_reset();

  if (webServer->isRunning()) {
    state = WebServerActivityState::SERVER_RUNNING;
    LOG_DBG("WEBACT", "Web server started successfully");
    lastWifiBars = isApMode ? 0 : barsForRssi(WiFi.RSSI(), 0);

    // Force an immediate render since we're transitioning from a subactivity
    // that had its own rendering task. We need to make sure our display is shown.
    requestUpdate();
  } else {
    LOG_ERR("WEBACT", "ERROR: Failed to start web server!");
    webServer.reset();
    // Go back on error
    onGoHome();
  }
}

void MyneWebServerActivity::stopWebServer() {
  if (webServer && webServer->isRunning()) {
    LOG_DBG("WEBACT", "Stopping web server...");
    webServer->stop();
    LOG_DBG("WEBACT", "Web server stopped");
  }
  webServer.reset();
}

void MyneWebServerActivity::loop() {
  // Handle different states
  if (state == WebServerActivityState::SERVER_RUNNING) {
    // Prefer UI responsiveness over raw HTTP throughput. This is checked before
    // any network work so the user can always leave the server screen.
    if (mappedInput.wasReleasedGroup(MappedInputManager::ButtonGroup::BottomLeft)) {
      goHomeOrSync(renderer, mappedInput);
      return;
    }

    // Handle DNS requests for captive portal (AP mode only)
    if (isApMode && dnsServer) {
      dnsServer->processNextRequest();
    }

    // STA mode: Monitor WiFi connection health
    if (!isApMode && webServer && webServer->isRunning()) {
      static unsigned long lastWifiCheck = 0;
      if (millis() - lastWifiCheck > 2000) {  // Check every 2 seconds
        lastWifiCheck = millis();
        const wl_status_t wifiStatus = WiFi.status();
        // Driver auto-reconnect handles retries; abandon (via onGoHome) only
        // after WIFI_ABANDON_MS, otherwise the activity freezes on a blip.
        bool repaint = false;
        if (wifiStatus != WL_CONNECTED) {
          if (consecutiveDisconnects == 0) {
            firstDisconnectAt = millis();
            repaint = true;
          }
          consecutiveDisconnects++;
          LOG_DBG("WEBACT", "WiFi not connected (status=%d, consecutive=%d, total=%lu ms)", wifiStatus,
                  consecutiveDisconnects, millis() - firstDisconnectAt);
          if (millis() - firstDisconnectAt > WIFI_ABANDON_MS) {
            LOG_DBG("WEBACT", "WiFi unavailable for >%lu s; returning to network selection", WIFI_ABANDON_MS / 1000UL);
            state = WebServerActivityState::SHUTTING_DOWN;
            goHomeOrSync(renderer, mappedInput);
            return;
          }
        } else {
          if (consecutiveDisconnects > 0) {
            LOG_DBG("WEBACT", "WiFi recovered after %d failed checks (%lu ms)", consecutiveDisconnects,
                    millis() - firstDisconnectAt);
            repaint = true;
          }
          consecutiveDisconnects = 0;
          firstDisconnectAt = 0;
          const int rssi = WiFi.RSSI();
          if (rssi < -75) {
            LOG_DBG("WEBACT", "Warning: Weak WiFi signal: %d dBm", rssi);
          }
          const int bars = barsForRssi(rssi, lastWifiBars);
          if (bars != lastWifiBars) {
            lastWifiBars = bars;
            repaint = true;
          }
        }
        if (repaint) requestUpdate();
      }
    }

    // Handle web server requests - maximize throughput with watchdog safety
    if (webServer && webServer->isRunning()) {
      const unsigned long timeSinceLastHandleClient = millis() - lastHandleClientTime;

      // Log if there's a significant gap between handleClient calls (>100ms)
      if (lastHandleClientTime > 0 && timeSinceLastHandleClient > 100) {
        LOG_DBG("WEBACT", "WARNING: %lu ms gap since last handleClient", timeSinceLastHandleClient);
      }

      // Reset watchdog BEFORE processing - HTTP header parsing can be slow
      esp_task_wdt_reset();

      // Process HTTP requests in a short, bounded burst. A large fixed loop can
      // monopolize the activity when a browser loads the dashboard and makes
      // the Back button feel frozen.
      constexpr int MAX_ITERATIONS = 32;
      constexpr unsigned long MAX_BURST_MS = 12;
      const unsigned long burstStart = millis();
      for (int i = 0; i < MAX_ITERATIONS && webServer->isRunning(); i++) {
        webServer->handleClient();
        if ((i & 0x03) == 0x03) {
          esp_task_wdt_reset();
          yield();
          mappedInput.update();
          if (mappedInput.wasReleasedGroup(MappedInputManager::ButtonGroup::BottomLeft)) {
            goHomeOrSync(renderer, mappedInput);
            return;
          }
        }
        if (millis() - burstStart >= MAX_BURST_MS) {
          break;
        }
      }
      lastHandleClientTime = millis();
      yield();
    }
  }
}

// ── Web firmware flash ────────────────────────────────────────────────────────

void MyneWebServerActivity::firmwareFlashCallback(const MyneWebServer::FirmwareFlashEvent& evt, void* ctx) {
  auto* self = static_cast<MyneWebServerActivity*>(ctx);
  using Phase = MyneWebServer::FirmwareFlashEvent::Phase;

  // Throttle FLASHING redraws to once per 1% to spare the e-ink panel
  if (evt.phase == Phase::FLASHING) {
    const int pct = evt.total > 0 ? static_cast<int>((evt.written * 100) / evt.total) : 0;
    if (pct == self->lastFlashPercent) return;
    self->lastFlashPercent = pct;
  }

  self->lastFlashEvent = evt;
  self->state = WebServerActivityState::WEB_FIRMWARE_FLASH;
  self->renderWebFirmwareFlash();
}

void MyneWebServerActivity::renderWebFirmwareFlash() const {
  using Phase = MyneWebServer::FirmwareFlashEvent::Phase;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int W = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, W, metrics.headerHeight});
  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  NetworkActivityUI::hero(renderer, Rect{NetworkActivityUI::PAD, heroY, W - NetworkActivityUI::PAD * 2, 104},
                          tr(STR_NETWORK), tr(STR_WEB_FIRMWARE_UPDATE));
  const int cardY = heroY + 136;
  const int cardW = W - NetworkActivityUI::PAD * 2;

  switch (lastFlashEvent.phase) {
    case Phase::VALIDATING:
      NetworkActivityUI::stateCard(renderer, Rect{NetworkActivityUI::PAD, cardY, cardW, 180},
                                   tr(STR_VALIDATING_FIRMWARE));
      break;

    case Phase::FLASHING: {
      const int pct =
          lastFlashEvent.total > 0 ? static_cast<int>((lastFlashEvent.written * 100) / lastFlashEvent.total) : 0;
      NetworkActivityUI::panel(renderer, Rect{NetworkActivityUI::PAD, cardY, cardW, 210}, true);
      renderer.drawCenteredText(UI_10_FONT_ID, cardY + 40, tr(STR_UPDATING), true, EpdFontFamily::BOLD);
      GUI.drawProgressBar(renderer,
                          Rect{NetworkActivityUI::PAD + NetworkActivityUI::INNER, cardY + 92,
                               cardW - NetworkActivityUI::INNER * 2, metrics.progressBarHeight},
                          pct, 100);
      renderer.drawCenteredText(SMALL_FONT_ID, cardY + 154, tr(STR_FIRMWARE_UPDATE_DO_NOT_POWER_OFF), true,
                                EpdFontFamily::BOLD);
      break;
    }

    case Phase::DONE:
      NetworkActivityUI::stateCard(renderer, Rect{NetworkActivityUI::PAD, cardY, cardW, 180}, tr(STR_UPDATE_COMPLETE),
                                   tr(STR_RESTARTING_HINT));
      break;

    case Phase::FAILED:
      NetworkActivityUI::stateCard(renderer, Rect{NetworkActivityUI::PAD, cardY, cardW, 180}, tr(STR_UPDATE_FAILED),
                                   lastFlashEvent.error);
      break;
  }

  renderer.displayBuffer();
}

// ── render ────────────────────────────────────────────────────────────────────

void MyneWebServerActivity::render(RenderLock&&) {
  if (state == WebServerActivityState::SERVER_RUNNING || state == WebServerActivityState::AP_STARTING) {
    renderer.clearScreen();
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int pageWidth = renderer.getScreenWidth();

    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight});
    const int heroY = metrics.topPadding + metrics.headerHeight + 8;
    const bool showWifiIndicator = !isApMode && state == WebServerActivityState::SERVER_RUNNING;
    NetworkActivityUI::hero(renderer, Rect{NetworkActivityUI::PAD, heroY, pageWidth - NetworkActivityUI::PAD * 2, 104},
                            isApMode ? tr(STR_HOTSPOT_MODE) : tr(STR_NETWORK),
                            state == WebServerActivityState::SERVER_RUNNING
                                ? tr(STR_OPEN_URL_HINT)
                                : (isApMode ? tr(STR_STARTING_HOTSPOT) : tr(STR_STARTING_SERVER)),
                            connectedSSID.c_str(), showWifiIndicator ? WIFI_ICON_RIGHT_RESERVE : 0);

    if (state == WebServerActivityState::SERVER_RUNNING) {
      renderServerRunning();
    } else {
      NetworkActivityUI::stateCard(
          renderer, Rect{NetworkActivityUI::PAD, heroY + 140, pageWidth - NetworkActivityUI::PAD * 2, 180},
          isApMode ? tr(STR_STARTING_HOTSPOT) : tr(STR_STARTING_SERVER));
    }
    renderer.displayBuffer();
  }
}

void MyneWebServerActivity::renderServerRunning() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int heroY = metrics.topPadding + metrics.headerHeight + 8;
  const int contentY = heroY + 124;
  constexpr int qrSize = 166;
  const int contentW = pageWidth - NetworkActivityUI::PAD * 2;
  const Rect heroRect{NetworkActivityUI::PAD, heroY, contentW, 104};

  if (isApMode) {
    const std::string wifiConfig = std::string("WIFI:S:") + connectedSSID + ";;";
    std::string hostnameUrl = std::string("http://") + AP_HOSTNAME + ".local/";
    std::string ipUrl = tr(STR_OR_HTTP_PREFIX) + connectedIP + "/";

    const int cardH = qrSize + 52;
    NetworkActivityUI::panel(renderer, Rect{NetworkActivityUI::PAD, contentY, contentW, cardH});
    NetworkActivityUI::text(renderer, SMALL_FONT_ID, NetworkActivityUI::PAD + NetworkActivityUI::INNER, contentY + 16,
                            tr(STR_CONNECT_WIFI_HINT), contentW - NetworkActivityUI::INNER * 2, EpdFontFamily::BOLD);
    QrUtils::drawQrCode(
        renderer, Rect{NetworkActivityUI::PAD + NetworkActivityUI::INNER, contentY + 42, qrSize, qrSize}, wifiConfig);
    NetworkActivityUI::metric(
        renderer,
        Rect{NetworkActivityUI::PAD + NetworkActivityUI::INNER + qrSize + NetworkActivityUI::GAP, contentY + 58,
             contentW - qrSize - NetworkActivityUI::INNER * 2 - NetworkActivityUI::GAP, 96},
        "SSID", connectedSSID.c_str());

    const int urlY = contentY + cardH + NetworkActivityUI::GAP;
    NetworkActivityUI::panel(renderer, Rect{NetworkActivityUI::PAD, urlY, contentW, 132});
    NetworkActivityUI::text(renderer, SMALL_FONT_ID, NetworkActivityUI::PAD + NetworkActivityUI::INNER, urlY + 16,
                            tr(STR_OPEN_URL_HINT), contentW - NetworkActivityUI::INNER * 2, EpdFontFamily::BOLD);
    NetworkActivityUI::text(renderer, UI_10_FONT_ID, NetworkActivityUI::PAD + NetworkActivityUI::INNER, urlY + 48,
                            hostnameUrl.c_str(), contentW - NetworkActivityUI::INNER * 2, EpdFontFamily::BOLD);
    NetworkActivityUI::text(renderer, SMALL_FONT_ID, NetworkActivityUI::PAD + NetworkActivityUI::INNER, urlY + 86,
                            ipUrl.c_str(), contentW - NetworkActivityUI::INNER * 2);
  } else {
    std::string webInfo = "http://" + connectedIP + "/";
    std::string hostnameUrl = std::string(tr(STR_OR_HTTP_PREFIX)) + AP_HOSTNAME + ".local/";

    NetworkActivityUI::panel(renderer, Rect{NetworkActivityUI::PAD, contentY, contentW, 330}, true);
    NetworkActivityUI::text(renderer, SMALL_FONT_ID, NetworkActivityUI::PAD + NetworkActivityUI::INNER, contentY + 18,
                            tr(STR_SCAN_QR_HINT), contentW - NetworkActivityUI::INNER * 2, EpdFontFamily::BOLD);
    const int qrX = (pageWidth - qrSize) / 2;
    QrUtils::drawQrCode(renderer, Rect{qrX, contentY + 58, qrSize, qrSize}, webInfo);
    renderer.drawCenteredText(UI_10_FONT_ID, contentY + 236, webInfo.c_str(), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(SMALL_FONT_ID, contentY + 270, hostnameUrl.c_str(), true);

    const int statY = contentY + 346;
    const int statW = (contentW - NetworkActivityUI::GAP) / 2;
    NetworkActivityUI::metric(renderer, Rect{NetworkActivityUI::PAD, statY, statW, 92}, "SSID", connectedSSID.c_str());
    NetworkActivityUI::metric(renderer, Rect{NetworkActivityUI::PAD + statW + NetworkActivityUI::GAP, statY, statW, 92},
                              tr(STR_IP_ADDRESS_PREFIX), connectedIP.c_str(), true);
    renderWifiIndicator(heroRect);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_EXIT), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void MyneWebServerActivity::renderWifiIndicator(Rect heroRect) const {
  const int iconRight = heroRect.x + heroRect.width - NetworkActivityUI::INNER;
  const int iconLeft = iconRight - WIFI_ICON_WIDTH;
  const int iconTop = heroRect.y + 14;
  const int iconBottom = iconTop + WIFI_ICON_HEIGHT;

  const bool wifiUp = (WiFi.status() == WL_CONNECTED) && (consecutiveDisconnects == 0);
  if (wifiUp) {
    for (int i = 0; i < WIFI_ICON_BAR_COUNT; i++) {
      const int barHeight = (i + 1) * WIFI_ICON_HEIGHT / WIFI_ICON_BAR_COUNT;
      const int x = iconLeft + i * (WIFI_ICON_BAR_WIDTH + WIFI_ICON_BAR_GAP);
      const int y = iconBottom - barHeight;
      if (i < lastWifiBars) {
        renderer.fillRect(x, y, WIFI_ICON_BAR_WIDTH, barHeight, true);
      } else {
        renderer.drawRect(x, y, WIFI_ICON_BAR_WIDTH, barHeight, true);
      }
    }
  } else {
    const int x0 = iconRight - WIFI_ICON_HEIGHT;
    renderer.drawLine(x0, iconTop, x0 + WIFI_ICON_HEIGHT, iconBottom, 2, true);
    renderer.drawLine(x0, iconBottom, x0 + WIFI_ICON_HEIGHT, iconTop, 2, true);
  }
}
