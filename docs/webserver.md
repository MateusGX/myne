# Web Server Guide

This guide explains how to connect Myne to Wi-Fi and use its built-in web server — either through the
[companion dashboard](../dashboard/README.md) or directly with `curl` — to manage files on your SD card,
change settings, and flash firmware over the network.

## Overview

Myne's built-in web server lets you, from any device on the same Wi-Fi network:

- Manage your physical book catalog, collections, and reading sessions
- Browse, upload, download, rename, move, and delete files on the SD card
- Download and restore full SD-card backups
- View and change device settings, including saved Wi-Fi networks
- Flash new firmware over the air

---

## Step 1: Connect to Wi-Fi

From the Home Screen, open **Network** to join an existing Wi-Fi network (station mode) or start the
device's own hotspot (access point mode). Both modes show a QR code to help you connect a phone or
computer. See [USER_GUIDE.md](../USER_GUIDE.md#6-file-transfer--dashboard) for details.

Once connected, the screen shows the device's **IP address** (e.g. `192.168.1.102`) — note this down,
you'll need it to reach the web server.

Saved networks (with passwords) can be managed later from **Settings > System > WiFi Networks**.

> **Note:** only 2.4GHz networks are supported — the ESP32-C3's radio doesn't do 5GHz.

---

## Step 2: Open the Dashboard

On a computer, phone, or tablet connected to the **same Wi-Fi network**, open a browser and go to the
device's IP address:

```
http://192.168.1.102/
```

This serves the [companion dashboard](../dashboard/README.md) — a single-page web app for managing
your book catalog, reading sessions, SD card files, and device settings. From here you can:

- Add, edit, and delete books and collections, and browse by letter
- View and log reading sessions per book
- Browse/upload/download/rename/move/delete files on the SD card (including fast drag-and-drop
  uploads over a WebSocket)
- Download and restore a full device backup from Settings
- View device status (battery, Wi-Fi signal, free SD space) and change settings
- Add or remove saved Wi-Fi networks
- Flash new firmware

---

## Command-line file & data management

For scripting or power users, every dashboard feature is backed by a plain HTTP/JSON API documented in
[webserver-endpoints.md](./webserver-endpoints.md) — for example:

```bash
# List files at the root of the SD card
curl http://192.168.1.102/api/files

# Upload a file
curl -X POST -F "file=@sleep.bmp" "http://192.168.1.102/upload?path=/sleep"

# Create a folder
curl -X POST -d "name=Sleep&path=/" http://192.168.1.102/mkdir

# Delete a file
curl -X POST -d "path=/old.bmp" http://192.168.1.102/delete
```

See [webserver-endpoints.md](./webserver-endpoints.md) for the full set of endpoints, including the
book catalog, reading log, settings, and Wi-Fi management APIs, and the
[book-catalog-format.md](./book-catalog-format.md) for the on-disk data formats they read and write.

---

## Security Notes

- The web server runs on port 80 (standard HTTP); the fast-upload WebSocket runs on port 81
- **No authentication is required** — anyone on the same Wi-Fi network can access the dashboard and API
- The web server is only active while the device is connected to Wi-Fi (Network screen)
- For security, only use Myne's Wi-Fi features on trusted private networks

---

## Related Documentation

- [User Guide](../USER_GUIDE.md) — general device operation
- [Webserver Endpoints](./webserver-endpoints.md) — full HTTP/WebSocket API reference
- [Book Catalog & Reading Log Format](./book-catalog-format.md) — on-disk data formats
- [Dashboard README](../dashboard/README.md) — companion web app
- [Troubleshooting](./troubleshooting.md)
