# Myne Development Guide

Project: Myne (https://github.com/MateusGX/myne) — a personal fork of CrossPoint, firmware for the Xteink X4 (ESP32-C3) e-ink device.
Mission: Catalog a library of physical books and track reading sessions for them, on constrained e-ink hardware. On-device browsing, logging, and stats, plus Wi-Fi file transfer, a companion web dashboard, and OTA updates. No EPUB/e-book reading — Myne tracks reading *of* physical books, not their content.

## AI Agent Identity and Cognitive Rules
* Role: Senior Embedded Systems Engineer (ESP-IDF/Arduino-ESP32 specialized).
* Primary Constraint: 380KB RAM is the hard ceiling. Stability is non-negotiable.
* Evidence-Based Reasoning: Before proposing a change, you MUST cite the specific file path and line numbers that justify the modification.
* Anti-Hallucination: Do not assume the existence of libraries or ESP-IDF functions. If you are unsure of an API's availability for the ESP32-C3 RISC-V target, check the open-x4-sdk or official docs first.
* No Unfounded Claims: Do not claim performance gains or memory savings without explaining the technical mechanism (e.g., DRAM vs IRAM usage).
* Resource Justification: You must justify any new heap allocation (new, malloc, std::vector) or explain why a stack/static alternative was rejected.
* Verification: After suggesting a fix, instruct the user on how to verify it (e.g., monitoring heap via Serial or checking a specific cache file).
---

## Development Environment Awareness

**CRITICAL**: Detect the host platform at session start to choose appropriate tools and commands.

### Platform Detection
```bash
# Detect platform (run once per session)
uname -s
# Returns: MINGW64_NT-* (Windows Git Bash), Linux, Darwin (macOS)
```

**Detection Required**: Run `uname -s` at session start to determine platform

### Platform-Specific Behaviors
- **Windows (Git Bash)**: Unix commands, `C:\` paths in Windows but `/` in bash, limited glob (use `find`+`xargs`)
- **Linux/WSL**: Full bash, Unix paths, native glob support

**Cross-Platform Code Formatting**:
```bash
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

---

## Platform and Hardware Constraints

### Hardware Specs
* MCU: ESP32-C3 (Single-core RISC-V @ 160MHz)
* RAM: ~380KB usable (VERY LIMITED - primary project constraint)
  * **NO PSRAM**: ESP32-C3 has no PSRAM capability (unlike ESP32-S3)
  * **Single Buffer Mode**: Only ONE 48KB framebuffer (not double-buffered)
* Flash: 16MB (Instruction storage and static data)
* Display: 800x480 E-Ink (Slow refresh, monochrome, 1-2s full update)
  * Framebuffer: 48,000 bytes (800 × 480 ÷ 8)
* Storage: SD Card (Used for files — no binary caching)

### The Resource Protocol
1. Stack Safety: Limit local function variables to < 256 bytes. The ESP32-C3 default stack is small; use std::unique_ptr or static pools for larger buffers.
2. Heap Fragmentation: Avoid repeated new/delete in loops. Allocate buffers once during onEnter() and reuse them.
3. Flash Persistence: Large constant data (UI strings, lookup tables) MUST be marked static const to stay in Flash (Instruction Bus), freeing DRAM.
4. String Policy: Prohibit std::string and Arduino String in hot paths. Use std::string_view for read-only access and snprintf with fixed char[] buffers for construction.
5. UI Strings: All user-facing text must use the `tr()` macro (e.g., `tr(STR_LOADING)`) for i18n support. Never hardcode UI strings directly. For the avoidance of doubt, logging messages (LOG_DBG/LOG_ERR) can be hardcoded, but user-facing text must use `tr()`.
6. `constexpr` First: Compile-time constants and lookup tables must be `constexpr`, not just `static const`. This moves computation to compile time, enables dead-branch elimination, and guarantees flash placement. Use `static constexpr` for class-level constants.
7. `std::vector` Pre-allocation: Always call `.reserve(N)` before any `push_back()` loop. Each growth event allocates a new block (2×), copies all elements, then frees the old one — three heap operations that fragment DRAM. When the final size is unknown, estimate conservatively.
8. SPIFFS Write Throttling: Never write a settings file on every user interaction. Guard all writes with a value-change check (`if (newVal == _current) return;`). SPIFFS sectors have a finite erase cycle limit.

---

## Project Architecture

### Build System: PlatformIO

**PlatformIO is BOTH a VS Code extension AND a CLI tool**:

1. **VS Code Extension** (Recommended):
   * Extension ID: `platformio.platformio-ide` (see `.vscode/extensions.json`)
   * Provides: Toolbar buttons, IntelliSense, integrated build/upload/monitor
   * Configuration: `.vscode/c_cpp_properties.json`, `.vscode/tasks.json`
   * Usage: Click Build (✓), Upload (→), or Monitor (🔌) buttons

2. **CLI Tool** (`pio` command):
   * **Installation**: Python package (typically `pip install platformio`)
   * **Windows Location**: `C:\Users\<user>\AppData\Local\Programs\Python\Python3xx\Scripts\pio.exe`
   * **Verify**: `which pio` (Git Bash) or `where.exe pio` (cmd)
   * **Usage**: `pio run`, `pio run -t upload`, etc.

**Configuration Files**:
* `platformio.ini`: Main build configuration (committed to git)
* `platformio.local.ini`: Local overrides (gitignored, create if needed)
* `partitions.csv`: ESP32 flash partition layout

### Build Environment
* **Standard**: C++20 (`-std=c++2a`). No Exceptions, No RTTI.
* **Logging**: ALWAYS use `LOG_INF`, `LOG_DBG`, or `LOG_ERR` from `Logging.h`. Raw Serial output is deprecated.
* **Environments** (in `platformio.ini`):
  * `default`: Development (LOG_LEVEL=2, serial enabled)
  * `gh_release`: Production (LOG_LEVEL=0)
  * `gh_release_rc`: Release candidate (LOG_LEVEL=1)
  * `slim`: Minimal build (no serial logging)

### Critical Build Flags
These flags in `platformio.ini` fundamentally affect firmware behavior:

```cpp
-DEINK_DISPLAY_SINGLE_BUFFER_MODE=1  // Single framebuffer (saves 48KB RAM!)
-DARDUINO_USB_MODE=1                 // Enable USB CDC
-DARDUINO_USB_CDC_ON_BOOT=1          // Serial available immediately at boot
-DUSE_UTF8_LONG_NAMES=1              // SD card long filename support
-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES=1   // Avoid zlib name conflicts
-DDESTRUCTOR_CLOSES_FILE=1           // FsFile destructor auto-closes (SdFat)
```

**DESTRUCTOR_CLOSES_FILE implications**:
- SdFat's `FsBaseFile` destructor calls `close()` automatically when the object goes out of scope
- **Do NOT add explicit `file.close()` calls** for local `FsFile` variables — the destructor handles it
- Explicit `close()` is still required in these cases:
  1. **Close before delete**: Must close before `Storage.remove()` on the same path
  2. **Close before reopen**: Must close before reopening the same `FsFile` variable (e.g., write then reopen for read, or rewrite the same path)
  3. **Member variables**: `FsFile` members persist beyond any single function scope, so close at the intended release point (e.g., in `onExit()`)

**SINGLE_BUFFER_MODE implications**:
- Only ONE framebuffer exists (not double-buffered)
- Grayscale rendering requires temporary buffer allocation (`renderer.storeBwBuffer()`)
- Must call `renderer.restoreBwBuffer()` to free temporary buffers
- See [lib/GfxRenderer/GfxRenderer.cpp](lib/GfxRenderer/GfxRenderer.cpp) for malloc usage

### Directory Structure
* `lib/`: Internal libraries
  * `lib/hal/`: Hardware Abstraction Layer (HalDisplay, HalGPIO, HalStorage)
  * `lib/I18n/`: Internationalization (translations in `translations/*.yaml`, generated string tables)
  * `lib/GfxRenderer/`: E-ink rendering engine (no SdCardFont, no FontCacheManager)
  * `lib/EpdFont/`: Built-in bitmap fonts only (no SdCardFont, no FontDecompressor)
  * `lib/DataStore/`: On-device physical-book catalog and reading-session storage (`BookCatalog`, `BookStore`, `ReadingLog`, `RecordStore`, `TimeSeriesStore`) — see [docs/book-catalog-format.md](docs/book-catalog-format.md)
  * `lib/FsHelpers/`, `lib/JsonParser/`, `lib/Logging/`, `lib/Serialization/`, `lib/Utf8/`: shared low-level utilities
* `src/activities/`: UI logic using the Activity Lifecycle (onEnter, loop, onExit)
  * `src/activities/home/`: HomeActivity (hero "Last Read" card + 2×2 icon grid)
  * `src/activities/books/`: Physical book catalog & reading-session UI — `LetterPickerActivity`, `LetterBooksActivity`, `CollectionBooksActivity`, `PhysicalBookDetailActivity`, `BookReadingsActivity`, `ReadingEditActivity`, `ReadingStatsActivity`, `CatalogSyncActivity`
  * `src/activities/browser/`: `FileBrowserActivity`
  * `src/activities/settings/`: SettingsActivity (Display, Controls, System)
  * `src/activities/network/`: `MyneWebServerActivity` (Wi-Fi file transfer + dashboard API), `WifiSelectionActivity`
  * `src/activities/boot_sleep/`: `BootActivity`, `SleepActivity`
  * `src/activities/util/`: Shared utilities (keyboard, popups, etc.)
* `src/components/`: UI components
  * `src/components/MyneUI.h/.cpp`: Single flat UI class (no themes, no inheritance)
  * `src/components/UITheme.h/.cpp`: Thin wrapper exposing `GUI` macro → `MyneUI`
  * `src/components/icons/`: Generated 1-bit icon headers (`folder64.h`, `settings64.h`, etc.)
  * `src/components/icons/src/`: Source SVG files for icon generation
* `open-x4-sdk/`: Low-level SDK (EInkDisplay, InputManager, BatteryMonitor, SDCardManager)
* `dashboard/`: Companion React + Vite web dashboard (books, reading sessions, file browser, settings) that talks to the device's `/api/*` endpoints over Wi-Fi
* `scripts/`: Python tooling
  * `scripts/build_icons.py`: Batch-converts all SVGs in `icons/src/` to C headers
  * `scripts/convert_icon.py`: Single-icon SVG/PNG → 1-bit C header converter

**Removed libraries** (no longer in the codebase):
- `lib/Epub/`, `lib/Xtc/`, `lib/Txt/` — reader engines
- `lib/KOReaderSync/`, `lib/OpdsParser/` — sync/discovery
- `lib/ZipFile/`, `lib/InflateReader/`, `lib/expat/`, `lib/uzlib/`, `lib/XmlParserUtils/` — EPUB parsing deps
- `lib/JpegToBmpConverter/`, `lib/PngToBmpConverter/` — image converters

### Hardware Abstraction Layer (HAL)

**CRITICAL**: Always use HAL classes, NOT SDK classes directly.

| HAL Class | Wraps SDK Class | Purpose | Singleton Macro |
|-----------|----------------|---------|-----------------|
| `HalDisplay` | `EInkDisplay` | E-ink display control | *(none)* |
| `HalGPIO` | `InputManager` | Button input handling | *(none)* |
| `HalStorage` | `SDCardManager` | SD card file I/O | `Storage` |

**Location**: [lib/hal/](lib/hal/)

**Why HAL?**
- Provides consistent error logging per module
- Abstracts SDK implementation details
- Centralizes resource management

**Example - HalStorage**:
```cpp
#include <HalStorage.h>

// Use Storage singleton (defined via macro)
FsFile file;
if (Storage.openFileForRead("MODULE", "/path/to/file.bin", file)) {
  // Read from file
  // No file.close() needed — DESTRUCTOR_CLOSES_FILE=1 handles it at scope exit
}
```

**Usage**: See example above. Uses `FsFile` (SdFat), NOT Arduino `File`. Do NOT add `file.close()` for local variables (see DESTRUCTOR_CLOSES_FILE above).

---

## UI Architecture: MyneUI

### UIIcon Enum

Defined in [src/components/icons/Icons.h](src/components/icons/Icons.h):

```cpp
enum class UIIcon {
  FolderIcon, ImageIcon, FileIcon, BookIcon, BookMarkedIcon, LibraryBigIcon,
  FolderBookmarkIcon, NetworkIcon, SettingsIcon, WifiIcon, HotspotIcon,
  BookHeartIcon, ChartIcon, ChevronRightIcon
};
```

`iconForName(UIIcon, int size)` supports sizes 24, 32, and 64 — but not every icon has every size. Check `Icons.h` for the per-icon size mapping before assuming a variant exists.

### Icon Generation Workflow

Icons are 1-bit packed bitmaps stored as C headers in `src/components/icons/`.

**Source SVGs**: `src/components/icons/src/` — filename encodes size: `folder64.svg` → 64×64.

**Single icon**:
```bash
python scripts/convert_icon.py input.svg output_name width height
# Writes: src/components/icons/output_name.h
```

**Batch rebuild all icons**:
```bash
python scripts/build_icons.py                  # All SVGs in icons/src/
python scripts/build_icons.py icons/src/x.svg  # Specific files
python scripts/build_icons.py --default-size 32  # Size fallback when not in filename
```

**Conversion pipeline** ([scripts/convert_icon.py](scripts/convert_icon.py)):
1. SVG → PNG: tries `cairosvg` first, falls back to macOS `qlmanage` (renders at 8× then trims white margins)
2. PNG → RGBA → RGB (white background composite)
3. Resize to target dimensions with LANCZOS
4. Rotate 90° CCW (e-ink display orientation)
5. Threshold to 1-bit (pixels ≥ 128 = white/1, < 128 = black/0)
6. Pack MSB-first into `uint8_t[]`, emit `#pragma once` C header

**Requirements** (Python):
```
pillow>=12.2.0
cairosvg>=2.9.0  # optional; qlmanage used as fallback on macOS
```

**After adding or modifying an SVG**, run `build_icons.py` and commit both the SVG source and the generated `.h` file.

---

## Coding Standards

### Naming Conventions
* Classes: PascalCase (e.g., FileBrowserActivity)
* Methods/Variables: camelCase (e.g., renderPage())
* Constants: UPPER_SNAKE_CASE (e.g., MAX_BUFFER_SIZE)
* Private Members: memberVariable (no prefix)
* File Names: Match Class names (e.g., PhysicalBookDetailActivity.cpp)

### Header Guards
* Use #pragma once for all header files.

### Memory Safety and RAII
* Smart Pointers: Prefer std::unique_ptr. Avoid std::shared_ptr (unnecessary atomic overhead for a single-core RISC-V).
* RAII: Use destructors for cleanup. Call `vTaskDelete()` explicitly for deterministic task release. Do NOT call `file.close()` on local `FsFile` variables — `DESTRUCTOR_CLOSES_FILE=1` handles it at scope exit (see Critical Build Flags).

### ESP32-C3 Platform Pitfalls

#### `std::string_view` and Null Termination
`string_view` is *not* null-terminated. Passing `.data()` to any C-style API (`drawText`, `snprintf`, `strcmp`, SdFat file paths) is undefined behaviour when the view is a substring or a view of a non-null-terminated buffer.

**Rule**: `string_view` is safe only when passing to C++ APIs that accept `string_view`. For any C API boundary, convert explicitly:
```cpp
// WRONG - undefined behaviour if view is a substring:
renderer.drawText(font, x, y, myView.data(), true);

// CORRECT - guaranteed null-terminated:
renderer.drawText(font, x, y, std::string(myView).c_str(), true);

// CORRECT - for short strings, use a stack buffer:
char buf[64];
snprintf(buf, sizeof(buf), "%.*s", (int)myView.size(), myView.data());
```

#### `IRAM_ATTR` and Flash Cache Safety
All code runs from flash via the instruction cache. During SPI flash operations (OTA write, SPIFFS commit, NVS update) the cache is briefly suspended. Any code that can execute during this window — ISRs in particular — must reside in IRAM or it will crash silently.

```cpp
// ISR handler: must be in IRAM
void IRAM_ATTR gpioISR() { ... }

// Data accessed from IRAM_ATTR code: must be in DRAM, never a flash const
static DRAM_ATTR uint32_t isrEventFlags = 0;
```

**Rules**:
- All ISR handlers: `IRAM_ATTR`
- Data read by `IRAM_ATTR` code: `DRAM_ATTR` (a flash-resident `static const` will fault)
- Normal task code does **not** need `IRAM_ATTR`

#### ISR vs Task Shared State
`xSemaphoreTake()` (mutex) **cannot** be called from ISR context — it will crash. Use the correct primitive for each communication direction:

| Direction | Correct primitive |
|---|---|
| ISR → task (data) | `xQueueSendFromISR()` + `portYIELD_FROM_ISR()` |
| ISR → task (signal) | `xSemaphoreGiveFromISR()` + `portYIELD_FROM_ISR()` |
| Task → task | `xSemaphoreTake()` / mutex |
| Simple flag (single writer ISR) | `volatile bool` + `portENTER_CRITICAL_ISR()` |

#### RISC-V Alignment
ESP32-C3 faults on unaligned multi-byte loads. Never cast a `uint8_t*` buffer to a wider pointer type and dereference it directly. Use `memcpy` for any unaligned read:

```cpp
// WRONG — faults if buf is not 4-byte aligned:
uint32_t val = *reinterpret_cast<const uint32_t*>(buf);

// CORRECT:
uint32_t val;
memcpy(&val, buf, sizeof(val));
```

This applies to all cache deserialization code and any raw buffer-to-struct casting. `__attribute__((packed))` structs have the same hazard when accessed via member reference.

#### Template and `std::function` Bloat
Each template instantiation generates a separate binary copy. `std::function<void()>` adds ~2–4 KB per unique signature and heap-allocates its closure. Avoid both in library code and any path called from the render loop:

```cpp
// Avoid — heap-allocating, large binary footprint:
std::function<void()> callback;

// Prefer — zero overhead:
void (*callback)() = nullptr;

// For member function + context (common activity callback pattern):
struct Callback { void* ctx; void (*fn)(void*); };
```

When a template is necessary, limit instantiations: use explicit template instantiation in a `.cpp` file to prevent the compiler from generating duplicates across translation units.

---

### Error Handling Philosophy

**Source**: [src/main.cpp](src/main.cpp), [lib/GfxRenderer/GfxRenderer.cpp](lib/GfxRenderer/GfxRenderer.cpp)

**Pattern Hierarchy**:
1. **LOG_ERR + return false** (90%): `LOG_ERR("MOD", "Failed: %s", reason); return false;`
2. **LOG_ERR + fallback**: `LOG_ERR("MOD", "Unavailable"); useDefault();`
3. **assert(false)**: Only for fatal "impossible" states (framebuffer missing)
4. **ESP.restart()**: Only for recovery (OTA complete)

**Rules**: NO exceptions, NO abort(), ALWAYS log before error return

### Acceptable malloc/free Patterns

**Source**: [lib/GfxRenderer/GfxRenderer.cpp](lib/GfxRenderer/GfxRenderer.cpp)

Despite "prefer stack allocation," malloc is acceptable for:
1. **Large temporary buffers** (> 256 bytes, won't fit on stack)
2. **One-time allocations** during activity initialization
3. **Bitmap rendering buffers** (variable size, used briefly)

**Pattern**:
```cpp
// Allocate
auto* buffer = static_cast<uint8_t*>(malloc(bufferSize));
if (!buffer) {
  LOG_ERR("MODULE", "malloc failed: %d bytes", bufferSize);
  return false;  // Handle allocation failure
}

// Use buffer
processData(buffer, bufferSize);

// Free immediately after use
free(buffer);
buffer = nullptr;
```

**Rules**:
- **ALWAYS check for nullptr** after malloc
- **Free immediately** after use (don't hold across multiple operations)
- **Set to nullptr** after free (avoid use-after-free)
- **Document size**: Comment why stack allocation was rejected

**Examples in codebase**:
- Bitmap rendering: [GfxRenderer.cpp](lib/GfxRenderer/GfxRenderer.cpp) (storeBwBuffer)
- OTA update buffer: [OtaUpdater.cpp](src/network/OtaUpdater.cpp)

---

## UI and Orientation Guidelines

### Orientation-Aware Logic
* No Hardcoding: Never assume 800 or 480. Use renderer.getScreenWidth() and renderer.getScreenHeight().
* Viewable Area: Use renderer.getOrientedViewableTRBL() to stay within physical bezel margins.

### Logical Button Mapping

**Source**: [src/MappedInputManager.cpp](src/MappedInputManager.cpp)

Constraint: Physical button positions are fixed on hardware, but their logical functions change based on user settings and screen orientation.

**Button Categories**:
1. **Physical Fixed** (Up/Down side buttons):
   - `Button::Up` → Always `HalGPIO::BTN_UP`
   - `Button::Down` → Always `HalGPIO::BTN_DOWN`

2. **User Remappable** (Front buttons):
   - `Button::Back` → Maps to `SETTINGS.frontButtonBack` (hardware index)
   - `Button::Confirm` → Maps to `SETTINGS.frontButtonConfirm`
   - `Button::Left` → Maps to `SETTINGS.frontButtonLeft`
   - `Button::Right` → Maps to `SETTINGS.frontButtonRight`

**Implementation**:
- Activities use **logical buttons** (e.g., `Button::Confirm`)
- `MappedInputManager` translates to **physical hardware buttons**
- User can remap front buttons in settings
- Orientation changes handled separately by renderer coordinate transforms

**Rule**: Always use `MappedInputManager::Button::*` enums, never raw `HalGPIO::BTN_*` indices (except in ButtonRemapActivity).

---

## Common Patterns

### Singleton Access
**Available Singletons**:
```cpp
#define SETTINGS MyneSettings::getInstance()  // User settings
#define APP_STATE MyneState::getInstance()    // Runtime state
#define GUI UITheme::getInstance().getTheme() // MyneUI instance
#define Storage HalStorage::getInstance()     // SD card I/O
#define I18N I18n::getInstance()              // Internationalization
```

### Activity Lifecycle and Memory Management

**Source**: [src/main.cpp](src/main.cpp)

**CRITICAL**: Activities are **heap-allocated** and **deleted on exit**.

```cpp
// main.cpp navigation pattern
void exitActivity() {
  if (currentActivity) {
    currentActivity->onExit();
    delete currentActivity;  // Activity deleted here!
    currentActivity = nullptr;
  }
}

void enterNewActivity(Activity* activity) {
  currentActivity = activity;  // Heap-allocated activity
  currentActivity->onEnter();
}
```

**Memory Implications**:
- Activity navigation = `delete` old activity + `new` create next activity
- Any memory allocated in `onEnter()` MUST be freed in `onExit()`
- FreeRTOS tasks MUST be deleted in `onExit()` before activity destruction
- Member `FsFile` handles MUST be closed in `onExit()` (local `FsFile` variables auto-close via destructor)

**Activity Pattern**:
```cpp
void onEnter()  { Activity::onEnter(); /* alloc: buffer, tasks */ render(); }
void loop()     { mappedInput.update(); /* handle input */ }
void onExit()   { /* free: vTaskDelete, free buffer, close member FsFiles */ Activity::onExit(); }
```

**Critical**: Free resources in reverse order. Delete tasks BEFORE activity destruction.

### FreeRTOS Task Guidelines

**Source**: [src/activities/util/KeyboardEntryActivity.cpp](src/activities/util/KeyboardEntryActivity.cpp)

**Pattern**: See Activity Lifecycle above. `xTaskCreate(&taskTrampoline, "Name", stackSize, this, 1, &handle)`

**Stack Sizing** (in BYTES, not words):
- **2048**: Simple rendering (most activities)
- **4096**: Network operations
- Monitor: `uxTaskGetStackHighWaterMark()` if crashes

**Rules**: Always `vTaskDelete()` in `onExit()` before destruction. Use mutex if shared state.

### Global Font Loading

**Source**: [src/main.cpp](src/main.cpp)

**Fonts loaded at firmware startup** (UI-only, no reader fonts):
- Ubuntu UI fonts: 10, 12pt (2 styles each: regular, bold)
- Noto Sans small: 8pt (for compact UI elements)

**Compilation Flag**:
```cpp
#ifndef OMIT_FONTS
  // Fonts loaded here
#endif
```

**Implications**:
- Fonts stored in **Flash** (marked as `static const` in `lib/EpdFont/builtinFonts/`)
- Font rendering data cached in **DRAM** when first used
- Font IDs defined in [src/fontIds.h](src/fontIds.h)

**Usage**:
```cpp
#include "fontIds.h"

renderer.insertFont(FONT_UI_MEDIUM, ui12FontFamily);
renderer.drawText(FONT_UI_MEDIUM, x, y, "Hello", true);
```

---

## Testing and Debugging

### Build Commands

**Via CLI**:
```bash
# Build firmware (default environment)
pio run

# Build and upload to device
pio run -t upload

# Build specific environment
pio run -e gh_release

# Clean build artifacts
pio run -t clean

# Upload filesystem data (if using SPIFFS/LittleFS)
pio run -t uploadfs
```

**Via VS Code**:
* Use PlatformIO toolbar: Build (✓), Upload (→), Clean (🗑️)
* Or Command Palette: `PlatformIO: Build`, `PlatformIO: Upload`, etc.

### Monitoring and Debugging

```bash
# Enhanced monitor with color/logging (recommended)
python3 scripts/debugging_monitor.py

# Standard PlatformIO monitor
pio device monitor

# Combined upload + monitor
pio run -t upload && pio device monitor
```

**Via VS Code**: Click Monitor (🔌) button in PlatformIO toolbar

### Code Quality

```bash
# Static analysis (cppcheck)
pio check

# Format code (clang-format) - Windows Git Bash
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Format code (clang-format) - Linux
clang-format -i src/**/*.cpp src/**/*.h
```

### Debugging Crashes

**Common Crash Causes**:

1. **Out of Memory** (Most common):
   ```cpp
   LOG_DBG("MEM", "Free heap: %d bytes", ESP.getFreeHeap());
   ```
   - Monitor heap usage throughout activity lifecycle
   - Check if large allocations (>10KB) occur before crash
   - Verify buffers are freed in `onExit()`

2. **Stack Overflow**:
   ```cpp
   LOG_DBG("TASK", "Stack high water: %d", uxTaskGetStackHighWaterMark(taskHandle));
   ```
   - Occurs during deep recursion or large local variables
   - Increase task stack size in `xTaskCreate()` (2048 → 4096)
   - Move large buffers to heap with malloc

3. **Use-After-Free**:
   - Activity deleted but task still running
   - Always `vTaskDelete()` in `onExit()` BEFORE activity destruction
   - Set pointers to `nullptr` after `free()`

4. **Watchdog Timeout**:
   - Loop/task blocked for >5 seconds
   - Add `vTaskDelay(1)` in tight loops
   - Check for blocking I/O operations

**Verification Steps**:
1. Check serial output for stack traces
2. Monitor heap with `ESP.getFreeHeap()` before/after operations
3. Verify task deletion with task list (`vTaskList()`)
4. Test with `LOG_LEVEL=2` (debug logging enabled)

---

## Settings Architecture

### Settings Categories (3 total)

**Source**: [src/activities/settings/SettingsActivity.cpp](src/activities/settings/SettingsActivity.cpp)

1. **Display** — sleep screen mode/cover/filter, hide battery percentage, refresh frequency, sunlight fading fix
2. **Controls** — remap front buttons
3. **System** — time to sleep, show hidden files, timezone offset, Wi-Fi networks, check for updates, SD firmware update, language

`MyneSettings.h` still defines several reader-era enums and fields (`FONT_FAMILY`, `FONT_SIZE`, `LINE_COMPRESSION`, `PARAGRAPH_ALIGNMENT`, `STATUS_BAR_TITLE`, etc.). These are loaded/saved for backward compatibility with existing settings files but are not read anywhere in `src/`/`lib/` — treat them as legacy/dead and do not build new features on top of them.

### MyneState

**Source**: [src/MyneState.h](src/MyneState.h)

Minimal runtime state — only the `recentSleepImages` circular buffer (plus `recentSleepPos`/`recentSleepFill`) remains, used for sleep-screen cover rotation. All book and reading-session data lives in `lib/DataStore/` (see [docs/book-catalog-format.md](docs/book-catalog-format.md)), not in `MyneState`.

### ActivityManager

**Source**: [src/activities/ActivityManager.h](src/activities/ActivityManager.h)

Available navigation methods:
- `goHome()` — HomeActivity (hero "Last Read" card + 2×2 icon grid)
- `goToFileBrowser(path)` — FileBrowserActivity
- `goToFileTransfer()` — MyneWebServerActivity (Wi-Fi file transfer + dashboard API)
- `goToSettings()` — SettingsActivity
- `goToPhysicalBooks()` — LetterPickerActivity (browse the catalog by letter/collection)
- `goToPhysicalBookDetail(PhysicalBook)` — PhysicalBookDetailActivity
- `goToBookReadings(PhysicalBook)` — BookReadingsActivity
- `goToReadingStats()` — ReadingStatsActivity
- `goToLastRead()` — opens the most recently read book
- `goToSleep()` — SleepActivity
- `goToBoot()` — BootActivity
- `goToFullScreenMessage()` — FullScreenMessageActivity
- `goToCrashReport()` — CrashReportActivity

---

## Git Workflow and Repository Awareness

### Repository Detection Protocol

**CRITICAL**: ALWAYS verify repository context before git operations. This could be:
- A **fork** with `origin` pointing to personal repo, `upstream` to main repo
- A **direct clone** with `origin` pointing to main repo
- Multiple collaborator remotes

**Verification Commands** (run at session start):
```bash
# Check current branch
git branch --show-current

# Check all remotes
git remote -v

# Identify main branch name (could be 'main' or 'master')
git symbolic-ref refs/remotes/origin/HEAD 2>/dev/null | sed 's@^refs/remotes/origin/@@'

# Check working tree status
git status --short
```

**Example Output** (Myne is itself a personal fork of CrossPoint):
```text
origin      https://github.com/MateusGX/myne.git (fetch/push)
upstream    https://github.com/crosspoint-reader/crosspoint-reader.git (fetch/push)
```

### Git Operation Rules

1. **Never assume branch names**:
   ```bash
   # Bad: git push origin main
   # Good: git push origin $(git branch --show-current)
   ```

2. **Never assume remote names or write permissions**:
   - **Forked repos**: Push to `origin` (your fork), submit PR to `upstream`
   - **Direct contributors**: May push feature branches to `upstream`
   - **Always ask**: "Should I push to origin or create a PR?"

3. **Check for upstream changes before starting work**:
   ```bash
   # Sync fork with upstream (if applicable)
   git fetch upstream
   git merge upstream/main  # or upstream/master
   ```

4. **Use explicit remote and branch names**:
   ```bash
   # Check remotes first
   git remote -v

   # Use explicit syntax
   git push <remote> <branch>
   ```

### Branch Naming Convention

**For feature/fix branches**:
```text
feature/<short-description>       # New features
fix/<issue-number>-<description>  # Bug fixes
refactor/<component-name>         # Code refactoring
docs/<topic>                      # Documentation updates
```

**Examples**:
- `feature/sd-download-progress`
- `fix/123-orientation-crash`
- `refactor/hal-storage`

### Commit Message Format

**Pattern**:
```text
<type>: <short summary (50 chars max)>

<optional detailed description>

```

**Types**: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`

**Example**:
```text
feat: add real-time SD download progress bar

Implements progress tracking for file downloads using
MyneUI progress bar component with heap-safe updates.

Tested in all 4 orientations with 5MB+ files.
```

### When to Commit

**DO commit when**:
- User explicitly requests: "commit these changes"
- Feature is complete and tested on device
- Bug fix is verified working
- Refactoring preserves all functionality
- All tests pass (`pio run` succeeds)

**DO NOT commit when**:
- Changes are untested on actual hardware
- Build fails or has warnings
- Experimenting or debugging in progress
- User hasn't explicitly requested commit
- Files excluded by `.gitignore` would be included — always run `git status` and cross-check against `.gitignore` before staging (e.g., `*.generated.h`, `.pio/`, `compile_commands.json`, `platformio.local.ini`)

**Rule**: **If uncertain, ASK before committing.**

---

## Generated Files and Build Artifacts

### Files Generated by Build Scripts

**NEVER manually edit these files** - they are regenerated automatically:

1. **HTML Headers** (generated by `scripts/build_html.py`):
   - `src/network/html/*.generated.h`
   - **Source**: HTML templates in `data/html/` directory
   - **Triggered**: During PlatformIO `pre:` build step
   - **To modify**: Edit source HTML in `data/html/`, not generated headers

2. **I18n Headers** (generated by `scripts/gen_i18n.py`):
   - `lib/I18n/I18nKeys.h`, `lib/I18n/I18nStrings.h`, `lib/I18n/I18nStrings.cpp`
   - **Source**: YAML translation files in `lib/I18n/translations/` (one per language)
   - **To modify**: Edit source YAML files, then run `python scripts/gen_i18n.py lib/I18n/translations lib/I18n/`
   - **Commit**: Source YAML files only. All three generated files (`I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp`) are in `.gitignore` and regenerated at build time.

3. **Icon Headers** (generated by `scripts/build_icons.py`):
   - `src/components/icons/*.h`
   - **Source**: SVG files in `src/components/icons/src/`
   - **To modify**: Edit or add SVG, then run `python scripts/build_icons.py`
   - **Commit**: Both the SVG source AND the generated `.h` file (icons are not regenerated at build time)

4. **Build Artifacts** (in `.gitignore`):
   - `.pio/` - PlatformIO build output
   - `build/` - Compiled binaries
   - `*.generated.h` - Any auto-generated headers
   - `compile_commands.json` - LSP/IDE metadata

### Modifying Generated Content Workflow

**To change HTML pages**:
1. Edit source: `data/html/<pagename>.html`
2. Build: `pio run` (auto-triggers `scripts/build_html.py`)
3. Generated headers update: `src/network/html/<pagename>Html.generated.h`
4. **Commit ONLY** source HTML, NOT generated `.generated.h` files

**To add/modify translations (i18n)**:
1. Edit or add YAML file: `lib/I18n/translations/<language>.yaml`
   - Each file must contain: `_language_name`, `_language_code`, `_order`, and `STR_*` keys
   - English (`english.yaml`) is the reference; missing keys in other languages fall back to English
2. Run generator: `python scripts/gen_i18n.py lib/I18n/translations lib/I18n/`
3. Generated files update: `I18nKeys.h`, `I18nStrings.h`, `I18nStrings.cpp`
4. **Commit** source YAML files only. All three generated files are in `.gitignore` and regenerated at build time.

**To use translated strings in code**:
```cpp
#include <I18n.h>
// Use tr() macro with StrId enum (defined in generated I18nKeys.h)
renderer.drawText(FONT_UI, x, y, tr(STR_LOADING), true);
```

**To add icons**:
1. Place source SVG in `src/components/icons/src/` — encode size in name (e.g., `myicon64.svg`)
2. Run: `python scripts/build_icons.py src/components/icons/src/myicon64.svg`
3. Generated header: `src/components/icons/myicon64.h`
4. Add to `UIIcon` enum in `src/components/icons/Icons.h` and handle in `iconForName()`
5. **Commit** both SVG and generated `.h` file

---

## Local Development Configuration

### platformio.local.ini (Personal Overrides)

**Purpose**: Personal development settings that should NEVER be committed.

**Use Cases**:
- Serial port configuration (varies by machine)
- Debug flags for specific testing
- Local build optimizations
- Developer-specific paths

**Example** `platformio.local.ini`:
```ini
# platformio.local.ini (gitignored)
[env:default]
upload_port = COM7              # Windows: COMx, Linux: /dev/ttyUSBx
monitor_port = COM7

build_flags =
  ${base.build_flags}
  -DMY_DEBUG_FLAG=1             # Personal debug flags
  -DTEST_FEATURE_ENABLED=1
```

**Configuration Hierarchy**:
1. `platformio.ini` - **Committed**, shared project settings
2. `platformio.local.ini` - **Gitignored**, personal overrides
3. Local file extends/overrides base config

**Rules**:
- **NEVER commit** `platformio.local.ini`
- **NEVER put** personal info (serial ports, credentials) in main `platformio.ini`
- Use `${base.build_flags}` to extend (not replace) base flags

---

## Testing and Verification Workflow

### Testing Checklist

**AI agent scope** (what you CAN verify):
1. ✅ **Build**: `pio run -t clean && pio run` (0 errors/warnings)
2. ✅ **Quality**: `pio check` + `find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i`
3. ✅ **Format**: Commit messages (`feat:`/`fix:`), no `.gitignore`-excluded files staged (e.g., `*.generated.h`, `.pio/`, `platformio.local.ini`)
4. ✅ **CI**: Fix GitHub Actions failures before review
5. ✅ **Code review**: Ensure orientation-aware logic is correct in all 4 modes by inspecting switch/case coverage

**Human tester scope** (flag these for the user):
6. 🔲 **Device**: Test on hardware
7. 🔲 **Orientations**: Verify all 4 modes (Portrait/Inverted/Landscape CW/CCW)
8. 🔲 **Heap**: `ESP.getFreeHeap()` > 50KB, no leaks

### CI/CD Pipeline Awareness

**GitHub Actions** run automatically on pull requests:

| Workflow | File | Purpose |
|----------|------|---------|
| Build Check | `.github/workflows/ci.yml` | Verifies code compiles |
| Format Check | `.github/workflows/pr-formatting-check.yml` | Validates clang-format |
| Release Build | `.github/workflows/release.yml` | Production releases |
| RC Build | `.github/workflows/release_candidate.yml` | Release candidates |

**Rules**:
- **Fix CI failures BEFORE** requesting review
- CI runs on: Push to PR, PR updates
- Format check fails → Run clang-format locally
- Build check fails → Fix compile errors

---

## Serial Monitoring and Live Debugging

### Serial Monitor Options

1. **Enhanced**: `python3 scripts/debugging_monitor.py` (color-coded, recommended)
2. **Standard**: `pio device monitor` (basic, no colors)
3. **VS Code**: Monitor (🔌) button (IDE-integrated)

### Live Debugging Patterns

**Heap**: `LOG_DBG("MEM", "Free: %d", ESP.getFreeHeap());` (every 5s in loop)
**Stack**: `uxTaskGetStackHighWaterMark(nullptr)` (< 512 bytes → increase stack)
**Flush**: `logSerial.flush();` (force output before crash)

**Port Detection**: Windows: `mode` | Linux: `ls /dev/ttyUSB* /dev/ttyACM*` or `dmesg | grep tty`

---

Philosophy: We are building a dedicated physical-book library and reading-session tracker, not a Swiss Army knife. If a feature adds RAM pressure without significantly improving book cataloging, reading tracking, file transfer, or the companion dashboard, it is Out of Scope.