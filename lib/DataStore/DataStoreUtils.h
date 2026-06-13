#pragma once
#include <cstddef>
#include <cstdint>

namespace DataStoreUtils {

// Fills out with a millis()-based unique ID string. out must be >= 16 bytes.
void generateId(char* out, size_t size);

// Fills out with today's date as "YYYY-MM-DD" (requires NTP sync).
// Writes empty string if clock not synced. out must be >= 11 bytes.
void currentDate(char* out);

// Fills out with an 8-hex-char random ID (lowercase). out must be >= 9 bytes.
void generateHexId(char* out, size_t size);

}  // namespace DataStoreUtils
