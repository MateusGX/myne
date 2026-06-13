#include "DataStoreUtils.h"

#include <Arduino.h>
#ifndef SIMULATOR
#include <esp_system.h>
#else
#include <cstdlib>
#endif

#include <cstdio>
#include <ctime>

namespace DataStoreUtils {

void generateId(char* out, size_t size) {
  snprintf(out, size, "%lu", (unsigned long)millis());
}

void currentDate(char* out) {
  time_t now = time(nullptr);
  if (now < 1000000000L) {
    out[0] = '\0';
    return;
  }
  struct tm t;
  localtime_r(&now, &t);
  snprintf(out, 11, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

void generateHexId(char* out, size_t size) {
#ifdef SIMULATOR
  // esp_random() is not available on the native simulator platform; fall
  // back to a seeded rand() for unique-enough IDs during local testing.
  static bool seeded = false;
  if (!seeded) {
    srand(static_cast<unsigned int>(time(nullptr)));
    seeded = true;
  }
  snprintf(out, size, "%08lx", static_cast<unsigned long>(rand()));
#else
  snprintf(out, size, "%08lx", static_cast<unsigned long>(esp_random()));
#endif
}

}  // namespace DataStoreUtils
