#include "ReadingEditActivity.h"

#include <BookStore.h>
#include <GfxRenderer.h>
#include <I18n.h>
#ifndef SIMULATOR
#include <WiFi.h>

#include "activities/network/WifiSelectionActivity.h"
#endif

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "BooksActivityUI.h"
#include "MappedInputManager.h"
#include "MyneSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

static constexpr int STATUS_COUNT = 5;

static void renderStatsProgress(void* raw, int done, int total) {
  if (total > 0 && done > 0 && done < total) {
    if ((done * 10 / total) == ((done - 1) * 10 / total)) return;
  }
  auto& r = *static_cast<GfxRenderer*>(raw);
  const int W = r.getScreenWidth();
  const int H = r.getScreenHeight();

  r.clearScreen();
  r.drawCenteredText(UI_10_FONT_ID, H / 2 - 48, tr(STR_UPDATING));

  constexpr int BAR_W = 320, BAR_H = 14, BAR_R = 7;
  const int bx = (W - BAR_W) / 2;
  const int by = H / 2 - BAR_H / 2;
  r.drawRoundedRect(bx, by, BAR_W, BAR_H, 1, BAR_R, true);
  if (total > 0 && done > 0) {
    const int fill = (BAR_W - 2) * std::min(done, total) / total;
    if (fill > 0) r.fillRect(bx + 1, by + 1, fill, BAR_H - 2, Color::Black);
  }

  char cnt[16];
  snprintf(cnt, sizeof(cnt), "%d / %d", done, total);
  const int cw = r.getTextWidth(SMALL_FONT_ID, cnt);
  r.drawText(SMALL_FONT_ID, (W - cw) / 2, H / 2 + 24, cnt, true);
  r.displayBuffer();
}

static int daysInMonth(int year, int month) {
  static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  return (month == 2 && leap) ? 29 : days[month - 1];
}

static const char* statusLabel(ReadingStatus s) {
  switch (s) {
    case ReadingStatus::WantToRead:
      return tr(STR_STATUS_WANT_TO_READ);
    case ReadingStatus::Reading:
      return tr(STR_STATUS_READING);
    case ReadingStatus::Paused:
      return tr(STR_STATUS_PAUSED);
    case ReadingStatus::Finished:
      return tr(STR_STATUS_FINISHED);
    case ReadingStatus::Dropped:
      return tr(STR_STATUS_DROPPED);
  }
  return "";
}

// ── Date helpers ──────────────────────────────────────────────────────────────

void ReadingEditActivity::initDate() {
  const time_t now = time(nullptr);
  if (now > 1000000000L) {
    const time_t localNow = now + static_cast<time_t>(SETTINGS.getTimezoneOffsetHours()) * 3600;
    struct tm t;
    gmtime_r(&localNow, &t);
    logYear = t.tm_year + 1900;
    logMonth = t.tm_mon + 1;
    logDay = t.tm_mday;
    logHour = t.tm_hour;
    logMinute = t.tm_min;
    hasTime = true;
    return;
  }
  if (!reading.sessions.empty()) {
    const auto& s = reading.sessions.back();
    if (s.date.size() == 10) {
      sscanf(s.date.c_str(), "%d-%d-%d", &logYear, &logMonth, &logDay);
    }
    if (!s.time.empty()) {
      sscanf(s.time.c_str(), "%d:%d", &logHour, &logMinute);
      hasTime = true;
    }
    return;
  }
  logYear = 2025;
  logMonth = 1;
  logDay = 1;
}

void ReadingEditActivity::adjustDate(int dir) {
  if (dateSubField == 0) {
    logYear += dir;
    if (logYear < 1900) logYear = 1900;
    if (logYear > 2100) logYear = 2100;
    const int max = daysInMonth(logYear, logMonth);
    if (logDay > max) logDay = max;
  } else if (dateSubField == 1) {
    logMonth += dir;
    if (logMonth < 1) {
      logMonth = 12;
      --logYear;
    }
    if (logMonth > 12) {
      logMonth = 1;
      ++logYear;
    }
    const int max = daysInMonth(logYear, logMonth);
    if (logDay > max) logDay = max;
  } else if (dateSubField == 2) {
    struct tm t = {};
    t.tm_year = logYear - 1900;
    t.tm_mon = logMonth - 1;
    t.tm_mday = logDay + dir;
    mktime(&t);
    logYear = t.tm_year + 1900;
    logMonth = t.tm_mon + 1;
    logDay = t.tm_mday;
  } else if (dateSubField == 3) {
    logHour = (logHour + dir + 24) % 24;
    hasTime = true;
  } else {
    logMinute = (logMinute + dir + 60) % 60;
    hasTime = true;
  }
}

void ReadingEditActivity::dateToBuffer(char* buf, size_t n) const {
  snprintf(buf, n, "%04d-%02d-%02d", logYear, logMonth, logDay);
}

// ── Field actions ─────────────────────────────────────────────────────────────

void ReadingEditActivity::adjustField(int dir) {
  switch (selectedField) {
    case Field::Status: {
      int s = static_cast<int>(reading.status) + dir;
      if (s < 0) s = STATUS_COUNT - 1;
      if (s >= STATUS_COUNT) s = 0;
      reading.status = static_cast<ReadingStatus>(s);
      dirty = dirty || (reading.status != originalStatus);
      break;
    }
    case Field::Date:
      adjustDate(dir);
      break;
    case Field::Position: {
      const int next = logPosition + dir;
      logPosition = next < 0 ? 0 : next;
      break;
    }
    case Field::SyncTime:
      break;
    case Field::Type:
      reading.readingType = (reading.readingType == ReadingType::Page) ? ReadingType::Chapter : ReadingType::Page;
      dirty = true;
      break;
  }
  requestUpdate();
}

void ReadingEditActivity::logSession() {
  char dateBuf[12];
  dateToBuffer(dateBuf, sizeof(dateBuf));

  char timeBuf[6] = {};
  if (hasTime) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", logHour, logMinute);

  ReadingSession session;
  session.date = dateBuf;
  session.time = timeBuf;
  session.position = logPosition;
  reading.sessions.push_back(std::move(session));
  if (reading.sessions.size() > ReadingLog::MAX_SESSIONS) reading.sessions.erase(reading.sessions.begin());

  dirty = true;
  requestUpdate();
}

// ── WiFi time sync ────────────────────────────────────────────────────────────

void ReadingEditActivity::startWifiSync() {
  syncState = SyncState::Syncing;
  requestUpdate();
#ifndef SIMULATOR
  LOG_DBG("REDIT", "Starting WiFi sync for NTP");
  wifiTurnedOn = true;
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& r) { onWifiConnected(!r.isCancelled); });
#else
  onWifiConnected(true);
#endif
}

void ReadingEditActivity::onWifiConnected(bool success) {
  if (!success) {
    LOG_DBG("REDIT", "WiFi cancelled or failed");
    syncState = SyncState::Failed;
    wifiTurnedOn = false;
#ifndef SIMULATOR
    WiFi.mode(WIFI_OFF);
#endif
    requestUpdate();
    return;
  }

  struct tm timeinfo = {};
  int retries = 0;

#ifndef SIMULATOR
  LOG_DBG("REDIT", "WiFi connected, syncing NTP (tz offset: %d h)", SETTINGS.getTimezoneOffsetHours());
  configTime(static_cast<long>(SETTINGS.getTimezoneOffsetHours()) * 3600L, 0, "pool.ntp.org", "time.google.com");
  while (!getLocalTime(&timeinfo) && retries < 20) {
    delay(500);
    ++retries;
  }
#else
  LOG_DBG("REDIT", "Simulator: using host system time");
  time_t now = time(nullptr);
  timeinfo = *localtime(&now);
#endif

  if (timeinfo.tm_year > 0) {
    logYear = timeinfo.tm_year + 1900;
    logMonth = timeinfo.tm_mon + 1;
    logDay = timeinfo.tm_mday;
    logHour = timeinfo.tm_hour;
    logMinute = timeinfo.tm_min;
    hasTime = true;
    syncState = SyncState::Success;
    LOG_INF("REDIT", "Time sync ok: %04d-%02d-%02d %02d:%02d", logYear, logMonth, logDay, logHour, logMinute);
  } else {
    LOG_ERR("REDIT", "Time sync failed after %d retries", retries);
    syncState = SyncState::Failed;
  }

#ifndef SIMULATOR
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
#endif
  wifiTurnedOn = false;
  requestUpdate();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void ReadingEditActivity::onEnter() {
  Activity::onEnter();
  selectedField = Field::Status;
  originalStatus = reading.status;
  logPosition = reading.lastPosition();
  dateSubField = 2;  // default to day segment
  dirty = false;
  wifiTurnedOn = false;
  syncState = SyncState::None;
  hasTime = false;
  logHour = 0;
  logMinute = 0;
  initDate();
  requestUpdate();
}

void ReadingEditActivity::onExit() {
#ifndef SIMULATOR
  if (wifiTurnedOn) {
    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
    wifiTurnedOn = false;
  }
#endif
  if (dirty) {
    auto readings = readingLog.loadForBook(book.id);
    bool found = false;
    for (auto& r : readings) {
      if (r.id == reading.id) {
        r = reading;
        found = true;
        break;
      }
    }
    if (!found) readings.push_back(reading);
    readingLog.saveForBook(book.id, readings);
    renderStatsProgress(&renderer, 0, 0);
    readingLog.rebuildStats(BookStore::DIR_PATH, renderStatsProgress, &renderer);
    dirty = false;
  }
  Activity::onExit();
}

// ── Input ─────────────────────────────────────────────────────────────────────

void ReadingEditActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.popActivity();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    logSession();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Power) && selectedField == Field::Date) {
    dateSubField = (dateSubField + 1) % 5;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (selectedField != Field::Status && selectedField != Field::SyncTime && selectedField != Field::Type)
      adjustField(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (selectedField == Field::SyncTime) {
      startWifiSync();
    } else {
      adjustField(+1);
    }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selectedField = static_cast<Field>((static_cast<int>(selectedField) + FIELD_COUNT - 1) % FIELD_COUNT);
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selectedField = static_cast<Field>((static_cast<int>(selectedField) + 1) % FIELD_COUNT);
    requestUpdate();
    return;
  }
}

// ── Render ────────────────────────────────────────────────────────────────────

void ReadingEditActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const auto& m = UITheme::getInstance().getMetrics();
  const int pad = 20;
  const int CW = W - 2 * pad;

  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);

  static constexpr int CARD_R = 8;
  static constexpr int CARD_IP = 16;
  static constexpr int TILE_H = 74;
  static constexpr int TILE_G = 10;

  char titleBuf[80];
  if (book.volume.empty()) {
    snprintf(titleBuf, sizeof(titleBuf), "%s", book.title.c_str());
  } else {
    snprintf(titleBuf, sizeof(titleBuf), "%s (%s)", book.title.c_str(), book.volume.c_str());
  }
  GUI.drawHeader(renderer, Rect{0, m.topPadding, W, m.headerHeight});

  auto drawCard = [&](Rect r, bool gray = false) {
    if (gray) renderer.fillRoundedRect(r.x, r.y, r.width, r.height, CARD_R, Color::LightGray);
    renderer.drawRoundedRect(r.x, r.y, r.width, r.height, 1, CARD_R, true);
  };

  auto compactDate = [&]() {
    char dateBuf[28] = {};
    static constexpr StrId MONTH_STRS[] = {
        StrId::STR_MONTH_SHORT_JAN, StrId::STR_MONTH_SHORT_FEB, StrId::STR_MONTH_SHORT_MAR, StrId::STR_MONTH_SHORT_APR,
        StrId::STR_MONTH_SHORT_MAY, StrId::STR_MONTH_SHORT_JUN, StrId::STR_MONTH_SHORT_JUL, StrId::STR_MONTH_SHORT_AUG,
        StrId::STR_MONTH_SHORT_SEP, StrId::STR_MONTH_SHORT_OCT, StrId::STR_MONTH_SHORT_NOV, StrId::STR_MONTH_SHORT_DEC,
    };
    const int mo = logMonth - 1;
    if (mo >= 0 && mo < 12) {
      const char* monthStr = I18N.get(MONTH_STRS[mo]);
      if (hasTime) {
        snprintf(dateBuf, sizeof(dateBuf), "%d %s '%02d \xc2\xb7 %02d:%02d", logDay, monthStr, logYear % 100, logHour,
                 logMinute);
      } else {
        snprintf(dateBuf, sizeof(dateBuf), "%d %s '%02d", logDay, monthStr, logYear % 100);
      }
    } else {
      dateToBuffer(dateBuf, sizeof(dateBuf));
    }
    return std::string(dateBuf);
  };

  auto drawCountChip = [&](Rect r, const char* text, int y) {
    const int tw = renderer.getTextWidth(SMALL_FONT_ID, text, EpdFontFamily::BOLD);
    renderer.fillRoundedRect(r.x + r.width - CARD_IP - tw - 18, y - 5, tw + 18, lhSm + 10, 5, Color::Black);
    renderer.drawText(SMALL_FONT_ID, r.x + r.width - CARD_IP - tw - 9, y, text, false, EpdFontFamily::BOLD);
  };

  auto drawFieldTile = [&](Rect r, const char* label, const char* val, bool selected) {
    if (selected) renderer.fillRoundedRect(r.x, r.y, r.width, r.height, CARD_R, Color::LightGray);
    renderer.drawRoundedRect(r.x, r.y, r.width, r.height, selected ? 2 : 1, CARD_R, true);
    if (selected) {
      renderer.fillRoundedRect(r.x, r.y, 8, r.height, CARD_R, true, false, true, false, Color::Black);
    }
    const auto safeLabel = renderer.truncatedText(SMALL_FONT_ID, label, r.width - CARD_IP * 2, EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, r.x + CARD_IP, r.y + 13, safeLabel.c_str(), true, EpdFontFamily::BOLD);
    const auto safe = renderer.truncatedText(UI_10_FONT_ID, val, r.width - CARD_IP * 2, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, r.x + CARD_IP, r.y + 13 + lhSm + 8, safe.c_str(), true, EpdFontFamily::BOLD);
  };

  const char* typeStr = (reading.readingType == ReadingType::Chapter) ? tr(STR_READ_BY_CHAPTER) : tr(STR_READ_BY_PAGE);

  int y = m.topPadding + m.headerHeight + 8;

  // Book context
  {
    BooksActivityUI::hero(renderer, Rect{pad, y, CW, BooksActivityUI::HERO_H}, tr(STR_READINGS), titleBuf, typeStr);
    y += BooksActivityUI::HERO_H + 12;
  }

  // Editable fields
  {
    const int tileW = (CW - TILE_G) / 2;
    const int row1 = y;
    const int row2 = y + TILE_H + TILE_G;

    drawFieldTile(Rect{pad, row1, tileW, TILE_H}, tr(STR_READ_STATUS), statusLabel(reading.status),
                  selectedField == Field::Status);
    drawFieldTile(Rect{pad + tileW + TILE_G, row1, tileW, TILE_H}, tr(STR_LOG_DATE), compactDate().c_str(),
                  selectedField == Field::Date);
    if (selectedField == Field::Date) {
      char seg[5][6];
      snprintf(seg[0], sizeof(seg[0]), "%04d", logYear);
      snprintf(seg[1], sizeof(seg[1]), "%02d", logMonth);
      snprintf(seg[2], sizeof(seg[2]), "%02d", logDay);
      snprintf(seg[3], sizeof(seg[3]), "%02d", logHour);
      snprintf(seg[4], sizeof(seg[4]), "%02d", logMinute);
      static constexpr StrId DATE_FIELD_LABELS[] = {
          StrId::STR_DATE_FIELD_YEAR, StrId::STR_DATE_FIELD_MONTH,  StrId::STR_DATE_FIELD_DAY,
          StrId::STR_DATE_FIELD_HOUR, StrId::STR_DATE_FIELD_MINUTE,
      };
      char chipBuf[24];
      snprintf(chipBuf, sizeof(chipBuf), "%s %s", trId(DATE_FIELD_LABELS[dateSubField]), seg[dateSubField]);
      drawCountChip(Rect{pad + tileW + TILE_G, row1, tileW, TILE_H}, chipBuf, row1 + 13);
    }

    const char* unit = (reading.readingType == ReadingType::Chapter) ? "ch." : "p.";
    char posBuf[16];
    snprintf(posBuf, sizeof(posBuf), "%s %d", unit, logPosition);
    drawFieldTile(Rect{pad, row2, tileW, TILE_H}, tr(STR_LOG_POSITION), posBuf, selectedField == Field::Position);

    const char* syncVal = "WiFi";
    if (syncState == SyncState::Syncing)
      syncVal = tr(STR_FETCHING_TIME);
    else if (syncState == SyncState::Success)
      syncVal = tr(STR_SYNC_TIME_OK);
    else if (syncState == SyncState::Failed)
      syncVal = tr(STR_SYNC_TIME_FAILED);
    drawFieldTile(Rect{pad + tileW + TILE_G, row2, tileW, TILE_H}, tr(STR_SYNC_TIME_WIFI), syncVal,
                  selectedField == Field::SyncTime);

    const int row3 = y + (TILE_H + TILE_G) * 2;
    drawFieldTile(Rect{pad, row3, CW, TILE_H}, tr(STR_READ_TRACKING), typeStr, selectedField == Field::Type);

    y += TILE_H * 3 + TILE_G * 2 + 14;
  }

  // Sessions
  {
    const int bx = pad, bw = W - 2 * pad;
    const int bh = H - m.buttonHintsHeight - y - 6;

    if (bh >= 90) {
      renderer.drawRoundedRect(bx, y, bw, bh, 1, CARD_R, true);

      renderer.drawText(SMALL_FONT_ID, bx + CARD_IP, y + 16, tr(STR_LOG_SESSIONS), true, EpdFontFamily::BOLD);

      const int sessionCount = static_cast<int>(reading.sessions.size());
      char cntBuf[8];
      snprintf(cntBuf, sizeof(cntBuf), "%d", sessionCount);
      drawCountChip(Rect{bx, y, bw, bh}, cntBuf, y + 16);

      const int typeY = y + 16 + lhSm + 8;
      renderer.drawText(SMALL_FONT_ID, bx + CARD_IP, typeY, typeStr, true);
      renderer.drawLine(bx + CARD_IP, typeY + lhSm + 8, bx + bw - CARD_IP, typeY + lhSm + 8);

      const int listTop = typeY + lhSm + 18;
      const int listH = bh - (listTop - y);
      const int sessRowH = 48;
      const int maxShow = listH / sessRowH;

      if (sessionCount == 0) {
        const int iconX = bx + bw / 2 - 24;
        const int iconY = listTop + std::max(10, (listH - 106) / 2);
        renderer.drawRoundedRect(iconX, iconY, 48, 56, 1, 5, true);
        renderer.drawLine(iconX + 12, iconY + 12, iconX + 36, iconY + 12, 2, true);
        renderer.drawLine(iconX + 12, iconY + 26, iconX + 32, iconY + 26, 2, true);
        renderer.drawCenteredText(UI_10_FONT_ID, iconY + 76, tr(STR_NO_SESSIONS), true, EpdFontFamily::BOLD);
      } else {
        const char* unit = (reading.readingType == ReadingType::Chapter) ? "ch." : "p.";
        const int showCount = sessionCount < maxShow ? sessionCount : maxShow;
        const int startIdx = sessionCount - showCount;

        auto fmtDateTime = [](const std::string& d, const std::string& tm, char* out, size_t n) {
          if (d.size() >= 7) {
            auto shortMonth = [](int m) -> const char* {
              switch (m) {
                case 0:
                  return tr(STR_MONTH_SHORT_JAN);
                case 1:
                  return tr(STR_MONTH_SHORT_FEB);
                case 2:
                  return tr(STR_MONTH_SHORT_MAR);
                case 3:
                  return tr(STR_MONTH_SHORT_APR);
                case 4:
                  return tr(STR_MONTH_SHORT_MAY);
                case 5:
                  return tr(STR_MONTH_SHORT_JUN);
                case 6:
                  return tr(STR_MONTH_SHORT_JUL);
                case 7:
                  return tr(STR_MONTH_SHORT_AUG);
                case 8:
                  return tr(STR_MONTH_SHORT_SEP);
                case 9:
                  return tr(STR_MONTH_SHORT_OCT);
                case 10:
                  return tr(STR_MONTH_SHORT_NOV);
                case 11:
                  return tr(STR_MONTH_SHORT_DEC);
                default:
                  return "";
              }
            };
            const int mo = (d[5] - '0') * 10 + (d[6] - '0') - 1;
            if (mo >= 0 && mo < 12) {
              if (!tm.empty()) {
                snprintf(out, n, "%s '%c%c %s", shortMonth(mo), d[2], d[3], tm.c_str());
              } else {
                snprintf(out, n, "%s '%c%c", shortMonth(mo), d[2], d[3]);
              }
              return;
            }
          }
          snprintf(out, n, "%s", d.empty() ? "\xe2\x80\x94" : d.c_str());
        };

        for (int i = sessionCount - 1; i >= startIdx; --i) {
          const auto& s = reading.sessions[static_cast<size_t>(i)];
          const int rowI = sessionCount - 1 - i;
          const int ry = listTop + rowI * sessRowH;
          const int ty = ry + (sessRowH - lh10) / 2;

          char dateFmt[24];
          fmtDateTime(s.date, s.time, dateFmt, sizeof(dateFmt));
          renderer.drawText(UI_10_FONT_ID, bx + CARD_IP, ty, dateFmt, true, EpdFontFamily::BOLD);

          char valBuf[20];
          snprintf(valBuf, sizeof(valBuf), "%s %d", unit, s.position);
          const int vw = renderer.getTextWidth(UI_10_FONT_ID, valBuf);
          renderer.drawText(UI_10_FONT_ID, bx + bw - CARD_IP - vw, ty, valBuf, true);

          if (rowI + 1 < showCount) renderer.drawLine(bx + 1, ry + sessRowH, bx + bw - 1, ry + sessRowH, true);
        }
      }
    }
  }

  const bool isCountField = selectedField == Field::Date || selectedField == Field::Position;
  const bool isSyncField = selectedField == Field::SyncTime;
  const bool isTypeField = selectedField == Field::Type;
  const auto lbls = mappedInput.mapLabels(tr(STR_BACK), tr(STR_LOG_ACTION), isCountField ? tr(STR_READ_UNIT_MINUS) : "",
                                          isCountField  ? tr(STR_READ_UNIT_PLUS)
                                          : isSyncField ? tr(STR_SYNC)
                                          : isTypeField ? tr(STR_READ_TRACKING)
                                                        : tr(STR_READ_STATUS));
  GUI.drawButtonHints(renderer, lbls.btn1, lbls.btn2, lbls.btn3, lbls.btn4);

  renderer.displayBuffer();
}
