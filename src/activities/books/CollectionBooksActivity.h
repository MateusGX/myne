#pragma once

#include <BookCatalog.h>
#include <BookStore.h>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Shows the books within one collection.
// Loads only one screen-page of entries at a time — collection size is unbounded.
// Book rows open PhysicalBookDetailActivity.
class CollectionBooksActivity final : public Activity {
  char collHash_[9];   // 8-hex hash + NUL
  char collName_[33];  // collection name for the header
  char collNote_[65];  // optional collection note

  // Pagination state
  int totalCount_ = 0;  // total books in this collection
  int selIdx_ = 0;      // absolute selected book index
  int pageItems_ = 0;   // entries per screen page (from UITheme, cached in onEnter)
  int bufStart_ = -1;   // index of first entry currently in buf_
  int bufCount_ = 0;    // number of valid entries in buf_

  BookCatalog::Entry* buf_ = nullptr;  // heap-allocated [pageItems_] entries

  ButtonNavigator buttonNavigator;

  void ensureBuf();
  void openSelected();

 public:
  CollectionBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* collHash, int count,
                          const char* collName = nullptr)
      : Activity("CollectionBooks", renderer, mappedInput), totalCount_(count) {
    strncpy(collHash_, collHash, 8);
    collHash_[8] = '\0';
    if (collName && collName[0]) {
      strncpy(collName_, collName, 32);
      collName_[32] = '\0';
    } else {
      collName_[0] = '\0';
    }
  }
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
