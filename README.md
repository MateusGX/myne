# Myne

Myne is a personal fork of [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader), repurposed away from EPUB reading and into a dedicated **physical book library and reading-session tracker**. Catalog the books on your shelf, log reading sessions against them, see your reading stats, and manage everything from the device or from a companion web dashboard over Wi-Fi.

**Now running on:** ESP32C3-based Xteink [X4](https://www.xteink.com/products/xteink-x4).

> Myne's UI and layouts target the X4 exclusively. Any X3-related code paths inherited from upstream CrossPoint are legacy, untested, and not maintained.

## What can Myne do?

- **Physical book catalog**: add and edit entries for the books you own (title, author, location, volume), browse the catalog by first letter or by collection.

- **Reading session tracking**: log reading sessions per book, track status (e.g. to-read / reading / finished), and review session history.

- **Reading stats**: aggregated daily/monthly reading statistics.

- **Library workflow**: SD card folder browser, hidden-file toggle, long-press delete.

- **Wireless workflows**:

  - File transfer web UI with WebSocket fast uploads
  - WebDAV handler
  - Web settings UI/API (edit device settings from a browser)
  - AP mode (hotspot) and STA mode (join existing WiFi), both with QR helpers
  - OTA update checks and installs from GitHub releases

- **Companion dashboard**: a React web app (`dashboard/`) for managing your book catalog, collections, reading sessions, SD card files, and device settings from a browser.

- **Customization**: sleep screen modes, front/side button remapping, status bar controls, power-button behavior, refresh cadence, and more.

- **Localization**: English and Portuguese, with more languages welcome via [translations](./docs/translators.md).

---

## USB-locked devices (Xteink Unlocker)

Some Xteink units purchased from third-party stores (e.g. AliExpress) ship with USB flashing locked from the factory.
If your device is locked, you will need to use the **Xteink Unlocker** tool available at
https://crosspointreader.com/#unlock-tool before you can flash custom firmware.

**You do not need this tool if you bought your device directly from xteink.com.** Those units are not locked.

**Not sure if your device is locked?** Power it on, connect the USB-C cable, and try flashing via the web flasher first (see
[Install firmware](#install-firmware) below). If the browser's serial device picker does not show your device, try a different
USB port or browser before assuming the device is locked. Only reach for the unlocker if the device still doesn't appear.

> ### ⚠️ WARNING: READ THIS BEFORE USING THE UNLOCKER ⚠️
>
> **The only officially supported firmwares in the unlock tool are CrossPoint and CrossInk. Myne is not on that list.**
>
> Flashing any firmware not on that list onto a USB-locked device may **permanently brick the device** or leave it **permanently
> stuck on that firmware with no recovery path other than OTA**. Myne retains OTA update support (checking
> [MateusGX/myne](https://github.com/MateusGX/myne) releases), so OTA-based recovery should remain possible — but if
> you are unsure, flash official CrossPoint first and confirm OTA works before switching a USB-locked device to Myne.

## Install firmware

### Web installer (recommended)

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Download a `firmware.bin` from [Releases](https://github.com/MateusGX/myne/releases), a local build, or a continuous integration artifact.
3. Go to https://crosspointreader.com/#flash-tools, select device (X4), click "Custom .bin" and upload the `firmware.bin`.

### Command line

1. Install [`esptool`](https://github.com/espressif/esptool):

```bash
pip install esptool
```

2. Download `firmware.bin` from the [releases page](https://github.com/MateusGX/myne/releases).
3. Connect your device via USB-C.
4. Find the device port. On Linux, run `dmesg` after connecting. On macOS:

```bash
log stream --predicate 'subsystem == "com.apple.iokit"' --info
```

5. Flash:

```bash
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

Adjust `/dev/ttyACM0` to match your system.

### Manual

See [Development quick start](#development-quick-start) below.

---

## Documentation

- [User Guide](./USER_GUIDE.md)
- [Web server usage](./docs/webserver.md)
- [Web server endpoints](./docs/webserver-endpoints.md)
- [Book catalog & reading-log format](./docs/book-catalog-format.md)
- [Project scope](./SCOPE.md)
- [Contributing docs](./docs/contributing/README.md)

---

## Development quick start

### Prerequisites

- [pioarduino](https://github.com/pioarduino/pioarduino) or VS Code + pioarduino plugin
- Python 3.8+
- `clang-format` 21
- USB-C cable supporting data transfer

### Setup

```bash
git clone --recursive https://github.com/MateusGX/myne
cd myne

# if cloned without --recursive:
git submodule update --init --recursive
```

### Build / flash / monitor

```bash
pio run --target upload
```

### Contributor pre-PR checks

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
```

### Debugging

After flashing the new features, it's recommended to capture detailed logs from the serial port.

First, make sure all required Python packages are installed:

```python
python3 -m pip install pyserial colorama matplotlib
```

After that run the script:

```sh
# For Linux
# This was tested on Debian and should work on most Linux systems.
python3 scripts/debugging_monitor.py

# For macOS
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```

Minor adjustments may be required for Windows.

---

## Internals

Myne is pretty aggressive about caching data down to the SD card to minimise RAM usage. The ESP32-C3 only has ~380KB of usable RAM, so we have to be careful. A lot of the decisions made in the design of the firmware were based on this constraint.

### On-device storage

Book catalog entries, collections, reading sessions, and stats are stored on the SD card under `/.myne/`:

```text
.myne/
├── books/            # one JSON file per physical book (id.json)
├── catalog/          # alphabetical (A-Z, #) and per-collection NDJSON indexes + idx.bin
├── collections.ndjson # collection ID registry
├── notes/            # per-book and per-collection notes
│   ├── books/        # notes/books/{bookId}.note
│   └── collections/  # notes/collections/{id8}.note
├── readings/         # reading session records (one JSON file per book)
├── readings-sum/     # 17-byte binary reading summaries for fast lookups
└── stats-cache/      # 40-byte binary stats cache for reading stats
```

For the full on-disk layout, see [docs/book-catalog-format.md](./docs/book-catalog-format.md).

---

## Contributing

Contributions are welcome. If you're new to the codebase, start with the [contributing docs](./docs/contributing/README.md). For things to work on, check the [issues](https://github.com/MateusGX/myne/issues) — leave a comment before starting so we don't duplicate effort.

Please be respectful and patient. For governance and community expectations, see [GOVERNANCE.md](./GOVERNANCE.md).

---

## Related projects

Myne is one of many community forks of [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader), an open-source e-reader firmware. If you're looking for an EPUB e-reader rather than a physical-book tracker, CrossPoint and its other forks are a better fit:

- [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) — the upstream e-reader firmware Myne is forked from.

- [CrossInk](https://github.com/uxjulia/CrossInk) — Typography and reading tracking: Bionic Reading (bolds word stems to create fixation points), guide dots between words, improved paragraph indents, and replaces the default fonts with ChareInk/Lexend/Bitter.

- [papyrix-reader](https://github.com/bigbag/papyrix-reader) — Adds FB2 and MD format support. Actively maintained with Arabic script support. Custom themes via SD card.

- [crosspet](https://github.com/trilwu/crosspet) — A Vietnamese fork that adds a Tamagotchi-style virtual chicken that grows based on your reading milestones (pages read, streaks, care). Also: Flashcards, Weather, Pomodoro timer, and mini-games.

- [crosspoint-reader (jpirnay)](https://github.com/jpirnay/crosspoint-reader) — Faster integration of functionality. Tracks upstream PRs and integrates the good ones ahead of the official merge.

- [crosspoint-reader-cjk](https://github.com/aBER0724/crosspoint-reader-cjk) — Purpose-built for Chinese, Japanese, and Korean reading.

- [inx](https://github.com/obijuankenobiii/inx) — Completely reimagines the user interface with tabbed navigation.

- ~~[PlusPoint](https://github.com/ngxson/pluspoint-reader) — custom JS apps support.~~ (Unmaintained)

- [crosspoint-reader-papers3](https://github.com/juicecultus/crosspoint-reader-papers3) — Crosspoint port for M5Stack Paper S3.

Want to build your own device? Be sure to check out the [de-link](https://github.com/iandchasse/de-link) project.

---

Myne is a personal, unofficial fork — **not affiliated with Xteink, any device manufacturer, or the CrossPoint project**.

Built on top of [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader). Huge shoutout to [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader), which inspired CrossPoint.
