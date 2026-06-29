#include "PhysicalBookDetailActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Utf8.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "BookReadingStatsActivity.h"
#include "BooksActivityUI.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kPad = 20;
constexpr int kCardR = 8;
constexpr int kInner = 16;
constexpr int kLineGap = 2;

const char* statusLabel(ReadingStatus s) {
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

const char* dashIfEmpty(const std::string& s) { return s.empty() ? "-" : s.c_str(); }

void drawCard(const GfxRenderer& renderer, Rect rect, bool gray = false) {
  if (gray) renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, kCardR, Color::LightGray);
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, kCardR, true);
}

int textBlockHeight(const GfxRenderer& renderer, int fontId, size_t lineCount) {
  if (lineCount == 0) return 0;
  return static_cast<int>(lineCount) * renderer.getLineHeight(fontId) + (static_cast<int>(lineCount) - 1) * kLineGap;
}

std::vector<std::string> wrappedTextFull(const GfxRenderer& renderer, int fontId, const char* text, int maxWidth,
                                         EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  std::vector<std::string> lines;
  if (!text || maxWidth <= 0) return lines;

  std::string current;
  std::string word;

  auto flushWord = [&] {
    while (!word.empty()) {
      const std::string candidate = current.empty() ? word : current + " " + word;
      if (renderer.getTextWidth(fontId, candidate.c_str(), style) <= maxWidth) {
        current = candidate;
        word.clear();
        return;
      }

      if (!current.empty()) {
        lines.push_back(current);
        current.clear();
        continue;
      }

      std::string fitted;
      const unsigned char* p = reinterpret_cast<const unsigned char*>(word.c_str());
      const unsigned char* split = p;
      while (*p != 0) {
        const unsigned char* cpStart = p;
        utf8NextCodepoint(&p);
        std::string next = fitted;
        next.append(reinterpret_cast<const char*>(cpStart), p - cpStart);
        if (!fitted.empty() && renderer.getTextWidth(fontId, next.c_str(), style) > maxWidth) {
          split = cpStart;
          break;
        }
        fitted = next;
        split = p;
      }

      if (fitted.empty()) {
        fitted = word.substr(0, 1);
        split = reinterpret_cast<const unsigned char*>(word.c_str()) + 1;
      }

      lines.push_back(fitted);
      word.erase(0, split - reinterpret_cast<const unsigned char*>(word.c_str()));
    }
  };

  const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
  while (*p != 0) {
    const unsigned char* cpStart = p;
    const uint32_t cp = utf8NextCodepoint(&p);
    if (cp == '\n') {
      flushWord();
      if (!current.empty()) {
        lines.push_back(current);
        current.clear();
      }
      continue;
    }
    if (cp == ' ' || cp == '\t' || cp == '\r') {
      flushWord();
      continue;
    }
    word.append(reinterpret_cast<const char*>(cpStart), p - cpStart);
  }
  flushWord();
  if (!current.empty()) lines.push_back(current);

  if (lines.empty() && text[0] != '\0') lines.push_back(text);
  return lines;
}

void drawWrappedLines(const GfxRenderer& renderer, int fontId, int x, int y, const std::vector<std::string>& lines,
                      EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const int lh = renderer.getLineHeight(fontId);
  for (const auto& line : lines) {
    renderer.drawText(fontId, x, y, line.c_str(), true, style);
    y += lh + kLineGap;
  }
}

int bookHeroHeight(const GfxRenderer& renderer, int width, const char* title, const char* detail) {
  const int textW = width - BooksActivityUI::INNER * 2;
  const auto titleLines = wrappedTextFull(renderer, UI_10_FONT_ID, title, textW, EpdFontFamily::BOLD);
  const auto detailLines = detail && detail[0] != '\0' ? wrappedTextFull(renderer, SMALL_FONT_ID, detail, textW)
                                                       : std::vector<std::string>{};
  const int contentH = 14 + renderer.getLineHeight(SMALL_FONT_ID) + 8 +
                       textBlockHeight(renderer, UI_10_FONT_ID, titleLines.size()) +
                       (detailLines.empty() ? 0 : 8 + textBlockHeight(renderer, SMALL_FONT_ID, detailLines.size())) +
                       14;
  return std::max(BooksActivityUI::HERO_H, contentH);
}

void drawBookHero(const GfxRenderer& renderer, Rect rect, const char* eyebrow, const char* title, const char* detail) {
  BooksActivityUI::panel(renderer, rect, true);
  const int textW = rect.width - BooksActivityUI::INNER * 2;
  const int textX = rect.x + BooksActivityUI::INNER;
  int y = rect.y + 14;

  renderer.drawText(SMALL_FONT_ID, textX, y, eyebrow, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(SMALL_FONT_ID) + 8;

  const auto titleLines = wrappedTextFull(renderer, UI_10_FONT_ID, title, textW, EpdFontFamily::BOLD);
  drawWrappedLines(renderer, UI_10_FONT_ID, textX, y, titleLines, EpdFontFamily::BOLD);
  y += textBlockHeight(renderer, UI_10_FONT_ID, titleLines.size());

  if (detail && detail[0] != '\0') {
    y += 8;
    const auto detailLines = wrappedTextFull(renderer, SMALL_FONT_ID, detail, textW);
    drawWrappedLines(renderer, SMALL_FONT_ID, textX, y, detailLines);
  }
}

void drawLabelValue(const GfxRenderer& renderer, Rect rect, const char* label, const char* value,
                    int valueFont = UI_10_FONT_ID, int maxLines = 1) {
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);
  const int lhVal = renderer.getLineHeight(valueFont);
  renderer.drawText(SMALL_FONT_ID, rect.x, rect.y, label, true, EpdFontFamily::BOLD);

  int y = rect.y + lhSm + 8;
  if (maxLines <= 1) {
    const auto safe = renderer.truncatedText(valueFont, value, rect.width,
                                             valueFont == UI_10_FONT_ID ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    renderer.drawText(valueFont, rect.x, y, safe.c_str(), true,
                      valueFont == UI_10_FONT_ID ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    return;
  }

  const auto lines = renderer.wrappedText(valueFont, value, rect.width, maxLines,
                                          valueFont == UI_10_FONT_ID ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  for (const auto& line : lines) {
    renderer.drawText(valueFont, rect.x, y, line.c_str(), true,
                      valueFont == UI_10_FONT_ID ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    y += lhVal + 2;
  }
}

int fullLabelValueCardHeight(const GfxRenderer& renderer, int width, const char* value,
                             int valueFont = UI_10_FONT_ID) {
  const auto style = valueFont == UI_10_FONT_ID ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  const auto lines = wrappedTextFull(renderer, valueFont, value, width - kInner * 2, style);
  return std::max(74, 14 + renderer.getLineHeight(SMALL_FONT_ID) + 8 +
                          textBlockHeight(renderer, valueFont, lines.size()) + 14);
}

void drawFullMetaCard(const GfxRenderer& renderer, Rect rect, const char* label, const char* value,
                      int valueFont = UI_10_FONT_ID) {
  const auto style = valueFont == UI_10_FONT_ID ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  drawCard(renderer, rect);
  renderer.drawText(SMALL_FONT_ID, rect.x + kInner, rect.y + 14, label, true, EpdFontFamily::BOLD);
  const auto lines = wrappedTextFull(renderer, valueFont, value, rect.width - kInner * 2, style);
  drawWrappedLines(renderer, valueFont, rect.x + kInner, rect.y + 14 + renderer.getLineHeight(SMALL_FONT_ID) + 8, lines,
                   style);
}

}  // namespace

void PhysicalBookDetailActivity::onEnter() {
  Activity::onEnter();

  ReadingSummary summary;
  hasReading = readingLog.loadSummaryForBook(book.id, summary) && summary.hasReading;
  if (hasReading) {
    lastStatus = summary.status;
    lastType = summary.readingType;
    lastPosition = summary.lastPosition;
    memcpy(lastDate, summary.lastDate, sizeof(lastDate));
  } else {
    lastDate[0] = '\0';
  }

  requestUpdate();
}

void PhysicalBookDetailActivity::onExit() { Activity::onExit(); }

void PhysicalBookDetailActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.popActivity();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activityManager.goToBookReadings(book);
    return;
  }
  if (mappedInput.wasReleasedGroup(MappedInputManager::ButtonGroup::BottomRight)) {
    startActivityForResult(std::make_unique<BookReadingStatsActivity>(renderer, mappedInput, book),
                           [this](const ActivityResult&) { requestUpdate(); });
  }
}

void PhysicalBookDetailActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int W = renderer.getScreenWidth();
  const auto& m = UITheme::getInstance().getMetrics();
  const int CW = W - 2 * kPad;

  GUI.drawHeader(renderer, Rect{0, m.topPadding, W, m.headerHeight});
  int y = m.topPadding + m.headerHeight + 8;

  // Hero
  {
    const char* author = dashIfEmpty(book.author);
    const int heroH = bookHeroHeight(renderer, CW, book.title.c_str(), author);
    drawBookHero(renderer, Rect{kPad, y, CW, heroH}, tr(STR_BOOK_DETAIL), book.title.c_str(), author);
    y += heroH + 14;
  }

  // Metadata
  {
    const int gap = 10;
    const int collectionH = fullLabelValueCardHeight(renderer, CW, dashIfEmpty(book.collection));
    drawFullMetaCard(renderer, Rect{kPad, y, CW, collectionH}, tr(STR_BOOK_COLLECTION), dashIfEmpty(book.collection));
    y += collectionH + gap;

    const int locationH = fullLabelValueCardHeight(renderer, CW, dashIfEmpty(book.location));
    drawFullMetaCard(renderer, Rect{kPad, y, CW, locationH}, tr(STR_BOOK_LOCATION), dashIfEmpty(book.location));
    y += locationH + 12;
  }

  // Notes
  {
    const int notesH = 96;
    drawCard(renderer, Rect{kPad, y, CW, notesH});
    const char* notes = book.notes.empty() ? "-" : book.notes.c_str();
    drawLabelValue(renderer, Rect{kPad + kInner, y + 14, CW - kInner * 2, notesH - 24}, tr(STR_BOOK_NOTES), notes,
                   SMALL_FONT_ID, 3);
    y += notesH + 12;
  }

  // Reading summary / action card
  {
    const int cardH = 132;
    drawCard(renderer, Rect{kPad, y, CW, cardH});
    renderer.fillRoundedRect(kPad, y, 8, cardH, kCardR, true, false, true, false, Color::Black);
    const int tx = kPad + kInner;
    renderer.drawText(SMALL_FONT_ID, tx, y + 16, tr(STR_READINGS), true, EpdFontFamily::BOLD);

    const char* status = hasReading ? statusLabel(lastStatus) : tr(STR_NO_READINGS);
    char posBuf[32] = {};
    if (hasReading && lastPosition > 0) {
      const char* unit = (lastType == ReadingType::Chapter) ? "ch." : "p.";
      if (lastDate[0] != '\0' && strlen(lastDate) >= 7) {
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
        const int month = (lastDate[5] - '0') * 10 + (lastDate[6] - '0') - 1;
        if (month >= 0 && month < 12) {
          char dateBuf[10];
          snprintf(dateBuf, sizeof(dateBuf), "%s '%c%c", shortMonth(month), lastDate[2], lastDate[3]);
          snprintf(posBuf, sizeof(posBuf), "%s %d \xc2\xb7 %s", unit, lastPosition, dateBuf);
        } else {
          snprintf(posBuf, sizeof(posBuf), "%s %d", unit, lastPosition);
        }
      } else {
        snprintf(posBuf, sizeof(posBuf), "%s %d", unit, lastPosition);
      }
    }
    const int colW = (CW - kInner * 3) / 2;
    const int rowY = y + 48;
    drawLabelValue(renderer, Rect{tx, rowY, colW, 52}, tr(STR_READ_STATUS), status);
    drawLabelValue(renderer, Rect{tx + colW + kInner, rowY, colW, 52}, tr(STR_LOG_POSITION), posBuf[0] ? posBuf : "-");
  }

  const auto btnLabels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_READINGS), tr(STR_READING_STATS), "");
  GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);

  renderer.displayBuffer();
}
