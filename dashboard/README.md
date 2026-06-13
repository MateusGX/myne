# Myne Dashboard

The Myne dashboard is a companion web app for managing your physical book catalog, reading sessions,
SD card files, and device settings from a browser on the same local network as your device.

It's a React + TypeScript + Vite app built with shadcn/ui, and is compiled to a **single-file
`dist/index.html`** ([`vite-plugin-singlefile`](https://github.com/richardtatum/vite-plugin-singlefile)).
That file is compressed and embedded into the firmware (via `scripts/build_html.py` →
`src/network/html/DashboardHtml.generated.h`) and served directly by the device's web server, so the
dashboard ships as part of the firmware itself — no separate hosting required.

## What it talks to

The dashboard communicates with the device over its local HTTP API (see
[`src/lib/api.ts`](./src/lib/api.ts) and [`../docs/webserver-endpoints.md`](../docs/webserver-endpoints.md)):

- `/api/books*`, `/api/readings*`, `/api/collections*` — manage the book catalog, reading sessions, and
  collections
- `/api/files*` — browse, upload, download, rename, move, and delete files on the SD card
- `/api/settings`, `/api/wifi`, `/api/status` — view and change device settings, Wi-Fi, and status
- `/api/firmware/flash` — OTA firmware updates
- A WebSocket on port 81 for fast file uploads

Because requests use relative paths, the dashboard must be loaded from the device itself (i.e. via its
web server) to reach these endpoints correctly.

## Development

```bash
npm install
npm run dev
```

This starts a local Vite dev server for UI work. Since `/api/*` calls are relative, point your browser
at the dashboard served by an actual device (or a local build of `MyneWebServer`) to exercise the full
API — the standalone dev server is best for iterating on layout/components in isolation.

Other useful scripts:

```bash
npm run build      # production build -> dist/index.html (single file)
npm run typecheck  # TypeScript checks
npm run lint       # ESLint
npm run format     # Prettier
```

## Adding shadcn/ui components

```bash
npx shadcn@latest add button
```

This places new components under `src/components/ui/`. Import them as:

```tsx
import { Button } from "@/components/ui/button"
```
