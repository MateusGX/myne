#pragma once

#include "../Activity.h"
#include <BookCatalog.h>
#include "util/ButtonNavigator.h"

// Entry point for physical book browsing.
// Reads idx.bin (26 × uint16_t) and shows a list of letters that have books.
// Selecting a letter opens LetterBooksActivity.
class LetterPickerActivity final : public Activity {
  struct LetterEntry {
    char letter;
    int  count;   // total entries for this letter (collections + standalone books)
  };

  static constexpr int MAX_LETTERS = 27;  // A-Z + '#' for digits/symbols
  LetterEntry letters[MAX_LETTERS];
  int         letterCount   = 0;
  int         selectorIndex = 0;

  ButtonNavigator buttonNavigator;

  void loadLetters();
  void openSelected();

 public:
  explicit LetterPickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("LetterPicker", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
