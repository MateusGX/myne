# Project Vision & Scope: Myne

The goal of Myne is to provide a focused, open-source tool for cataloging the physical books you own and tracking your
reading sessions for them on the Xteink X4. We believe a dedicated device for this should do one thing exceptionally
well: **help you keep track of what you're reading, without getting in the way.**

## 1. Core Mission

To provide a lightweight, high-performance firmware that catalogs a personal library of physical books and logs
reading sessions against them — on-device and via a companion web dashboard — prioritizing reliability and simplicity
over "swiss-army-knife" functionality.

## 2. Scope

### In-Scope

*These are features that directly improve the primary purpose of the device.*

* **User Experience:** User-friendly interfaces and interactions for navigating the firmware — button mapping,
  browsing the book catalog, logging reading sessions, and viewing stats.
* **Library Management:** Simple, intuitive ways to organize and navigate a catalog of physical books — by
  title/author letter, by collection, with notes per collection.
* **Reading Session Tracking:** Logging reading sessions per book (status, progress, history) and aggregating
  reading stats (daily/monthly).
* **Local Transfer & Dashboard Sync:** Simple, "pull"-based file transfer via a basic web server and widely-used
  standards (WebDAV), plus the companion web dashboard for managing the book catalog, collections, reading sessions,
  SD card files, and device settings from a browser on the local network.
* **Language Support:** Support for multiple languages in the interface.
* **E-Ink Driver Refinement:** Reducing full-screen flashes (ghosting management) and improving general rendering.
* **Clock Display:** The X4 relies on the ESP32-C3's internal RTC, which drifts significantly during deep sleep.
  NTP sync could correct this, with an appropriate user experience around connecting to the internet on wake or on
  demand. This causes some tension with the **Active Connectivity** section below, so please open a discussion about
  this UX if it's a feature you would find useful.

### Out-of-Scope

*These items are rejected because they compromise the device's stability or mission.*

* **E-book/Document Reading:** No EPUB, PDF, or other document rendering. Myne tracks your reading *of* physical
  books — it does not display book content. If you're looking for an e-reader, see
  [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) (the project Myne is forked from) and its
  other forks.
* **Interactive Apps:** No Notepads, Calculators, or Games. This is a book tracker, not a PDA.
* **Active Connectivity:** No RSS readers, News aggregators, or Web browsers. Background Wi-Fi tasks drain the
  battery and complicate the single-core CPU's execution. (Local-network dashboard sync and file transfer, which are
  on-demand and user-initiated, are in scope — see Local Transfer above.)
* **Media Playback:** No Audio players or Audio-books.
* **Complex Annotation:** No typed-out notes beyond simple per-book/per-collection notes. Free-form note-taking is
  better suited for devices with better input capabilities and more powerful chips.

## 3. Idea Evaluation

While I appreciate the desire to add new and exciting features to Myne, Myne is designed to be a lightweight,
reliable, and focused physical-book library and reading-session tracker. Things which distract or compromise the
device's core mission will not be accepted. As a guiding question, consider whether your idea improves cataloging
your books or tracking your reading of them — and, critically, doesn't distract from that.

> **Note to Contributors:** If you are unsure if your idea fits the scope, please open a **Discussion** before you
> start coding!
