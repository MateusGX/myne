#pragma once

#include <BookCatalog.h>
#include <BookStore.h>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Shows all entries (collections and standalone books) for one letter.
// Loads only one screen-page of entries at a time — the total catalog can be
// arbitrarily large; the page buffer is the only in-memory cost.
// Collection rows open CollectionBooksActivity; book rows open PhysicalBookDetailActivity.
class LetterBooksActivity final : public Activity {
  char letter_;

  // Pagination state — all counts are absolute indices into the full letter list.
  int totalCount_ = 0;  // total entries for this letter (from idx.bin)
  int selIdx_ = 0;      // absolute selected entry index
  int pageItems_ = 0;   // entries per screen page (from UITheme, cached in onEnter)
  int bufStart_ = -1;   // index of first entry currently in buf_
  int bufCount_ = 0;    // number of valid entries in buf_

  BookCatalog::Entry* buf_ = nullptr;  // heap-allocated [pageItems_] entries

  ButtonNavigator buttonNavigator;

  // Reload buf_ for the page that contains selIdx_.
  // Does nothing if the correct page is already loaded.
  void ensureBuf();

  void openSelected();

 public:
  LetterBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, char letter)
      : Activity("LetterBooks", renderer, mappedInput), letter_(letter) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
