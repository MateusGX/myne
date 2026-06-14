#include "PhysicalBookDetailActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>

#include "BookReadingStatsActivity.h"
#include "BooksActivityUI.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kPad = 20;
constexpr int kCardR = 8;
constexpr int kInner = 16;

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

void drawMetaCard(const GfxRenderer& renderer, Rect rect, const char* label, const char* value) {
  drawCard(renderer, rect);
  drawLabelValue(renderer, Rect{rect.x + kInner, rect.y + 14, rect.width - kInner * 2, rect.height - 24}, label, value);
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
    const auto author =
        renderer.truncatedText(SMALL_FONT_ID, dashIfEmpty(book.author), CW - BooksActivityUI::INNER * 2);
    BooksActivityUI::hero(renderer, Rect{kPad, y, CW, BooksActivityUI::HERO_H}, tr(STR_BOOK_DETAIL), book.title.c_str(),
                          author.c_str());
    y += BooksActivityUI::HERO_H + 14;
  }

  // Metadata grid
  {
    const int gap = 10;
    const int cardW = (CW - gap) / 2;
    const int cardH = 74;
    drawMetaCard(renderer, Rect{kPad, y, cardW, cardH}, tr(STR_BOOK_COLLECTION), dashIfEmpty(book.collection));
    drawMetaCard(renderer, Rect{kPad + cardW + gap, y, cardW, cardH}, tr(STR_BOOK_LOCATION),
                 dashIfEmpty(book.location));
    y += cardH + 12;
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
