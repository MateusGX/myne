import axios from "axios"

const api = axios.create()

export type FileInfo = { name: string; size: number; isDirectory: boolean }
export type Status = {
  version: string
  ip: string
  mode: "AP" | "STA"
  rssi: number
  freeHeap: number
  uptime: number
  storageTotal: number
  storageUsed: number
}
export type Setting = {
  key: string
  name: string
  category: string
  type: "toggle" | "enum" | "value" | "string"
  value: number | string
  options?: string[]
  min?: number
  max?: number
  step?: number
}
export type WifiNetwork = {
  index: number
  ssid: string
  hasPassword: boolean
  isLastConnected: boolean
}

type RawStatus = Partial<Status> & Record<string, unknown>

function numberFrom(value: unknown, fallback = 0) {
  if (typeof value === "number" && Number.isFinite(value)) return value
  if (typeof value === "string") {
    const parsed = Number(value)
    if (Number.isFinite(parsed)) return parsed
  }
  return fallback
}

function stringFrom(value: unknown, fallback = "") {
  return typeof value === "string" ? value : fallback
}

function firstNumber(data: RawStatus, keys: string[], fallback = 0) {
  for (const key of keys) {
    if (data[key] !== undefined && data[key] !== null) {
      return numberFrom(data[key], fallback)
    }
  }
  return fallback
}

function firstString(data: RawStatus, keys: string[], fallback = "") {
  for (const key of keys) {
    if (data[key] !== undefined && data[key] !== null) {
      return stringFrom(data[key], fallback)
    }
  }
  return fallback
}

function normalizeStatus(data: RawStatus): Status {
  const mode = firstString(data, ["mode", "networkMode"], "STA")
  return {
    version: firstString(data, ["version", "firmware", "firmwareVersion"], "Unknown"),
    ip: firstString(data, ["ip", "ipAddress", "address"], "Unavailable"),
    mode: mode === "AP" ? "AP" : "STA",
    rssi: firstNumber(data, ["rssi", "wifiRssi", "signal"], 0),
    freeHeap: firstNumber(data, ["freeHeap", "heap", "freeMemory"], 0),
    uptime: firstNumber(data, ["uptime", "uptimeSeconds", "runtime"], 0),
    storageTotal: firstNumber(data, ["storageTotal", "storage_total", "totalBytes", "storage_total_bytes"], 0),
    storageUsed: firstNumber(data, ["storageUsed", "storage_used", "usedBytes", "storage_used_bytes"], 0),
  }
}

export const getStatus = () =>
  api.get<RawStatus>("/api/status").then((r) => normalizeStatus(r.data))

export const getFiles = (path: string) =>
  api.get<FileInfo[]>("/api/files", { params: { path } }).then((r) => r.data)

export const createFolder = (path: string, name: string) =>
  api.post("/mkdir", null, { params: { path, name } })

export const renameFile = (path: string, name: string) =>
  api.post("/rename", null, { params: { path, name } })

export const moveFile = (path: string, dest: string) =>
  api.post("/move", null, { params: { path, dest } })

export const deleteItems = (paths: string[]) =>
  api.post(
    "/delete",
    `paths=${encodeURIComponent(JSON.stringify(paths))}`,
    { headers: { "Content-Type": "application/x-www-form-urlencoded" } },
  )

export const getSettings = () =>
  api.get<Setting[]>("/api/settings").then((r) => r.data)

export const saveSettings = (changes: Record<string, unknown>) =>
  api.post("/api/settings", changes)

export const getWifiNetworks = () =>
  api.get<WifiNetwork[]>("/api/wifi").then((r) => r.data)

export const saveWifiNetwork = (data: {
  ssid: string
  password?: string
  index?: number
}) => api.post("/api/wifi", data)

export const deleteWifiNetwork = (index: number) =>
  api.post("/api/wifi/delete", { index })

export const downloadUrl = (path: string) =>
  `/download?path=${encodeURIComponent(path)}`

export const backupDownloadUrl = () => "/api/backup/download"

export const BACKUP_RESTORE_DIR = "/"
export const BACKUP_RESTORE_FILENAME = ".myne-restore-backup.ndjson"

export const restoreBackup = () =>
  api
    .post<{ ok: boolean; error?: string }>("/api/backup/restore")
    .then((r) => r.data)

export const WS_PORT = 81

export const flashFirmware = () =>
  api.post<{ ok: boolean; message?: string; error?: string }>("/api/firmware/flash").then((r) => r.data)

export function uploadViaWebSocket(
  file: File,
  destPath: string,
  onProgress: (received: number, total: number) => void,
  saveName?: string,
): Promise<void> {
  return new Promise((resolve, reject) => {
    const host = window.location.hostname
    const ws = new WebSocket(`ws://${host}:${WS_PORT}/`)
    let done = false

    ws.binaryType = "arraybuffer"

    ws.onopen = () => {
      ws.send(`START:${saveName ?? file.name}:${file.size}:${destPath}`)
    }

    ws.onmessage = (e) => {
      const msg = e.data as string
      if (msg === "READY") {
        const CHUNK = 4096
        let offset = 0
        const send = () => {
          if (offset >= file.size) return
          const blob = file.slice(offset, offset + CHUNK)
          blob.arrayBuffer().then((buf) => {
            ws.send(buf)
            offset += buf.byteLength
          })
        }
        send()
      } else if (msg.startsWith("PROGRESS:")) {
        const [, recv, total] = msg.split(":")
        onProgress(Number(recv), Number(total))
        // Send next chunk
        const CHUNK = 4096
        const recv2 = Number(recv)
        if (recv2 < file.size) {
          const blob = file.slice(recv2, recv2 + CHUNK)
          blob.arrayBuffer().then((buf) => ws.send(buf))
        }
      } else if (msg === "DONE") {
        done = true
        onProgress(file.size, file.size)
        ws.close()
        resolve()
      } else if (msg.startsWith("ERROR:")) {
        done = true
        ws.close()
        reject(new Error(msg.slice(6)))
      }
    }

    ws.onerror = () => {
      if (!done) reject(new Error("WebSocket error"))
    }
    ws.onclose = () => {
      if (!done) reject(new Error("Connection closed unexpectedly"))
    }
  })
}

// ─── Image conversion ─────────────────────────────────────────────────────

export function isImageFile(file: File): boolean {
  return file.type.startsWith("image/")
}

export async function convertToJpeg(file: File, quality = 0.92): Promise<File> {
  return new Promise((resolve, reject) => {
    const img = new Image()
    const url = URL.createObjectURL(file)
    img.onload = () => {
      URL.revokeObjectURL(url)
      const canvas = document.createElement("canvas")
      canvas.width = img.naturalWidth
      canvas.height = img.naturalHeight
      const ctx = canvas.getContext("2d")!
      ctx.drawImage(img, 0, 0)
      canvas.toBlob(
        (blob) => {
          if (!blob) return reject(new Error("toBlob failed"))
          const name = file.name.replace(/\.[^.]+$/, "") + ".jpg"
          resolve(new File([blob], name, { type: "image/jpeg" }))
        },
        "image/jpeg",
        quality,
      )
    }
    img.onerror = () => {
      URL.revokeObjectURL(url)
      reject(new Error("Image load failed"))
    }
    img.src = url
  })
}

// ─── Physical Books ────────────────────────────────────────────────────────

export type Book = {
  id: string
  title: string
  author: string
  collection: string
  volume: string
  location: string
  notes: string
}

export type BookFormData = Omit<Book, "id">

export const getBooks = () => api.get<Book[]>("/api/books").then((r) => r.data)

export const createBook = (data: BookFormData) =>
  api.post<{ ok: boolean; id: string }>("/api/books/create", data).then((r) => r.data)

export const updateBook = (data: Book) =>
  api.post<{ ok: boolean }>("/api/books/update", data).then((r) => r.data)

export const deleteBook = (id: string) =>
  api.post<{ ok: boolean }>("/api/books/delete", { id }).then((r) => r.data)

// ─── Collections ───────────────────────────────────────────────────────────

export type Collection = {
  id: string
  name: string
  expectedCount: number
  initialVolume: number
}

export const getCollections = () =>
  api.get<Collection[]>("/api/collections").then((r) => r.data)

export const renameCollection = (id: string, name: string) =>
  api.post<{ ok: boolean }>("/api/collections/rename", { id, name }).then((r) => r.data)

export const setCollectionExpectedCount = (id: string, expectedCount: number) =>
  api
    .post<{ ok: boolean }>("/api/collections/expected-count", { id, expectedCount })
    .then((r) => r.data)

export const setCollectionInitialVolume = (id: string, initialVolume: number) =>
  api
    .post<{ ok: boolean }>("/api/collections/initial-volume", { id, initialVolume })
    .then((r) => r.data)

export const getCollectionNote = (id: string) =>
  api.get<{ id: string; note: string }>("/api/collections/note", { params: { id } }).then((r) => r.data)

export const setCollectionNote = (id: string, note: string) =>
  api.post<{ ok: boolean }>("/api/collections/note", { id, note }).then((r) => r.data)

export const deleteCollectionNote = (id: string) =>
  api.delete<{ ok: boolean }>("/api/collections/note", { params: { id } }).then((r) => r.data)

function bookImportKey(book: Pick<Book, "title" | "author" | "collection" | "volume">) {
  return [book.title, book.author, book.collection, book.volume]
    .map((value) => value.trim().toLowerCase())
    .join("\u001f")
}

export async function importBooks(
  books: Book[],
  existingBooks: Book[] = [],
  onProgress?: (done: number, total: number, currentTitle: string) => void,
): Promise<{
  ok: boolean
  count: number
  created: number
  updated: number
  failed: number
  failedBooks: Book[]
}> {
  let created = 0
  let updated = 0
  const failedBooks: Book[] = []
  const existingById = new Map(existingBooks.map((book) => [book.id, book]))
  const existingByKey = new Map(existingBooks.map((book) => [bookImportKey(book), book]))
  for (const book of books) {
    const existing =
      (book.id ? existingById.get(book.id) : undefined) ??
      existingByKey.get(bookImportKey(book))
    onProgress?.(created + updated + failedBooks.length, books.length, book.title)
    try {
      if (existing) {
        const data = { ...book, id: existing.id }
        await updateBook(data)
        updated++
        existingById.set(data.id, data)
        existingByKey.set(bookImportKey(data), data)
      } else {
        const { id: _id, ...data } = book
        const result = await createBook(data)
        created++
        const createdBook = { ...book, id: result.id }
        existingById.set(result.id, createdBook)
        existingByKey.set(bookImportKey(createdBook), createdBook)
      }
    } catch {
      failedBooks.push(book)
    }
  }
  return { ok: true, count: created + updated, created, updated, failed: failedBooks.length, failedBooks }
}

// ─── Reading Log ───────────────────────────────────────────────────────────

export type ReadingSession = { date: string; position: number }

export type Reading = {
  id: string
  status: string  // 'want' | 'reading' | 'paused' | 'finished' | 'dropped'
  readingType: number  // 0 = page, 1 = chapter
  sessions: ReadingSession[]
}

export const getReadings = (bookId: string) =>
  api.get<Reading[]>("/api/readings", { params: { bookId } }).then((r) => r.data)

export const saveReadings = (bookId: string, readings: Reading[]) =>
  api.post<{ ok: boolean }>("/api/readings/save", { bookId, readings }).then((r) => r.data)
