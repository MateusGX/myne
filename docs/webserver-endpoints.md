# Webserver Endpoints

This document describes all HTTP and WebSocket endpoints exposed by Myne's on-device webserver
(`src/network/MyneWebServer.cpp`). It's the API the [companion dashboard](../dashboard/README.md) talks
to, and can also be used directly with `curl` for scripting.

- [Webserver Endpoints](#webserver-endpoints)
  - [Overview](#overview)
  - [Dashboard \& status](#dashboard--status)
    - [GET `/`, `/files`, `/settings` - Dashboard](#get--files-settings---dashboard)
    - [GET `/api/status` - Device Status](#get-apistatus---device-status)
  - [SD card files](#sd-card-files)
    - [GET `/api/files` - List Files](#get-apifiles---list-files)
    - [GET `/download` - Download File](#get-download---download-file)
    - [GET `/api/backup/download` - Download Full Device Backup](#get-apibackupdownload---download-full-device-backup)
    - [POST `/api/backup/restore` - Restore Full Device Backup](#post-apibackuprestore---restore-full-device-backup)
    - [POST `/upload` - Upload File](#post-upload---upload-file)
    - [POST `/mkdir` - Create Folder](#post-mkdir---create-folder)
    - [POST `/rename` - Rename File](#post-rename---rename-file)
    - [POST `/move` - Move File](#post-move---move-file)
    - [POST `/delete` - Delete File(s) or Folder](#post-delete---delete-files-or-folder)
  - [WebSocket - Fast Binary Upload (port 81)](#websocket---fast-binary-upload-port-81)
  - [Settings](#settings)
    - [GET `/api/settings` - List Settings](#get-apisettings---list-settings)
    - [POST `/api/settings` - Update Settings](#post-apisettings---update-settings)
  - [Wi-Fi](#wi-fi)
    - [GET `/api/wifi` - List Saved Networks](#get-apiwifi---list-saved-networks)
    - [POST `/api/wifi` - Add/Update a Network](#post-apiwifi---addupdate-a-network)
    - [POST `/api/wifi/delete` - Remove a Network](#post-apiwifidelete---remove-a-network)
  - [Firmware](#firmware)
    - [POST `/api/firmware/flash` - Flash Firmware](#post-apifirmwareflash---flash-firmware)
  - [Books](#books)
    - [GET `/api/books` - List Books](#get-apibooks---list-books)
    - [POST `/api/books/create` - Create a Book](#post-apibookscreate---create-a-book)
    - [POST `/api/books/update` - Update a Book](#post-apibooksupdate---update-a-book)
    - [POST `/api/books/delete` - Delete a Book](#post-apibooksdelete---delete-a-book)
  - [Collections](#collections)
    - [GET `/api/collections` - List Collections](#get-apicollections---list-collections)
    - [POST `/api/collections/rename` - Rename a Collection](#post-apicollectionsrename---rename-a-collection)
    - [POST `/api/collections/expected-count` - Set Expected Book Count](#post-apicollectionsexpected-count---set-expected-book-count)
    - [POST `/api/collections/initial-volume` - Set Initial Volume](#post-apicollectionsinitial-volume---set-initial-volume)
    - [GET `/api/collections/note` - Get a Collection Note](#get-apicollectionsnote---get-a-collection-note)
    - [POST `/api/collections/note` - Set a Collection Note](#post-apicollectionsnote---set-a-collection-note)
    - [DELETE `/api/collections/note` - Clear a Collection Note](#delete-apicollectionsnote---clear-a-collection-note)
  - [Readings](#readings)
    - [GET `/api/readings` - Get a Book's Reading Sessions](#get-apireadings---get-a-books-reading-sessions)
    - [POST `/api/readings/save` - Save a Book's Reading Sessions](#post-apireadingssave---save-a-books-reading-sessions)
  - [Network Modes](#network-modes)
  - [Notes](#notes)

## Overview

- **HTTP Server**: Port 80
- **WebSocket Server**: Port 81 (fast binary file uploads)

All JSON request bodies must be sent with `Content-Type: application/json` unless noted otherwise.
Examples use `myne.local`; replace with your device's IP address if mDNS doesn't resolve on your
network (see [Notes](#notes)).

---

## Dashboard & status

### GET `/`, `/files`, `/settings` - Dashboard

All three routes serve the same thing: the bundled [dashboard](../dashboard/README.md) single-page app
(gzip-compressed HTML/JS/CSS embedded in the firmware).

```bash
curl http://myne.local/
```

**Response:** HTML page (200 OK)

---

### GET `/api/status` - Device Status

```bash
curl http://myne.local/api/status
```

**Response (200 OK):**
```json
{
  "version": "1.0.0",
  "ip": "192.168.1.100",
  "mode": "STA",
  "rssi": -45,
  "freeHeap": 123456,
  "uptime": 3600,
  "storageTotal": 31914983424,
  "storageUsed": 1048576
}
```

| Field          | Type   | Description                                          |
| -------------- | ------ | ----------------------------------------------------- |
| `version`      | string | Myne firmware version                                  |
| `ip`           | string | Device IP address                                      |
| `mode`         | string | `"STA"` (connected to WiFi) or `"AP"` (access point)   |
| `rssi`         | number | WiFi signal strength in dBm (`0` in AP mode)           |
| `freeHeap`     | number | Free heap memory in bytes                              |
| `uptime`       | number | Seconds since device boot                              |
| `storageTotal` | number | Total SD card capacity in bytes                        |
| `storageUsed`  | number | Used SD card space in bytes                            |

---

## SD card files

### GET `/api/files` - List Files

```bash
# List root directory
curl http://myne.local/api/files

# List a specific directory
curl "http://myne.local/api/files?path=/Books"
```

**Query Parameters:**

| Parameter | Required | Default | Description            |
| --------- | -------- | ------- | ----------------------- |
| `path`    | No       | `/`     | Directory path to list   |

**Response (200 OK, streamed JSON array):**
```json
[
  {"name": "Notes", "size": 0, "isDirectory": true},
  {"name": "sleep.bmp", "size": 54321, "isDirectory": false}
]
```

| Field         | Type    | Description                        |
| ------------- | ------- | ------------------------------------ |
| `name`        | string  | File or folder name                   |
| `size`        | number  | Size in bytes (`0` for directories)   |
| `isDirectory` | boolean | `true` if the item is a folder        |

**Notes:**
- Hidden files/folders (starting with `.`) are omitted unless **Settings > System > Show Hidden
  Files** is enabled
- `System Volume Information` and `XTCache` are always hidden and protected

---

### GET `/download` - Download File

```bash
curl -OJ "http://myne.local/download?path=/sleep.bmp"
```

**Query Parameters:**

| Parameter | Required | Description       |
| --------- | -------- | ------------------ |
| `path`    | Yes      | File path to download |

**Response (200 OK):** raw file bytes, streamed, with `Content-Type: application/octet-stream` and
`Content-Disposition: attachment; filename="..."`.

**Error Responses:**

| Status | Body                            | Cause                         |
| ------ | ------------------------------- | ------------------------------ |
| 400    | `Missing path` / `Invalid path` / `Path is a directory` | Bad request |
| 403    | `Cannot access system files` / `Cannot access protected items` | Hidden/protected path |
| 404    | `Item not found`                | Path does not exist            |
| 500    | `Failed to open file`            | SD card error                  |

---

### GET `/api/backup/download` - Download Full Device Backup

```bash
curl -OJ http://myne.local/api/backup/download
```

Streams a full SD-card backup as newline-delimited JSON (`.ndjson`). File contents are split into
base64 chunks so the firmware does not need to build a ZIP archive or buffer the whole card in RAM.

**Response (200 OK):** `application/x-ndjson`, with
`Content-Disposition: attachment; filename="myne-backup-<version>.ndjson"`.

**Format:**

```json
{"format":"myne-device-backup","version":1,"firmware":"..."}
{"type":"dir","path":"/Books"}
{"type":"file","path":"/Books/example.epub","size":12345}
{"type":"chunk","data":"...base64..."}
{"type":"end"}
{"type":"done"}
```

The temporary restore upload file (`/.myne-restore-backup.ndjson`) is omitted from generated backups.

---

### POST `/api/backup/restore` - Restore Full Device Backup

```bash
# 1. Upload the backup file to the restore staging path
curl -X POST -F "file=@myne-backup.ndjson" \
  "http://myne.local/upload?path=/&name=.myne-restore-backup.ndjson"

# 2. Restore the staged backup
curl -X POST http://myne.local/api/backup/restore
```

Restores the staged `.ndjson` backup from `/.myne-restore-backup.ndjson`. Files present in the backup
overwrite files with the same path on the SD card. Extra files that are not present in the backup are
left in place. After a successful restore, Myne removes the staged backup file and marks the physical
book catalog for rebuild.

**Response (200 OK):**

```json
{"ok":true}
```

**Error Responses:**

| Status | Body | Cause |
| ------ | ---- | ----- |
| 404 | `{"ok":false,"error":"Upload restore-backup.ndjson first"}` | No staged backup file |
| 400 | `{"ok":false,"error":"Backup restore failed"}` | Invalid/corrupt backup or write failure |
| 500 | `{"ok":false,"error":"Failed to open uploaded backup"}` | SD card error |

---

### POST `/upload` - Upload File

```bash
# Upload to root directory
curl -X POST -F "file=@sleep.bmp" http://myne.local/upload

# Upload to a specific directory
curl -X POST -F "file=@sleep.bmp" "http://myne.local/upload?path=/sleep"
```

**Query Parameters:**

| Parameter | Required | Default | Description                     |
| --------- | -------- | ------- | --------------------------------- |
| `path`    | No       | `/`     | Target directory for the upload   |
| `name`    | No       | uploaded filename | File name to use on the SD card |

**Response (200 OK):**
```
File uploaded successfully: sleep.bmp
```

**Error Responses (400):** `Failed to create file on SD card`, `Failed to write to SD card - disk may
be full`, `Failed to write final data to SD card`, `Upload aborted`, `Unknown error during upload`.

**Notes:**
- Existing files with the same name are overwritten
- Writes are buffered in 4KB chunks
- The firmware flasher (see [Firmware](#firmware)) reuses this endpoint — upload to
  `/firmware_update.bin` first, then call `/api/firmware/flash`

---

### POST `/mkdir` - Create Folder

```bash
curl -X POST -d "name=NewFolder&path=/" http://myne.local/mkdir
```

**Form Parameters:**

| Parameter | Required | Default | Description                  |
| --------- | -------- | ------- | ------------------------------ |
| `name`    | Yes      | -       | Name of the folder to create   |
| `path`    | No       | `/`     | Parent directory path          |

**Response (200 OK):**
```
Folder created: NewFolder
```

**Error Responses:**

| Status | Body                          | Cause                         |
| ------ | ----------------------------- | -------------------------------- |
| 400    | `Missing folder name` / `Folder name cannot be empty` / `Invalid path` | Bad request |
| 400    | `Folder already exists`        | Name collision                  |
| 500    | `Failed to create folder`      | SD card error                    |

Note: the parent directory must already exist — `/mkdir` does not create intermediate folders.

---

### POST `/rename` - Rename File

```bash
curl -X POST -d "path=/Books/old.txt&name=new.txt" http://myne.local/rename
```

**Form Parameters:**

| Parameter | Required | Description                  |
| --------- | -------- | ------------------------------ |
| `path`    | Yes      | Full path to the file to rename |
| `name`    | Yes      | New file name (no path separators) |

**Response (200 OK):** `Renamed successfully` or `Name unchanged`.

**Error Responses:**

| Status | Body | Cause |
| ------ | ---- | ----- |
| 400 | `Missing path or new name` / `Invalid path` / `New name cannot be empty` / `Invalid file name` / `Only files can be renamed` | Bad request |
| 403 | `Cannot rename protected item` / `Cannot rename to protected name` | Hidden/protected path |
| 404 | `Item not found` | Path does not exist |
| 409 | `Target already exists` | Name collision |
| 500 | `Failed to rename file` | SD card error |

Only files can be renamed — folders cannot.

---

### POST `/move` - Move File

```bash
curl -X POST -d "path=/old.txt&dest=/Books" http://myne.local/move
```

**Form Parameters:**

| Parameter | Required | Description                  |
| --------- | -------- | ------------------------------ |
| `path`    | Yes      | Full path to the file to move  |
| `dest`    | Yes      | Destination directory path     |

**Response (200 OK):** `Moved successfully` or `Already in destination`.

**Error Responses:**

| Status | Body | Cause |
| ------ | ---- | ----- |
| 400 | `Missing path or destination` / `Invalid path` / `Invalid destination` / `Only files can be moved` / `Destination is not a folder` | Bad request |
| 403 | `Cannot move protected item` / `Cannot move into protected folder` | Hidden/protected path |
| 404 | `Item not found` / `Destination not found` | Path does not exist |
| 409 | `Target already exists` | Name collision at destination |
| 500 | `Failed to move file` | SD card error |

Only files can be moved — folders cannot.

---

### POST `/delete` - Delete File(s) or Folder

```bash
# Single item (legacy form)
curl -X POST -d "path=/old.txt" http://myne.local/delete

# Multiple items
curl -X POST --data-urlencode 'paths=["/old.txt","/EmptyFolder"]' http://myne.local/delete
```

**Form Parameters (provide exactly one of):**

| Parameter | Description                                  |
| --------- | ---------------------------------------------- |
| `path`    | Single path to delete (string)                  |
| `paths`   | JSON array of paths to delete                   |

**Response (200 OK):** `All items deleted successfully`.

**Error Responses:**

| Status | Body | Cause |
| ------ | ---- | ----- |
| 400 | `Missing 'path' or 'paths' argument` / `Provide either 'path' or 'paths', not both` / `Invalid paths format` / `No paths provided` | Bad request |
| 500 | `Failed to delete some items: <details>` | One or more items failed (e.g. cannot delete root, non-empty folder, protected item, not found) |

**Constraints:** the root directory (`/`) cannot be deleted; directories must be empty; hidden/protected
items cannot be deleted.

---

## WebSocket - Fast Binary Upload (port 81)

A faster alternative to `POST /upload` for large files, used by the dashboard's drag-and-drop uploader.

```text
ws://myne.local:81/
```

**Protocol:**

1. Client sends a text frame: `START:<filename>:<size>:<path>`
   - e.g. `START:sleep.bmp:1234567:/sleep`
2. Server responds `READY`, or `ERROR:<message>` if the file couldn't be created
3. Client streams the file as binary frames (any chunk size; the dashboard uses 4096 bytes)
4. Server sends `PROGRESS:<received>:<total>` periodically (every 64KB or on completion)
5. Server sends `DONE` on success, or `ERROR:<message>` on failure

**Error messages:** `ERROR:Upload already in progress`, `ERROR:Invalid START format`,
`ERROR:Failed to create file`, `ERROR:Upload overflow` (more bytes sent than `<size>`),
`ERROR:Write failed - disk full?`.

**Notes:**
- A zero-byte upload (`<size>` is `0`) completes immediately with `DONE` after `START`
- Existing files with the same name are overwritten
- Disconnecting mid-upload deletes the partial file

---

## Settings

### GET `/api/settings` - List Settings

```bash
curl http://myne.local/api/settings
```

**Response (200 OK, streamed JSON array):**
```json
[
  {"key": "timeToSleep", "name": "Time to Sleep", "category": "System", "type": "enum", "value": 1, "options": ["1 min", "5 min", "10 min", "15 min", "30 min"]},
  {"key": "showHiddenFiles", "name": "Show Hidden Files", "category": "System", "type": "toggle", "value": 0},
  {"key": "timezoneOffset", "name": "Timezone", "category": "System", "type": "value", "value": 0, "min": -12, "max": 14, "step": 1}
]
```

Each entry's shape depends on `type`:

| `type`   | Fields                                    | Meaning                                  |
| -------- | ------------------------------------------ | ------------------------------------------ |
| `toggle` | `value` (0 or 1)                            | On/off setting                              |
| `enum`   | `value` (index), `options` (string array)   | One of a fixed list of choices              |
| `value`  | `value`, `min`, `max`, `step`               | Numeric range                               |
| `string` | `value` (string)                            | Free-text setting                           |

Action-only menu items (e.g. "Check for Updates", "Remap Front Buttons") have no `key` and are not
included in this list — they aren't remotely settable.

---

### POST `/api/settings` - Update Settings

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"showHiddenFiles": 1, "timeToSleep": 2}' \
  http://myne.local/api/settings
```

**Request body:** a JSON object mapping setting `key`s (from `GET /api/settings`) to new values.

**Response (200 OK):**
```
Applied 2 setting(s)
```

**Error Responses (400):** `Missing JSON body`, `Invalid JSON: <error>`.

**Notes:**
- Enum values are validated against the option count; numeric values against `min`/`max`
- Unknown keys are silently ignored
- Settings are written to the SD card after all changes are applied (see SPIFFS write-throttling note
  in [CLAUDE.md](../CLAUDE.md))

---

## Wi-Fi

### GET `/api/wifi` - List Saved Networks

```bash
curl http://myne.local/api/wifi
```

**Response (200 OK, streamed JSON array):**
```json
[
  {"index": 0, "ssid": "HomeNetwork", "hasPassword": true, "isLastConnected": true},
  {"index": 1, "ssid": "OpenGuestWifi", "hasPassword": false, "isLastConnected": false}
]
```

Passwords are never returned — only `hasPassword`.

---

### POST `/api/wifi` - Add/Update a Network

```bash
# Add a new network
curl -X POST -H "Content-Type: application/json" \
  -d '{"ssid": "HomeNetwork", "password": "secret123"}' \
  http://myne.local/api/wifi

# Update an existing network's password (by index)
curl -X POST -H "Content-Type: application/json" \
  -d '{"index": 0, "ssid": "HomeNetwork", "password": "newpassword"}' \
  http://myne.local/api/wifi
```

| Field      | Required | Description                                                        |
| ---------- | -------- | --------------------------------------------------------------------- |
| `ssid`     | Yes      | Network name                                                           |
| `password` | No       | Network password (omit/empty for open networks; preserved on update if omitted) |
| `index`    | No       | If present, updates the network at this index instead of adding a new one |

**Response (200 OK):** `OK`

**Error Responses (400):** `Missing JSON body`, `Invalid JSON: <error>`, `SSID is required`,
`Invalid network index`, `Failed to update Wi-Fi network`, `Cannot add network (limit reached)`.

---

### POST `/api/wifi/delete` - Remove a Network

```bash
curl -X POST -H "Content-Type: application/json" -d '{"index": 1}' http://myne.local/api/wifi/delete
```

**Response (200 OK):** `OK`

**Error Responses (400):** `Missing JSON body`, `Invalid JSON: <error>`, `Missing index`,
`Invalid network index`, `Failed to delete Wi-Fi network`.

(This is a `POST`, not `DELETE`, due to limitations of the underlying ESP32 web server library.)

---

## Firmware

### POST `/api/firmware/flash` - Flash Firmware

Two-step process:

```bash
# 1. Upload the firmware image
curl -X POST -F "file=@firmware.bin" "http://myne.local/upload?path=/&name=firmware_update.bin"

# 2. Trigger the flash
curl -X POST http://myne.local/api/firmware/flash
```

The image must first be uploaded to `/firmware_update.bin` via `POST /upload`.
`POST /api/firmware/flash` then validates and flashes it to the OTA partition.

**Response (200 OK):**
```json
{"ok": true, "message": "Flashing firmware, device will restart"}
```
The connection is closed immediately and the device restarts ~1.5s later.

**Error Responses (400):**
```json
{"ok": false, "error": "No firmware file found. Upload firmware_update.bin first."}
```
```json
{"ok": false, "error": "Invalid firmware: BAD_MAGIC"}
```
(`error` values include `OPEN_FAIL`, `TOO_SMALL`, `TOO_LARGE`, `BAD_MAGIC`, `BAD_SEGMENTS`,
`BAD_CHECKSUM`, `BAD_SHA`, `BAD_SIZE`.)

---

## Books

See [book-catalog-format.md](./book-catalog-format.md) for the on-disk storage format these endpoints
read and write.

### GET `/api/books` - List Books

```bash
curl http://myne.local/api/books
```

**Response (200 OK, streamed JSON array):**
```json
[
  {"id": "lz3k9f2a", "title": "The Hobbit", "author": "J.R.R. Tolkien", "collection": "Middle-earth", "volume": "", "location": "Shelf 2", "note": ""}
]
```

| Field        | Type   | Description                          |
| ------------ | ------ | --------------------------------------- |
| `id`         | string | Book ID                                  |
| `title`      | string | Title                                    |
| `author`     | string | Author                                   |
| `collection` | string | Collection name (empty if standalone)    |
| `volume`     | string | Volume/number label                      |
| `location`   | string | Free-text shelf/location                 |
| `note`       | string | Free-text note                           |

---

### POST `/api/books/create` - Create a Book

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"title": "The Hobbit", "author": "J.R.R. Tolkien", "collection": "Middle-earth"}' \
  http://myne.local/api/books/create
```

**Request body:** `title` is required (non-empty); `author`, `collection`, `volume`, `location`, and
`note` are optional and default to `""`.

**Response (201 Created):**
```json
{"ok": true, "id": "lz3k9f2a"}
```

**Error Responses:** 400 `Missing JSON body` / `Invalid JSON: <error>` / `title is required`; 500
`Failed to initialize book store` / `Failed to save book`.

Creating a book sets `/.myne/sync_needed` so the on-device catalog is rebuilt on next boot/connect.

---

### POST `/api/books/update` - Update a Book

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"id": "lz3k9f2a", "location": "Shelf 3"}' \
  http://myne.local/api/books/update
```

**Request body:** `id` is required; any of `title`, `author`, `collection`, `volume`, `location`,
`note` may be included and will be updated (omitted fields are left unchanged).

**Response (200 OK):** `{"ok": true}`

**Error Responses:** 400 `Missing JSON body` / `Invalid JSON: <error>` / `id is required`; 404 `Book not
found`; 500 `Failed to initialize book store`.

---

### POST `/api/books/delete` - Delete a Book

```bash
curl -X POST -H "Content-Type: application/json" -d '{"id": "lz3k9f2a"}' http://myne.local/api/books/delete
```

**Response (200 OK):** `{"ok": true}`

**Error Responses:** 400 `Missing JSON body` / `Invalid JSON: <error>` / `id is required`; 404 `Book not
found`; 500 `Failed to initialize book store`.

Also deletes the book's reading log (`/.myne/readings/{id}.json`) and reading summary
(`/.myne/readings-sum/{id}.bin`).

---

## Collections

### GET `/api/collections` - List Collections

```bash
curl http://myne.local/api/collections
```

**Response (200 OK, streamed JSON array):**
```json
[
  {"id": "a1b2c3d4", "name": "Middle-earth", "expectedCount": 7, "initialVolume": 1, "note": "Read in publication order"}
]
```

`id` is the persistent 8-hex-character collection ID (see
[book-catalog-format.md](./book-catalog-format.md#collectionsndjson)). `expectedCount` is `0` when
no expected total is set. `initialVolume` is `0` when no initial volume is set. `note` is `""` when
no collection note is set.

---

### POST `/api/collections/rename` - Rename a Collection

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"id": "a1b2c3d4", "name": "Middle Earth"}' \
  http://myne.local/api/collections/rename
```

**Response (200 OK):** `{"ok": true}`

**Error Responses:** 400 `Missing JSON body` / `Invalid JSON: <error>` / `id and name are required`; 404
`Collection not found`.

Updates the registry entry and every member book's stored collection name, and flags the catalog for a
full rebuild.

---

### POST `/api/collections/expected-count` - Set Expected Book Count

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"id": "a1b2c3d4", "expectedCount": 7}' \
  http://myne.local/api/collections/expected-count
```

Set `expectedCount` to `0` to clear it. Negative values are stored as `0`.

**Response (200 OK):** `{"ok": true}`

**Error Responses:** 400 `Missing JSON body` / `Invalid JSON: <error>` / `id is required`; 404
`Collection not found`.

---

### POST `/api/collections/initial-volume` - Set Initial Volume

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"id": "a1b2c3d4", "initialVolume": 1}' \
  http://myne.local/api/collections/initial-volume
```

Set `initialVolume` to `0` to clear it. Negative values are stored as `0`.

**Response (200 OK):** `{"ok": true}`

**Error Responses:** 400 `Missing JSON body` / `Invalid JSON: <error>` / `id is required`; 404
`Collection not found`.

---

### GET `/api/collections/note` - Get a Collection Note

```bash
curl "http://myne.local/api/collections/note?id=a1b2c3d4"
```

**Response (200 OK):**
```json
{"id": "a1b2c3d4", "note": "Read in publication order, not chronological order"}
```
`note` is `""` if no note is set. **Error (400):** `Missing id`.

---

### POST `/api/collections/note` - Set a Collection Note

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"id": "a1b2c3d4", "note": "Read in publication order"}' \
  http://myne.local/api/collections/note
```

**Response (200 OK):** `{"ok": true}`

**Error Responses:** 400 `Missing JSON body` / `Invalid JSON: <error>` / `id is required`; 500 `Failed to
save collection note`. Notes are limited to 64 bytes.

---

### DELETE `/api/collections/note` - Clear a Collection Note

```bash
curl -X DELETE "http://myne.local/api/collections/note?id=a1b2c3d4"
```

**Response (200 OK):** `{"ok": true}`

**Error Responses:** 400 `Missing id`; 500 `Failed to delete collection note`.

---

## Readings

### GET `/api/readings` - Get a Book's Reading Sessions

```bash
curl "http://myne.local/api/readings?bookId=lz3k9f2a"
```

**Response (200 OK):**
```json
[
  {
    "id": "r7x2m9q1",
    "status": "reading",
    "readingType": 0,
    "sessions": [
      {"date": "2026-06-01", "position": 12},
      {"date": "2026-06-05", "position": 42}
    ]
  }
]
```

| Field         | Type   | Description                                                  |
| ------------- | ------ | --------------------------------------------------------------- |
| `id`          | string | Reading entry ID                                                 |
| `status`      | string | `want`, `reading`, `paused`, `finished`, or `dropped`            |
| `readingType` | number | `0` = Page, `1` = Chapter                                        |
| `sessions`    | array  | `{date: "YYYY-MM-DD", position: number}`, oldest first           |

**Error (400):** `Missing bookId`.

---

### POST `/api/readings/save` - Save a Book's Reading Sessions

```bash
curl -X POST -H "Content-Type: application/json" -d '{
  "bookId": "lz3k9f2a",
  "readings": [
    {
      "id": "r7x2m9q1",
      "status": "reading",
      "readingType": 0,
      "sessions": [{"date": "2026-06-05", "position": 50}]
    }
  ]
}' http://myne.local/api/readings/save
```

**Request body:**

| Field      | Required | Description                                                            |
| ---------- | -------- | -------------------------------------------------------------------------- |
| `bookId`   | Yes      | Book ID                                                                     |
| `readings` | Yes      | Array of reading entries (same shape as `GET /api/readings`)               |

For each reading entry, `id` may be empty to create a new entry (a new ID is generated); `status`
defaults to `reading` and `readingType` defaults to `0` if omitted.

**Response (200 OK):** `{"ok": true}`

**Error Responses:** 400 `Missing JSON body` / `Invalid JSON` / `bookId is required` / `readings array is
required`; 500 `Failed to save readings`.

This replaces the full reading-entry list for the book in `/.myne/readings/{bookId}.json`, regenerates
`/.myne/readings-sum/{bookId}.bin`, and flags the catalog for rebuild. Up to 200 sessions per reading
entry are kept.

---

## Network Modes

The device operates in one of two Wi-Fi modes, reflected in `GET /api/status`:

- **Station (STA)** — connects to an existing network; `mode: "STA"`, IP assigned by DHCP, `rssi`
  reflects signal strength
- **Access Point (AP)** — device creates its own hotspot (default `192.168.4.1`); `mode: "AP"`,
  `rssi: 0`

---

## Notes

- Examples use `myne.local`. If mDNS doesn't resolve on your network, use the device's IP address shown
  on the **Network** screen (e.g. `http://192.168.1.102/`)
- All SD card paths start with `/`; trailing slashes are stripped except for the root `/`
- Several endpoints stream their JSON response (no `Content-Length`) to avoid buffering large
  responses in RAM
