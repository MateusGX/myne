#include "HomeActivity.h"

#include <ArduinoJson.h>
#include <BookStore.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <I18nKeys.h>
#include <ReadingLog.h>

#include <cstring>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

static constexpr int kMenuCount = 5;

namespace {
constexpr int kHomePad = 20;
constexpr int kCardR = 8;

void drawSelectionFrame(const GfxRenderer& renderer, Rect rect, bool selected) {
  if (selected) {
    renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, kCardR, Color::LightGray);
    renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 2, kCardR, true);
    renderer.fillRoundedRect(rect.x, rect.y, 10, rect.height, kCardR, true, false, true, false, Color::Black);
  } else {
    renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, kCardR, true);
  }
}

void drawLastReadTile(GfxRenderer& renderer, Rect rect, bool selected, const char* label, const char* emptyDescription,
                      bool hasBook, const char* title, const char* author, const char* progress, const char* date,
                      const char* status) {
  drawSelectionFrame(renderer, rect, selected);

  constexpr int iconSize = 64;
  const int iconX = rect.x + 24;
  const int iconY = rect.y + (rect.height - iconSize) / 2;
  if (const uint8_t* bmp = iconForName(UIIcon::BookHeartIcon, iconSize)) {
    renderer.drawIcon(bmp, iconX, iconY, iconSize, iconSize);
  }

  const int textX = iconX + iconSize + 22;
  const int textW = rect.width - (textX - rect.x) - 24;
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);
  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.drawText(SMALL_FONT_ID, textX, rect.y + 22, label, true, EpdFontFamily::BOLD);

  if (!hasBook) {
    const auto desc = renderer.truncatedText(UI_10_FONT_ID, emptyDescription, textW);
    renderer.drawText(UI_10_FONT_ID, textX, rect.y + 22 + lhSm + 14, desc.c_str(), true);
    return;
  }

  const auto lines = renderer.wrappedText(UI_10_FONT_ID, title, textW, 2, EpdFontFamily::BOLD);
  int y = rect.y + 22 + lhSm + 12;
  for (const auto& line : lines) {
    renderer.drawText(UI_10_FONT_ID, textX, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += lh10 + 1;
  }

  const int footerY = rect.y + rect.height - lhSm - 18;
  const int maxAuthorY = footerY - lhSm - 14;
  if (author && author[0]) {
    const auto by = renderer.truncatedText(SMALL_FONT_ID, author, textW);
    if (y + 6 <= maxAuthorY) {
      renderer.drawText(SMALL_FONT_ID, textX, y + 6, by.c_str(), true);
    }
  }

  renderer.drawLine(textX, footerY - 12, rect.x + rect.width - 24, footerY - 12);
  int statusW = 0;
  if (status && status[0]) {
    const auto st = renderer.truncatedText(SMALL_FONT_ID, status, textW / 2);
    statusW = renderer.getTextWidth(SMALL_FONT_ID, st.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - 24 - statusW, footerY, st.c_str(), true);
  }

  char meta[48] = {};
  if (progress && progress[0] && date && date[0]) {
    snprintf(meta, sizeof(meta), "%s \xc2\xb7 %s", progress, date);
  } else if (progress && progress[0]) {
    snprintf(meta, sizeof(meta), "%s", progress);
  } else if (date && date[0]) {
    snprintf(meta, sizeof(meta), "%s", date);
  }
  if (meta[0]) {
    const int metaW = textW - (statusW > 0 ? statusW + 14 : 0);
    const auto safe = renderer.truncatedText(SMALL_FONT_ID, meta, metaW);
    renderer.drawText(SMALL_FONT_ID, textX, footerY, safe.c_str(), true);
  }
}

void drawActionTile(GfxRenderer& renderer, Rect rect, bool selected, UIIcon icon, const char* label,
                    const char* description) {
  drawSelectionFrame(renderer, rect, selected);

  constexpr int iconSize = 64;
  const int textX = rect.x + 18;
  const int textW = rect.width - 36;
  const auto title = renderer.truncatedText(UI_10_FONT_ID, label, textW, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, textX, rect.y + 24, title.c_str(), true, EpdFontFamily::BOLD);

  const auto descLines = renderer.wrappedText(SMALL_FONT_ID, description, textW, 3);
  int descY = rect.y + 52;
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);
  for (const auto& line : descLines) {
    renderer.drawText(SMALL_FONT_ID, textX, descY, line.c_str(), true);
    descY += lhSm + 3;
  }

  const int iconX = rect.x + rect.width - iconSize - 22;
  const int iconY = rect.y + rect.height - iconSize - 38;
  if (const uint8_t* bmp = iconForName(icon, iconSize)) {
    renderer.drawIcon(bmp, iconX, iconY, iconSize, iconSize);
  }

  renderer.drawLine(textX, rect.y + rect.height - 18, rect.x + rect.width - 18, rect.y + rect.height - 18);
}
}  // namespace

// ---------------------------------------------------------------------------
// loadLastRead — scans readings dir for the most recently read book
// ---------------------------------------------------------------------------

void HomeActivity::loadLastRead() {
  hasLastRead = false;
  lastReadTitle[0] = '\0';
  lastReadAuthor[0] = '\0';
  lastReadProgress[0] = '\0';
  lastReadDate[0] = '\0';
  lastReadStatus[0] = '\0';

  if (!Storage.exists(ReadingLog::DIR_PATH)) return;

  HalFile dir = Storage.open(ReadingLog::DIR_PATH);
  if (!dir || !dir.isDirectory()) return;

  char bestBookId[32] = {};
  char bestDate[11] = {};
  char bestTime[6] = {};  // "HH:MM"
  int bestPos = 0;
  int bestType = 0;          // 0=Page, 1=Chapter
  char bestStatus[12] = {};  // "reading", "finished", etc.

  static constexpr size_t BUF_SIZE = 4096;
  auto* buf = static_cast<char*>(malloc(BUF_SIZE));
  if (!buf) return;

  while (true) {
    HalFile entry = dir.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) continue;

    char name[32];
    entry.getName(name, sizeof(name));
    const size_t nameLen = strlen(name);
    if (nameLen < 6 || strcmp(name + nameLen - 5, ".json") != 0) continue;

    const size_t idLen = nameLen - 5;
    if (idLen >= sizeof(bestBookId)) continue;

    char bookId[32] = {};
    memcpy(bookId, name, idLen);

    const size_t n = entry.read(buf, BUF_SIZE - 1);
    buf[n] = '\0';

    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok) continue;

    JsonArray readings = doc.as<JsonArray>();
    if (readings.isNull()) continue;

    for (JsonObject r : readings) {
      JsonArray sessions = r["sessions"].as<JsonArray>();
      if (sessions.isNull()) continue;
      for (JsonObject s : sessions) {
        const char* d = s["d"] | "";
        if (d[0] != '\0' && (bestDate[0] == '\0' || strcmp(d, bestDate) > 0)) {
          strncpy(bestDate, d, sizeof(bestDate) - 1);
          strncpy(bestBookId, bookId, sizeof(bestBookId) - 1);
          bestPos = s["p"] | 0;
          bestType = r["rt"] | 0;
          const char* st = r["s"] | "reading";
          strncpy(bestStatus, st, sizeof(bestStatus) - 1);
          const char* tm = s["tm"] | "";
          strncpy(bestTime, tm, sizeof(bestTime) - 1);
        }
      }
    }
  }
  free(buf);

  if (bestBookId[0] == '\0') return;

  // Load book JSON for title/author
  char bookPath[80];
  snprintf(bookPath, sizeof(bookPath), "%s/%s.json", BookStore::DIR_PATH, bestBookId);
  if (!Storage.exists(bookPath)) return;

  static constexpr size_t BOOK_BUF = 512;
  auto* bbuf = static_cast<char*>(malloc(BOOK_BUF));
  if (!bbuf) return;

  const size_t n = Storage.readFileToBuffer(bookPath, bbuf, BOOK_BUF);
  bbuf[n] = '\0';

  JsonDocument doc;
  const bool ok = (deserializeJson(doc, bbuf) == DeserializationError::Ok);
  free(bbuf);
  if (!ok) return;

  const char* title = doc["t"] | "";
  const char* author = doc["a"] | "";
  if (title[0] == '\0') return;

  strncpy(lastReadTitle, title, sizeof(lastReadTitle) - 1);
  strncpy(lastReadAuthor, author, sizeof(lastReadAuthor) - 1);

  // Format progress: "p. 42" or "ch. 3"
  const char* unit = (bestType == 1) ? "ch." : "p.";
  snprintf(lastReadProgress, sizeof(lastReadProgress), "%s %d", unit, bestPos);

  // Map raw status to i18n string
  {
    StrId sid = StrId::STR_STATUS_READING;
    if (strcmp(bestStatus, "want") == 0)
      sid = StrId::STR_STATUS_WANT_TO_READ;
    else if (strcmp(bestStatus, "paused") == 0)
      sid = StrId::STR_STATUS_PAUSED;
    else if (strcmp(bestStatus, "finished") == 0)
      sid = StrId::STR_STATUS_FINISHED;
    else if (strcmp(bestStatus, "dropped") == 0)
      sid = StrId::STR_STATUS_DROPPED;
    strncpy(lastReadStatus, trId(sid), sizeof(lastReadStatus) - 1);
  }

  // Format date "Jan '26 21:00" from "YYYY-MM-DD" + "HH:MM"
  if (bestDate[0] != '\0' && strlen(bestDate) >= 7) {
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
    const int month = (bestDate[5] - '0') * 10 + (bestDate[6] - '0') - 1;
    if (month >= 0 && month < 12) {
      if (bestTime[0] != '\0') {
        snprintf(lastReadDate, sizeof(lastReadDate), "%s '%c%c %s", shortMonth(month), bestDate[2], bestDate[3],
                 bestTime);
      } else {
        snprintf(lastReadDate, sizeof(lastReadDate), "%s '%c%c", shortMonth(month), bestDate[2], bestDate[3]);
      }
    }
  }

  hasLastRead = true;
}

// ---------------------------------------------------------------------------
// Activity lifecycle
// ---------------------------------------------------------------------------

void HomeActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  loadLastRead();
  requestUpdate();
}

void HomeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
      mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selectorIndex = (selectorIndex + 1) % kMenuCount;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
      mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selectorIndex = (selectorIndex - 1 + kMenuCount) % kMenuCount;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleasedGroup(MappedInputManager::ButtonGroup::BottomLeft)) {
    switch (selectorIndex) {
      case 0:
        onLastReadOpen();
        break;
      case 1:
        onPhysicalBooksOpen();
        break;
      case 2:
        onReadingStatsOpen();
        break;
      case 3:
        onFileTransferOpen();
        break;
      case 4:
        onSettingsOpen();
        break;
      default:
        break;
    }
  }
}

void HomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageW = renderer.getScreenWidth();
  const auto pageH = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight});

  const char* labels[] = {tr(STR_LAST_READ), tr(STR_PHYSICAL_BOOKS), tr(STR_READING_STATS), tr(STR_NETWORK),
                          tr(STR_SETTINGS_TITLE)};
  const char* descriptions[] = {tr(STR_DESC_LAST_READ), tr(STR_DESC_PHYSICAL_BOOKS), tr(STR_DESC_READING_STATS),
                                tr(STR_DESC_NETWORK), tr(STR_DESC_SETTINGS)};
  const UIIcon icons[] = {UIIcon::BookHeartIcon, UIIcon::LibraryBigIcon, UIIcon::ChartIcon, UIIcon::NetworkIcon,
                          UIIcon::SettingsIcon};

  const int top = metrics.topPadding + metrics.headerHeight + 8;
  const int heroH = 190;
  drawLastReadTile(renderer, Rect{kHomePad, top, pageW - kHomePad * 2, heroH}, selectorIndex == 0, labels[0],
                   descriptions[0], hasLastRead, lastReadTitle, lastReadAuthor, lastReadProgress, lastReadDate,
                   lastReadStatus);

  constexpr int cols = 2;
  constexpr int gap = 14;
  const int gridTop = top + heroH + 16;
  const int gridBottom = pageH - metrics.buttonHintsHeight - 12;
  const int tileW = (pageW - kHomePad * 2 - gap) / cols;
  const int tileH = (gridBottom - gridTop - gap) / 2;

  for (int i = 1; i < kMenuCount; ++i) {
    const int zero = i - 1;
    const int col = zero % cols;
    const int row = zero / cols;
    const int x = kHomePad + col * (tileW + gap);
    const int y = gridTop + row * (tileH + gap);
    drawActionTile(renderer, Rect{x, y, tileW, tileH}, selectorIndex == i, icons[i], labels[i], descriptions[i]);
  }

  const auto btnLabels = mappedInput.mapLabels("", tr(STR_SELECT), tr(STR_PREV), tr(STR_NEXT));
  GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);

  renderer.displayBuffer();
}

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }
void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }
void HomeActivity::onPhysicalBooksOpen() { activityManager.goToPhysicalBooks(); }
void HomeActivity::onReadingStatsOpen() { activityManager.goToReadingStats(); }
void HomeActivity::onLastReadOpen() { activityManager.goToLastRead(); }
