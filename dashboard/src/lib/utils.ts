import { clsx, type ClassValue } from "clsx"
import { twMerge } from "tailwind-merge"

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

// Mirrors REG_NAME_MAX in lib/DataStore/books/BookCatalog.cpp — the max raw
// bytes the on-device collection registry stores for a name. Names longer
// than this get truncated on-device too, which would desync the dashboard's
// name -> id lookup, so we cap input here to match.
export const MAX_COLLECTION_NAME_BYTES = 96

// Truncates `s` so its UTF-8 byte length is <= maxBytes, without splitting a
// multi-byte character.
export function truncateUtf8(s: string, maxBytes: number): string {
  const encoded = new TextEncoder().encode(s)
  if (encoded.length <= maxBytes) return s
  let end = maxBytes
  while (end > 0 && (encoded[end] & 0xc0) === 0x80) end--
  return new TextDecoder().decode(encoded.subarray(0, end))
}
