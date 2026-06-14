#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/Logo120.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const int W = renderer.getScreenWidth();
  const int H = renderer.getScreenHeight();
  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhSm = renderer.getLineHeight(SMALL_FONT_ID);

  static constexpr int logoSize = 120;

  renderer.clearScreen();
  renderer.drawImage(Logo120, (W - logoSize) / 2, (H - logoSize) / 2, logoSize, logoSize);

  const int nameY = H / 2 + logoSize / 2 + 16;
  renderer.drawCenteredText(UI_10_FONT_ID, nameY, tr(STR_MYNE), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, nameY + lh10 + 8, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, H - lhSm - 16, MYNE_VERSION);

  renderer.displayBuffer();
}
