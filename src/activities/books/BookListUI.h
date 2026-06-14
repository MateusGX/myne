#pragma once

#include <BookCatalog.h>
#include <GfxRenderer.h>

#include "components/MyneUI.h"

namespace BookListUI {

constexpr int kRowHeight = 112;
constexpr int kSectionRowHeight = 78;
constexpr int kPad = 20;

int pageItemsForSections(const GfxRenderer& renderer);
int pageItemsForLetter(const GfxRenderer& renderer);
int pageItemsForCollection(const GfxRenderer& renderer);

void drawSectionRow(const GfxRenderer& renderer, Rect row, char letter, int count, int itemNumber, bool selected);
void drawEntryRow(const GfxRenderer& renderer, Rect row, const BookCatalog::Entry& entry, int itemNumber,
                  bool selected);

}  // namespace BookListUI
