# Troubleshooting

This document shows the most common issues and possible solutions while using Myne's network and web
features.

- [Troubleshooting](#troubleshooting)
    - [Cannot See the Device on the Network](#cannot-see-the-device-on-the-network)
    - [Connection Drops or Times Out](#connection-drops-or-times-out)
    - [Upload Fails](#upload-fails)
    - [Saved Password Not Working](#saved-password-not-working)
    - [Books or Readings Don't Appear / Catalog Seems Out of Date](#books-or-readings-dont-appear--catalog-seems-out-of-date)
    - [Resetting On-Device Data](#resetting-on-device-data)

### Cannot See the Device on the Network

**Problem:** Browser shows "Cannot connect" or "Site can't be reached"

**Solutions:**

1. Verify both devices are on the **same WiFi network**
   - Check your computer/phone WiFi settings
   - Confirm Myne shows "Connected" status on the Network screen
2. Double-check the IP address
   - Make sure you typed it correctly
   - Include `http://` at the beginning
3. Try disabling VPN if you're using one
4. Some networks have "client isolation" enabled - check with your network administrator

### Connection Drops or Times Out

**Problem:** WiFi connection is unstable

**Solutions:**

1. Move closer to the WiFi router
2. Check signal strength on the device (should be at least `||` or better)
3. Avoid interference from other devices
4. Try a different WiFi network if available
5. Only 2.4GHz networks are supported — the ESP32-C3's radio doesn't do 5GHz

### Upload Fails

**Problem:** A file upload (from the dashboard's file browser, firmware flashing, or book import)
doesn't complete or shows an error

**Solutions:**

1. Check that the SD card has enough free space (`/api/status` and the dashboard both show free space)
2. Try uploading a smaller file first to test
3. Refresh the browser page and try again
4. For firmware updates, make sure the `.bin` file matches the device's hardware variant

### Saved Password Not Working

**Problem:** Device fails to connect with saved credentials

**Solutions:**

1. When connection fails, you'll be prompted to "Forget Network"
2. Select **Yes** to remove the saved password
3. Reconnect and enter the password again
4. Choose to save the new password

### Books or Readings Don't Appear / Catalog Seems Out of Date

**Problem:** A book, collection, or reading session added or edited from the dashboard doesn't show up
on the device

**Solutions:**

1. After saving changes from the dashboard, the device rebuilds its on-device catalog index the next
   time it boots or wakes — give it a moment on the "Rebuilding catalog…" screen
2. If the rebuild reports "Sync failed", try saving again from the dashboard, then power-cycle the device
3. See [book-catalog-format.md](./book-catalog-format.md) for how the on-disk catalog is structured

### Resetting On-Device Data

**Problem:** The catalog, settings, or Wi-Fi credentials are corrupted and the device won't behave
correctly

**Solutions:**

1. All catalog data, settings, and Wi-Fi credentials are stored on the SD card under `/.myne/`. Removing
   this folder resets the device to a fresh state — you'll lose your book catalog, reading history, and
   saved settings, so back it up first if you want to keep your data
2. Settings and reading data are plain JSON/NDJSON files under `/.myne/`, so they can be inspected or
   edited directly on a computer if needed (close any app holding the SD card before re-inserting it
   into the device)
