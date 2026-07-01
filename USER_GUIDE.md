# Myne User Guide

Myne turns your Xteink X4 into a pocket library catalog and reading log. It does **not** display book
content — instead, it helps you keep track of the physical books you own, log your reading sessions
against them, and see your reading stats over time. This guide covers the on-device interface, the
companion web dashboard, and device settings.

---

## 1. Hardware & Controls

The X4 has a row of front buttons along the bottom edge, two side buttons, and a power button.

| Logical Button | Default Physical Location | Notes |
|---|---|---|
| **Back** | Front, leftmost | Remappable |
| **Confirm** | Front, second from left | Remappable |
| **Left** | Front, third from left | Remappable |
| **Right** | Front, rightmost | Remappable |
| **Up** | Side (top) | Fixed — also used for paging/scrolling up |
| **Down** | Side (bottom) | Fixed — also used for paging/scrolling down |
| **Power** | Side/top edge | Fixed |

The four front buttons (**Back**, **Confirm**, **Left**, **Right**) can be remapped to any physical
button via **Settings > Controls > Remap Front Buttons** — useful if you're left-handed or prefer a
different layout. The **Up**/**Down** side buttons are always fixed and are used throughout the UI for
scrolling lists and flipping between pages of items.

Throughout this guide:
- **Confirm** opens/selects the highlighted item.
- **Back** returns to the previous screen.
- **Up**/**Down** move the selection or page through long lists.
- **Left**/**Right** are used for secondary actions (creating, deleting, adjusting values) depending on
  the screen.

---

## 2. Powering On

Press the **Power** button to wake the device from sleep, or to turn it on. Myne boots directly to the
**Home Screen**.

On first boot — or any time the on-device book catalog needs to be rebuilt (for example, after editing
books or collections from the dashboard) — Myne automatically shows a **"Rebuilding catalog…"** screen
with a progress bar and a running count of books processed. This happens automatically; there's nothing
to trigger here. If it succeeds, the device returns to the Home Screen automatically after a moment (or
on any keypress). If it fails, press any button to continue to the Home Screen — try the rebuild again
later, e.g. by re-saving from the dashboard.

---

## 3. Home Screen

The Home Screen has two parts:

- **Last Read** (top): a hero card showing the book you most recently logged a reading session for —
  title, author, current progress (page or chapter number), the date of your last session, and its
  status (*Reading*, *Paused*, *Finished*, *Dropped*, or *Want to Read*). If you haven't logged any
  reading sessions yet, this card shows a placeholder instead.
- **A 2×2 grid of tiles** (below): **Physical Books**, **Reading Stats**, **Network**, and **Settings**,
  each with an icon and short description.

Use **Up**/**Down** or **Left**/**Right** to move between the Last Read card and the four tiles, and
press **Confirm** to open the highlighted item. **Back** does nothing on the Home Screen.

---

## 4. Physical Books

This is your library catalog. Books are organized alphabetically by title (A–Z, plus a `#` section for
titles starting with a digit or symbol), and can optionally be grouped into **collections** (e.g. a
series).

### 4.1 Letter Picker

Opening **Physical Books** from the Home Screen shows the **Physical Books / A–Z** list — one row per
letter (and `#`), each showing how many catalog entries start with it. Use **Up**/**Down** or
**Left**/**Right** to scroll, **Confirm** to open a letter. If your catalog is empty, this screen shows
an empty-state message instead. **Back** returns to the Home Screen.

### 4.2 A Letter's Entries

Opening a letter shows a paginated list of every book and collection whose title starts with that
letter. Each row shows its position, title, author, and a small badge if it belongs to a collection.
**Up**/**Down** or **Left**/**Right** scroll/page through the list, **Confirm** opens the highlighted
book or collection, and **Back** returns to the letter picker.

### 4.3 Collections

Opening a collection shows its name, an optional note (if you've added one from the dashboard), and a
paginated list of the books in it. **Confirm** opens a book's detail screen; **Back** returns to the
letter view. If a collection has no books in it, **Confirm** simply returns you to the previous screen.

### 4.4 Book Detail

The book detail screen shows everything Myne knows about a single book:

- **Collection** — which collection (if any) the book belongs to
- **Location** — a free-text field for where the physical book lives (e.g. "Shelf 2, Box A")
- **Note** — the note you've added for the book, if any (wraps up to 3 lines)
- **Reading summary** — the book's current status and your last logged position/date, or "No readings
  yet" if you haven't logged any sessions

Fields that haven't been filled in show as "–". This screen is read-only on the device — to add or edit
book metadata (title, author, volume, collection, location, note), use the **dashboard** (see
[File Transfer & Dashboard](#6-file-transfer--dashboard)).

From here:
- **Confirm** opens the book's **Readings** list (reading session history)
- **Right** opens the book's **Reading Stats** (per-book stats)
- **Back** returns to the book list

### 4.5 Reading Sessions

The **Readings** screen lists every reading session you've logged for the book, most recent first —
each row shows the session number, status, last position (page or chapter), and date.

- **Up**/**Down** or **Left**/**Right** — scroll/page through sessions
- **Confirm** — edit the highlighted session
- **Left** — log a new session
- **Right** — delete the highlighted session (only available if there's at least one session; if the
  list is empty, **Left** creates the first one)
- **Back** — return to the book detail screen

When deleting, the screen shows **Back = Cancel** / **Confirm = Delete** hints — confirm carefully, as
deletion is permanent.

### 4.6 Logging or Editing a Reading Session

This screen has five fields, arranged vertically. **Up**/**Down** move between fields (wrapping around
all five); **Left**/**Right** adjust the value of the currently selected field. The selected field is
highlighted with a dark bar and light background.

1. **Status** — *Want to Read*, *Reading*, *Paused*, *Finished*, or *Dropped*
2. **Date** — shown compactly (e.g. "5 Jun '26 · 21:30"). While this field is selected, press
   **Power** to cycle which part of the date/time you're editing (year → month → day → hour → minute),
   then use **Left**/**Right** to change it
3. **Position** — a number representing your current page or chapter; **Up**/**Down** increment or
   decrement it
4. **Sync Time** — sets the session's date/time from the network. Shows *None* / *Syncing…* /
   *Time synced* / *Sync failed*; press **Right** to trigger a sync (requires Wi-Fi — see
   [File Transfer & Dashboard](#6-file-transfer--dashboard))
5. **Type** — whether **Position** tracks **Page** or **Chapter** numbers

Press **Confirm** to save the session (this also updates the book's and your library's reading stats).
**Back** exits the screen, saving any changes you made first.

### 4.7 Per-Book Reading Stats

Reached via **Right** from the Book Detail screen. **Up**/**Down** cycle between two views (shown as
dots at the bottom):

1. **Summary** — total number of reading sessions, the date range of your first to most recent session
   for this book, and a bar chart of monthly activity (most recent month first)
2. **Timeline** — an expanded version of the monthly chart with month/year labels

**Back** returns to the book detail screen.

---

## 5. Reading Stats (Library-Wide)

Opening **Reading Stats** from the Home Screen shows your overall reading activity across your whole
library, across three views. Use **Up**/**Down** or **Left**/**Right** to cycle between them (a page
indicator like "1/3" shows your position):

1. **Overview** — total books, total reading sessions, and a breakdown by status (Want to Read /
   Reading / Paused / Finished / Dropped) and by tracking type (Pages vs. Chapters)
2. **Month** — a calendar heatmap showing how many sessions you logged on each day of the month;
   **Left**/**Right** move to the previous/next month
3. **Year** — a bar chart with one bar per month showing session counts across the year;
   **Left**/**Right** move to the previous/next year

**Back** returns to the Home Screen.

---

## 6. File Transfer & Dashboard

Selecting **Network** from the Home Screen lets you connect the device to Wi-Fi (joining an existing
network, or starting its own hotspot/AP mode) — both modes display a QR code to make connecting from
your phone or computer easier.

Once connected, Myne exposes:

- A **file transfer web UI** for browsing, uploading (including fast WebSocket uploads), downloading,
  renaming, moving, and deleting files on the SD card from any browser on the same network
- A **WebDAV** endpoint, so you can mount the SD card as a network drive in supporting apps/OS file
  managers
- A **web settings UI**, mirroring the on-device Settings menu, for changing device settings remotely

### The companion dashboard

The `dashboard/` web app is the primary way to manage your book catalog day-to-day. Run it on your
computer (see its [README](./dashboard/README.md) for setup) and point it at your device's IP address.
From the dashboard you can:

- Add, edit, and delete books — title, author, volume, collection, location, and note
- Create and rename collections, and add a per-collection note
- View and edit reading sessions for any book
- Browse, upload, download, rename, move, and delete files on the SD card
- View device status (battery, Wi-Fi, free space) and change settings
- Flash new firmware to the device (OTA)

### OTA updates

From **Settings > System > Check for Updates**, the device checks
[github.com/MateusGX/myne/releases](https://github.com/MateusGX/myne/releases) for a newer firmware
version and offers to download and install it over Wi-Fi.

---

## 7. File Browser

The SD card file browser lets you look at the raw contents of your SD card — useful for managing sleep
screen images, firmware files, or anything else stored on the card outside the book catalog.

- The header shows the current folder name (or "SD Card" at the root); the full path is shown at the
  bottom, truncated from the left if it's long
- **Up**/**Down** or **Left**/**Right** — scroll/page through entries (folders show a trailing `/`)
- **Confirm** (short press) — open a folder, or select a file
- **Confirm** (press and hold) — delete the highlighted file or folder, with a confirmation prompt
- **Back** (short press) — go up one level (from the root, returns to the Home Screen)
- **Back** (press and hold) — jump straight back to the root

Hidden files (names starting with `.`) are hidden unless **Settings > System > Show Hidden Files** is
enabled.

---

## 8. Settings

Settings are organized into three categories. Open **Settings** from the Home Screen, then **Up**/**Down**
to move between categories and items, **Confirm** to open/toggle, **Left**/**Right** to cycle option
values, **Back** to return to the Home Screen.

### 8.1 Display

| Setting | Options |
|---|---|
| Sleep Screen | Dark, Light, Custom, Cover, Blank, Cover + Custom |
| Sleep Screen Cover Mode *(only if Sleep Screen is Cover or Cover + Custom)* | Fit, Crop |
| Sleep Screen Cover Filter *(only if Sleep Screen is Cover or Cover + Custom)* | None, Black & White, Inverted |
| Hide Battery % | Never, In Reader, Always |
| Refresh Frequency | Every 1 / 5 / 10 / 15 / 30 pages |
| Sunlight Fading Fix | On / Off |

See [Sleep Screen](#9-sleep-screen) below for details on each sleep screen mode.

### 8.2 Controls

| Setting | What it does |
|---|---|
| Remap Front Buttons | Walks you through pressing a physical button for each of the **Back**, **Confirm**, **Left**, and **Right** roles, in order. While remapping: press **Up** to reset to the default layout, or **Down** to cancel without saving. The same physical button can't be assigned to two roles. |

### 8.3 System

| Setting | Options |
|---|---|
| Time to Sleep | 1 / 5 / 10 / 15 / 30 minutes of inactivity |
| Show Hidden Files | On / Off — controls visibility of dot-files in the File Browser |
| Timezone | UTC-12 to UTC+14 |
| WiFi Networks | Opens the Wi-Fi network list to scan/select/forget networks |
| Check for Updates | Checks for and installs a newer firmware build over OTA |
| SD Card Firmware Update | Installs a firmware `.bin` file from the SD card |
| Language | Switches the UI language (currently English and Portuguese) |

---

## 9. Sleep Screen

When the device sleeps (after the **Time to Sleep** timeout, or when you press **Power**), it shows a
full-screen image based on **Settings > Display > Sleep Screen**:

- **Dark** — a dark logo on a light background
- **Light** — a light logo on a dark (inverted) background
- **Custom** — a random image from your SD card (see below)
- **Cover** — the cover image of the book from your most recent reading session (i.e. your "Last Read"
  book on the Home Screen)
- **Blank** — a blank screen
- **Cover + Custom** — the last-read book's cover, falling back to a random custom image if no cover is
  available

When **Cover** or **Cover + Custom** is selected, two extra settings appear:
- **Sleep Screen Cover Mode** — **Fit** (scale the cover to fit without cropping) or **Crop** (scale and
  crop to fill the screen)
- **Sleep Screen Cover Filter** — **None** (grayscale as-is), **Black & White**, or **Inverted**

For **Custom** images, Myne first looks for a single `/sleep.bmp` file at the root of the SD card; if
that doesn't exist, it picks a random `.bmp` from a `/sleep/` (or `/.sleep/`) folder, avoiding repeats
of recently shown images where possible.

---

## 10. Troubleshooting

**Device won't connect to Wi-Fi**
- Make sure you're entering the correct password from **Settings > System > WiFi Networks**
- 5GHz-only networks aren't supported — the ESP32-C3 radio is 2.4GHz only

**Books or readings don't appear / catalog seems out of date**
- After adding or editing books, collections, or readings from the dashboard, the device rebuilds its
  on-device catalog index the next time it boots or wakes — give it a moment on the "Rebuilding
  catalog…" screen
- If the rebuild reports "Sync failed", try saving again from the dashboard, then power-cycle the device

**Resetting on-device data**
- All catalog data, settings, and Wi-Fi credentials are stored on the SD card under `/.myne/`. Removing
  this folder resets the device to a fresh state (you'll lose your book catalog, reading history, and
  saved settings) — back it up first if you want to keep your data
- Settings and reading data are plain JSON/NDJSON files under `/.myne/`, so they can be inspected or
  edited directly on a computer if needed (close any app holding the SD card before re-inserting it
  into the device)

**Firmware update fails**
- Make sure the device has a stable Wi-Fi connection for OTA updates, or use
  **Settings > System > SD Card Firmware Update** with a `.bin` file copied to the SD card
- If the device becomes unresponsive after a failed update, see the [README](./README.md#install-firmware)
  for how to reflash over USB
