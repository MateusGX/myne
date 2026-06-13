#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/readinglog"
BINARY="$BUILD_DIR/ReadingLogTest"
SIM_DIR="$ROOT_DIR/.pio/libdeps/simulator/simulator/src"

mkdir -p "$BUILD_DIR"

SOURCES=(
  "$ROOT_DIR/test/readinglog/ReadingLogTest.cpp"
  "$ROOT_DIR/lib/DataStore/readings/ReadingLog.cpp"
  "$ROOT_DIR/lib/DataStore/books/BookStore.cpp"
  "$ROOT_DIR/lib/DataStore/core/RecordStore.cpp"
  "$ROOT_DIR/lib/DataStore/core/TimeSeriesStore.cpp"
  "$ROOT_DIR/lib/DataStore/core/DataStoreUtils.cpp"
  "$ROOT_DIR/lib/Logging/Logging.cpp"
  "$SIM_DIR/HalStorage.cpp"
  "$SIM_DIR/ESP.cpp"
)

CXXFLAGS=(
  -std=c++20
  -O2
  -Wall
  -Wextra
  -pedantic
  -DSIMULATOR
  -I"$ROOT_DIR/lib/DataStore"
  -I"$ROOT_DIR/lib/Logging"
  -I"$ROOT_DIR/.pio/libdeps/simulator/ArduinoJson/src"
  -I"$SIM_DIR"
)

c++ "${CXXFLAGS[@]}" "${SOURCES[@]}" -o "$BINARY"

"$BINARY" "$@"
