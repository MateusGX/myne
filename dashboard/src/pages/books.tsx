import { useEffect, useMemo, useRef, useState } from "react"
import {
  BookOpen,
  CaretDoubleDownIcon,
  CaretDoubleUpIcon,
  CaretDown,
  CaretRight,
  ArrowSquareIn,
  ArrowSquareOut,
  CircleNotch,
  FileXls,
  Hash,
  NotePencilIcon,
  PencilSimple,
  Plus,
  Trash,
  X,
} from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Separator } from "@/components/ui/separator"
import { Switch } from "@/components/ui/switch"
import {
  EmptyState,
  MetricCard,
  PageHeader,
  Toolbar,
} from "@/components/dashboard-layout"
import { BadgeLike } from "@/components/books/BookBadge"
import { BookRow } from "@/components/books/BookRow"
import { BookDetailDialog } from "@/components/books/BookDetailDialog"
import { BookFormDialog } from "@/components/books/BookFormDialog"
import { ReadingsDialog } from "@/components/books/ReadingsDialog"
import {
  ImportDialog,
  type ImportMode,
  type ImportPhase,
} from "@/components/books/ImportDialog"
import {
  deleteBook,
  getCollections,
  renameCollection,
  deleteCollectionNote,
  importBooks,
  setCollectionExpectedCount,
  setCollectionInitialVolume,
  setCollectionMetadata,
  setCollectionNote,
  updateBook,
  type Book,
  type BookFormData,
  type Collection,
} from "@/lib/api"
import { useBooksData } from "@/lib/useBooksData"
import {
  type CollectionMetadata,
  downloadBooksTemplate,
  exportBooksXlsx,
  parseBooksXlsx,
} from "@/lib/booksXlsx"
import { MAX_COLLECTION_NAME_BYTES, truncateUtf8 } from "@/lib/utils"

const emptyForm: BookFormData = {
  title: "",
  author: "",
  collection: "",
  volume: "",
  location: "",
  note: "",
}

type CollectionGroup = { name: string; books: Book[] }
type MissingVolumeMode = "with-existing" | "missing-only"
type CollectionDisplayRow =
  | { kind: "book"; key: string; book: Book }
  | { kind: "missing"; key: string; volume: number }

function parseVolumeNumber(value: string) {
  const match = value.trim().match(/\d+/)
  if (!match) return null
  const volume = Number(match[0])
  return Number.isFinite(volume) ? volume : null
}

function expectedVolumes(collection?: Collection) {
  if (
    !collection ||
    collection.expectedCount <= 0 ||
    collection.initialVolume <= 0
  ) {
    return []
  }
  return Array.from(
    { length: collection.expectedCount },
    (_, index) => collection.initialVolume + index
  )
}

function missingVolumes(collection: Collection | undefined, books: Book[]) {
  const present = new Set(
    books
      .map((book) => parseVolumeNumber(book.volume))
      .filter((volume): volume is number => volume !== null)
  )
  return expectedVolumes(collection).filter((volume) => !present.has(volume))
}

function buildCollectionRows({
  visibleBooks,
  allCollectionBooks,
  collection,
  missingVolumeMode,
  showMissingVolumes,
}: {
  visibleBooks: Book[]
  allCollectionBooks: Book[]
  collection?: Collection
  missingVolumeMode: MissingVolumeMode
  showMissingVolumes: boolean
}): CollectionDisplayRow[] {
  const volumes = expectedVolumes(collection)
  if (!showMissingVolumes) {
    return visibleBooks.map((book) => ({
      kind: "book",
      key: book.id,
      book,
    }))
  }
  if (volumes.length === 0) {
    if (missingVolumeMode === "missing-only") return []
    return visibleBooks.map((book) => ({
      kind: "book",
      key: book.id,
      book,
    }))
  }

  const expectedSet = new Set(volumes)
  const present = new Set(
    allCollectionBooks
      .map((book) => parseVolumeNumber(book.volume))
      .filter((volume): volume is number => volume !== null)
  )
  const visibleByVolume = new Map<number, Book[]>()
  const visibleOutsideRange: Book[] = []

  for (const book of visibleBooks) {
    const volume = parseVolumeNumber(book.volume)
    if (volume !== null && expectedSet.has(volume)) {
      const books = visibleByVolume.get(volume) ?? []
      books.push(book)
      visibleByVolume.set(volume, books)
    } else {
      visibleOutsideRange.push(book)
    }
  }

  const rows: CollectionDisplayRow[] = []
  for (const volume of volumes) {
    if (missingVolumeMode === "with-existing") {
      for (const book of visibleByVolume.get(volume) ?? []) {
        rows.push({ kind: "book", key: book.id, book })
      }
    }
    if (!present.has(volume)) {
      rows.push({
        kind: "missing",
        key: `missing-${collection?.id ?? "collection"}-${volume}`,
        volume,
      })
    }
  }

  if (missingVolumeMode === "with-existing") {
    for (const book of visibleOutsideRange) {
      rows.push({ kind: "book", key: book.id, book })
    }
  }

  return rows
}

function MissingVolumeRow({ volume }: { volume: number }) {
  return (
    <div className="flex items-center gap-4 px-4 py-3 text-muted-foreground">
      <div className="flex size-10 shrink-0 items-center justify-center rounded-md border border-dashed border-border bg-muted/30">
        <Hash size={16} />
      </div>
      <div className="min-w-0 flex-1">
        <div className="flex items-baseline gap-1.5">
          <span className="truncate text-sm font-medium">Missing volume</span>
          <span className="shrink-0 text-xs">Vol. {volume}</span>
        </div>
        <p className="truncate text-xs">Not registered in this collection</p>
      </div>
    </div>
  )
}

export function BooksPage() {
  const {
    books,
    collections,
    loading,
    loaded,
    refresh,
    setBooks,
    setCollections,
  } = useBooksData()
  const [search, setSearch] = useState("")
  const [collectionFilter, setCollectionFilter] = useState("All")
  const [showIncompleteOnly, setShowIncompleteOnly] = useState(false)
  const [missingVolumeMode, setMissingVolumeMode] =
    useState<MissingVolumeMode>("with-existing")
  const [expandedCollections, setExpandedCollections] = useState<Set<string>>(
    new Set()
  )
  const [showForm, setShowForm] = useState(false)
  const [editing, setEditing] = useState<Book | null>(null)
  const [form, setForm] = useState<BookFormData>(emptyForm)
  const [saving, setSaving] = useState(false)
  const [importPhase, setImportPhase] = useState<ImportPhase | null>(null)
  const [detailBook, setDetailBook] = useState<Book | null>(null)
  const [readingsBook, setReadingsBook] = useState<Book | null>(null)
  const importInputRef = useRef<HTMLInputElement>(null)
  const [editingNote, setEditingNote] = useState<string | null>(null)
  const [noteDraft, setNoteDraft] = useState("")
  const didExpandInitialCollections = useRef(false)

  const nameToId = useMemo(
    () => new Map(collections.map((c) => [c.name, c.id])),
    [collections]
  )
  const collectionByName = useMemo(
    () => new Map(collections.map((c) => [c.name, c])),
    [collections]
  )
  const collectionNotes = useMemo(
    () =>
      Object.fromEntries(
        collections.map((collection) => [collection.id, collection.note ?? ""])
      ),
    [collections]
  )

  useEffect(() => {
    refresh(false).catch(() => toast.error("Failed to load books"))
  }, [refresh])

  useEffect(() => {
    if (!loaded || didExpandInitialCollections.current) return
    setExpandedCollections(
      new Set(books.map((b) => b.collection).filter(Boolean))
    )
    didExpandInitialCollections.current = true
  }, [books, loaded])

  const initialLoading = loading && !loaded
  // `loading` also flips back on after every refresh(true) call (post
  // save/delete/import). Once the first load has completed, show a
  // non-blocking "refreshing" indicator instead of the full skeleton.
  const isRefreshing = loading && loaded

  const collectionOptions = useMemo(
    () => [
      "All",
      ...Array.from(
        new Set(books.map((b) => b.collection).filter(Boolean))
      ).sort(),
    ],
    [books]
  )
  const existingCollections = useMemo(
    () =>
      Array.from(
        new Set(books.map((b) => b.collection).filter(Boolean))
      ).sort(),
    [books]
  )

  const filtered = useMemo(
    () =>
      books.filter((b) => {
        const matchCol =
          collectionFilter === "All" || b.collection === collectionFilter
        const q = search.toLowerCase()
        const matchSearch =
          !q ||
          b.title.toLowerCase().includes(q) ||
          b.author.toLowerCase().includes(q)
        return matchCol && matchSearch
      }),
    [books, collectionFilter, search]
  )

  const collectionBookCounts = useMemo(() => {
    const counts = new Map<string, number>()
    for (const book of books) {
      if (!book.collection) continue
      counts.set(book.collection, (counts.get(book.collection) ?? 0) + 1)
    }
    return counts
  }, [books])

  const booksByCollection = useMemo(() => {
    const byCollection = new Map<string, Book[]>()
    for (const book of books) {
      if (!book.collection) continue
      const collectionBooks = byCollection.get(book.collection) ?? []
      collectionBooks.push(book)
      byCollection.set(book.collection, collectionBooks)
    }
    return byCollection
  }, [books])

  const incompleteCollections = useMemo(
    () =>
      new Set(
        collections
          .filter(
            (collection) =>
              collection.expectedCount > 0 &&
              (collectionBookCounts.get(collection.name) ?? 0) <
                collection.expectedCount
          )
          .map((collection) => collection.name)
      ),
    [collectionBookCounts, collections]
  )

  const { groups, standalone } = useMemo(() => {
    const colMap = new Map<string, Book[]>()
    const standalone: Book[] = []
    for (const book of filtered) {
      if (book.collection) {
        if (!colMap.has(book.collection)) colMap.set(book.collection, [])
        colMap.get(book.collection)!.push(book)
      } else {
        standalone.push(book)
      }
    }
    const groups: CollectionGroup[] = [...colMap.entries()]
      .filter(
        ([name]) => !showIncompleteOnly || incompleteCollections.has(name)
      )
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([name, books]) => ({
        name,
        books: [...books].sort((a, b) => a.title.localeCompare(b.title)),
      }))
    return {
      groups,
      standalone: showIncompleteOnly
        ? []
        : standalone.sort((a, b) => a.title.localeCompare(b.title)),
    }
  }, [filtered, incompleteCollections, showIncompleteOnly])

  const allExpanded =
    groups.length > 0 &&
    groups.every((group) => expandedCollections.has(group.name))

  function toggleCollection(name: string) {
    setExpandedCollections((prev) => {
      const next = new Set(prev)
      if (next.has(name)) next.delete(name)
      else next.add(name)
      return next
    })
  }

  function expandAllCollections() {
    setExpandedCollections(new Set(groups.map((group) => group.name)))
  }

  function collapseAllCollections() {
    setExpandedCollections((prev) => {
      const next = new Set(prev)
      for (const group of groups) next.delete(group.name)
      return next
    })
  }

  function startEditNote(name: string) {
    const id = nameToId.get(name)
    if (!id) return
    setNoteDraft(collectionNotes[id] ?? "")
    setEditingNote(name)
  }

  async function saveNote(name: string) {
    const id = nameToId.get(name)
    if (!id) {
      toast.error("Collection not yet synced")
      return
    }
    try {
      await setCollectionNote(id, noteDraft)
      setCollections((prev) =>
        prev.map((collection) =>
          collection.id === id ? { ...collection, note: noteDraft } : collection
        )
      )
      setEditingNote(null)
    } catch {
      toast.error("Failed to save collection note")
    }
  }

  async function deleteNote(name: string) {
    const id = nameToId.get(name)
    if (!id) return
    try {
      await deleteCollectionNote(id)
      setCollections((prev) =>
        prev.map((collection) =>
          collection.id === id ? { ...collection, note: "" } : collection
        )
      )
    } catch {
      toast.error("Failed to delete collection note")
    }
  }

  async function handleRenameCollection(name: string) {
    const id = nameToId.get(name)
    if (!id) {
      toast.error("Collection not yet synced")
      return
    }
    const typed = prompt("Rename collection", name)?.trim()
    if (!typed || typed === name) return
    const newName = truncateUtf8(typed, MAX_COLLECTION_NAME_BYTES)
    if (newName !== typed) {
      toast.error(
        `Collection name too long, truncated to ${MAX_COLLECTION_NAME_BYTES} bytes`
      )
    }
    if (newName === name) return
    try {
      await renameCollection(id, newName)
      setBooks((prev) =>
        prev.map((book) =>
          book.collection === name ? { ...book, collection: newName } : book
        )
      )
      setCollections((prev) =>
        prev.map((collection) =>
          collection.id === id ? { ...collection, name: newName } : collection
        )
      )
      setExpandedCollections((prev) => {
        const next = new Set(prev)
        if (next.delete(name)) next.add(newName)
        return next
      })
      toast.success("Collection renamed")
    } catch {
      toast.error("Failed to rename collection")
    }
  }

  async function handleSetExpectedCount(name: string) {
    const collection = collectionByName.get(name)
    if (!collection) {
      toast.error("Collection not yet synced")
      return
    }
    const typed = prompt(
      "Expected total books in this collection",
      collection.expectedCount > 0 ? String(collection.expectedCount) : ""
    )
    if (typed === null) return
    const expectedCount = Math.max(0, Math.floor(Number(typed.trim() || "0")))
    if (!Number.isFinite(expectedCount)) {
      toast.error("Expected total must be a number")
      return
    }
    try {
      await setCollectionExpectedCount(collection.id, expectedCount)
      setCollections((prev) =>
        prev.map((c) => (c.id === collection.id ? { ...c, expectedCount } : c))
      )
      toast.success("Expected total updated")
    } catch {
      toast.error("Failed to update expected total")
    }
  }

  async function handleSetInitialVolume(name: string) {
    const collection = collectionByName.get(name)
    if (!collection) {
      toast.error("Collection not yet synced")
      return
    }
    const typed = prompt(
      "Initial volume in this collection",
      collection.initialVolume > 0 ? String(collection.initialVolume) : ""
    )
    if (typed === null) return
    const initialVolume = Math.max(0, Math.floor(Number(typed.trim() || "0")))
    if (!Number.isFinite(initialVolume)) {
      toast.error("Initial volume must be a number")
      return
    }
    try {
      await setCollectionInitialVolume(collection.id, initialVolume)
      setCollections((prev) =>
        prev.map((c) => (c.id === collection.id ? { ...c, initialVolume } : c))
      )
      toast.success("Initial volume updated")
    } catch {
      toast.error("Failed to update initial volume")
    }
  }

  function openCreate() {
    setEditing(null)
    setForm(emptyForm)
    setShowForm(true)
  }

  function openEdit(book: Book) {
    setDetailBook(null)
    setEditing(book)
    setForm({
      title: book.title,
      author: book.author,
      collection: book.collection,
      volume: book.volume,
      location: book.location,
      note: book.note,
    })
    setShowForm(true)
  }

  function closeForm() {
    setShowForm(false)
    setEditing(null)
    setForm(emptyForm)
  }

  async function handleSave() {
    if (!form.title.trim()) {
      toast.error("Title is required")
      return
    }
    if (!editing) return
    setSaving(true)
    try {
      await updateBook({ ...form, title: form.title.trim(), id: editing.id })
      toast.success("Book updated")
      closeForm()
      refresh(true).catch(() => toast.error("Failed to refresh books"))
    } catch {
      toast.error("Failed to update book")
    } finally {
      setSaving(false)
    }
  }

  function handleCreateMany(booksToCreate: BookFormData[]) {
    const cleaned: Book[] = booksToCreate
      .map((item) => ({ id: "", ...item, title: item.title.trim() }))
      .filter((book) => book.title)
    if (cleaned.length === 0) {
      toast.error("Title is required")
      return
    }
    closeForm()
    setImportPhase({
      step: "preview",
      mode: "create",
      books: cleaned,
      collectionMetadata: [],
    })
  }

  async function handleDelete(book: Book) {
    if (!confirm(`Delete "${book.title}"?`)) return
    try {
      await deleteBook(book.id)
      toast.success("Book deleted")
      setDetailBook(null)
      refresh(true).catch(() => toast.error("Failed to refresh books"))
    } catch {
      toast.error("Failed to delete book")
    }
  }

  function handleExport() {
    exportBooksXlsx(books, collections, collectionNotes)
  }

  function handleImportFilePicked(file: File) {
    parseBooksXlsx(file)
      .then(({ books: parsed, collectionMetadata: importedMetadata }) => {
        setImportPhase({
          step: "preview",
          books: parsed,
          collectionMetadata: importedMetadata,
        })
      })
      .catch((e) => {
        if (e instanceof Error && e.message === "No valid rows found") {
          toast.error("No valid books or collections found in file")
        } else {
          toast.error("Failed to read file — check the format")
        }
      })
  }

  async function runImport(
    booksToImport: Book[],
    metadataEntries: CollectionMetadata[],
    mode: ImportMode = "import"
  ) {
    const booksTotal = booksToImport.length
    const metadataTotal = metadataEntries.length
    if (booksTotal > 0) {
      setImportPhase({
        step: "running",
        mode,
        done: 0,
        total: booksTotal,
        current: "",
        kind: "book",
      })
    }
    const result = await importBooks(
      booksToImport,
      mode === "import" ? books : [],
      (done, _total, current) => {
        setImportPhase({
          step: "running",
          mode,
          done,
          total: booksTotal,
          current,
          kind: "book",
        })
      }
    )
    // Re-fetch collections so newly created collection names have registered ids.
    const cols = await getCollections().catch(() => collections)
    const byId = new Map(cols.map((c) => [c.id, c]))
    const byName = new Map(cols.map((c) => [c.name, c]))
    // Restore collection rows from the import file, with progress feedback.
    let metadataImported = 0
    const failedMetadata: CollectionMetadata[] = []
    for (let i = 0; i < metadataEntries.length; i++) {
      const metadata = metadataEntries[i]
      const displayName = metadata.name || metadata.id || "Collection"
      setImportPhase({
        step: "running",
        mode,
        done: i,
        total: metadataTotal,
        current: displayName,
        kind: "metadata",
      })
      let collection =
        (metadata.id ? byId.get(metadata.id) : undefined) ??
        (metadata.name ? byName.get(metadata.name) : undefined)
      let ok = Boolean(collection)
      if (collection && metadata.name && metadata.name !== collection.name) {
        ok =
          ok &&
          (await renameCollection(collection.id, metadata.name)
            .then(() => true)
            .catch(() => false))
        if (ok) {
          byName.delete(collection.name)
          collection = { ...collection, name: metadata.name }
          byId.set(collection.id, collection)
          byName.set(collection.name, collection)
        }
      }
      if (collection) {
        ok =
          ok &&
          (await setCollectionMetadata(collection.id, {
            note: metadata.note,
            expectedCount: metadata.expectedCount,
            initialVolume: metadata.initialVolume,
          })
            .then(() => true)
            .catch(() => false))
      }
      if (ok) metadataImported++
      else failedMetadata.push(metadata)
    }
    setImportPhase({
      step: "done",
      mode,
      created: result.count,
      updated: result.updated,
      failed: result.failed,
      metadataImported,
      metadataFailed: failedMetadata.length,
      failedBooks: result.failedBooks,
      failedMetadata,
    })
    refresh(true).catch(() => toast.error("Failed to refresh books"))
  }

  async function handleConfirmImport() {
    if (!importPhase || importPhase.step !== "preview") return
    const { books: importBooks_, collectionMetadata: importedMetadata } =
      importPhase
    await runImport(
      importBooks_,
      importedMetadata ?? [],
      importPhase.mode ?? "import"
    )
  }

  async function handleRetryFailedImport() {
    if (!importPhase || importPhase.step !== "done") return
    const { failedBooks, failedMetadata } = importPhase
    await runImport(failedBooks, failedMetadata, importPhase.mode ?? "import")
  }

  function handleImportClose() {
    setImportPhase(null)
  }

  const visibleBookCount =
    standalone.length +
    groups.reduce((total, group) => total + group.books.length, 0)
  const visibleMissingCount = showIncompleteOnly
    ? groups.reduce(
        (total, group) =>
          total +
          missingVolumes(
            collectionByName.get(group.name),
            booksByCollection.get(group.name) ?? []
          ).length,
        0
      )
    : 0
  const visibleCount =
    showIncompleteOnly && missingVolumeMode === "missing-only"
      ? visibleMissingCount
      : visibleBookCount + visibleMissingCount
  const isEmpty = groups.length === 0 && standalone.length === 0

  return (
    <div className="space-y-6">
      <PageHeader
        eyebrow="Library"
        title="Physical books"
        description="Manage your shelf, collections, reading sessions and import/export snapshots."
        actions={
          <div className="flex flex-wrap items-center gap-2">
            {isRefreshing && (
              <span className="flex items-center gap-1.5 text-xs text-muted-foreground">
                <CircleNotch size={14} className="animate-spin" />
                Refreshing…
              </span>
            )}
            <div className="grid grid-cols-2 gap-2 sm:flex">
              <BadgeLike value={books.length} label="Books" />
              <BadgeLike value={groups.length} label="Collections" />
            </div>
          </div>
        }
      />

      <Toolbar>
        <Input
          placeholder="Search title or author..."
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          className="min-w-56 flex-1 sm:max-w-80"
        />
        {collectionOptions.length > 2 && (
          <select
            value={collectionFilter}
            onChange={(e) => setCollectionFilter(e.target.value)}
            className="h-10 rounded-md border border-input bg-background px-3 text-sm outline-none focus:border-ring focus:ring-2 focus:ring-ring/20"
          >
            {collectionOptions.map((c) => (
              <option key={c} value={c}>
                {c}
              </option>
            ))}
          </select>
        )}
        {groups.length > 1 && (
          <Button
            variant="outline"
            size="sm"
            onClick={allExpanded ? collapseAllCollections : expandAllCollections}
          >
            {allExpanded ? (
              <CaretDoubleUpIcon size={16} className="mr-1" />
            ) : (
              <CaretDoubleDownIcon size={16} className="mr-1" />
            )}
            {allExpanded ? "Collapse all" : "Expand all"}
          </Button>
        )}
        <label className="flex h-10 items-center gap-2 rounded-md border border-border px-3 text-xs text-muted-foreground">
          <Switch
            size="sm"
            checked={showIncompleteOnly}
            onCheckedChange={setShowIncompleteOnly}
          />
          Incomplete only
        </label>
        {showIncompleteOnly && (
          <select
            value={missingVolumeMode}
            onChange={(e) =>
              setMissingVolumeMode(e.target.value as MissingVolumeMode)
            }
            className="h-10 rounded-md border border-input bg-background px-3 text-sm outline-none focus:border-ring focus:ring-2 focus:ring-ring/20"
            title="Missing volume display"
          >
            <option value="with-existing">Existing + missing</option>
            <option value="missing-only">Missing only</option>
          </select>
        )}
        <div className="ml-auto flex flex-wrap items-center gap-2">
          <input
            ref={importInputRef}
            type="file"
            accept=".xlsx"
            className="hidden"
            onChange={(e) => {
              const f = e.target.files?.[0]
              if (f) handleImportFilePicked(f)
              e.target.value = ""
            }}
          />
          <Button variant="outline" size="sm" onClick={downloadBooksTemplate}>
            <FileXls size={16} className="mr-1" />
            Template
          </Button>
          <Button
            variant="outline"
            size="sm"
            onClick={() => importInputRef.current?.click()}
            disabled={importPhase !== null}
          >
            <ArrowSquareIn size={16} className="mr-1" />
            Import
          </Button>
          <Button
            variant="outline"
            size="sm"
            onClick={handleExport}
            disabled={books.length === 0}
          >
            <ArrowSquareOut size={16} className="mr-1" />
            Export
          </Button>
          <Button onClick={openCreate} size="sm">
            <Plus size={16} className="mr-1" />
            Add Book
          </Button>
        </div>
      </Toolbar>

      {/* Stats */}
      <div className="grid gap-3 sm:grid-cols-3">
        <MetricCard
          label="Visible"
          value={initialLoading ? "…" : visibleCount}
          detail={`${books.length} total books`}
        />
        <MetricCard
          label="Collections"
          value={groups.length}
          detail={
            showIncompleteOnly
              ? `${incompleteCollections.size} incomplete`
              : "Grouped series and shelves"
          }
        />
        <MetricCard
          label="Standalone"
          value={standalone.length}
          detail="Books without collection"
        />
      </div>

      {/* Content */}
      <div
        className={`transition-opacity duration-200 ${isRefreshing ? "opacity-50" : "opacity-100"}`}
      >
        {initialLoading ? (
          <div className="space-y-3">
            {[...Array(3)].map((_, i) => (
              <div key={i} className="h-28 animate-pulse rounded-lg bg-muted" />
            ))}
          </div>
        ) : isEmpty ? (
          <EmptyState
            icon={<BookOpen size={40} weight="thin" />}
            title={
              books.length === 0
                ? "No books registered yet. Add your first book!"
                : "No books match your search."
            }
          />
        ) : (
          <div className="space-y-4">
            {/* Collection groups */}
            {groups.map((group) => {
              const expanded = expandedCollections.has(group.name)
              const collection = collectionByName.get(group.name)
              const collId = collection?.id
              const note = collId ? (collectionNotes[collId] ?? "") : ""
              const isEditingNote = editingNote === group.name
              const expectedCount = collection?.expectedCount ?? 0
              const initialVolume = collection?.initialVolume ?? 0
              const collectionBookCount =
                collectionBookCounts.get(group.name) ?? group.books.length
              const allCollectionBooks = booksByCollection.get(group.name) ?? []
              const collectionRows = buildCollectionRows({
                visibleBooks: group.books,
                allCollectionBooks,
                collection,
                missingVolumeMode,
                showMissingVolumes: showIncompleteOnly,
              })
              const canCalculateMissing =
                showIncompleteOnly && expectedCount > 0 && initialVolume > 0
              return (
                <div key={group.name} className="flat-panel overflow-hidden">
                  <button
                    onClick={() => toggleCollection(group.name)}
                    className={`flex w-full items-center gap-3 px-5 py-4 text-left transition-colors ${
                      expanded
                        ? "border-b border-border bg-muted/40"
                        : "hover:bg-muted/45"
                    }`}
                  >
                    {expanded ? (
                      <CaretDown
                        size={12}
                        className="shrink-0 text-muted-foreground"
                      />
                    ) : (
                      <CaretRight
                        size={12}
                        className="shrink-0 text-muted-foreground"
                      />
                    )}
                    <span className="flex-1 text-base font-semibold tracking-tight">
                      {group.name}
                    </span>
                    {initialVolume > 0 && (
                      <span className="text-xs text-muted-foreground">
                        Vol. {initialVolume}
                      </span>
                    )}
                    <span className="text-xs text-muted-foreground">
                      {expectedCount > 0
                        ? `${collectionBookCount} / ${expectedCount}`
                        : collectionBookCount}{" "}
                      {collectionBookCount === 1 ? "book" : "books"}
                    </span>
                    <span
                      role="button"
                      onClick={(e) => {
                        e.stopPropagation()
                        handleSetInitialVolume(group.name)
                      }}
                      className="ml-1 flex size-5 shrink-0 items-center justify-center rounded-md text-[10px] font-semibold text-muted-foreground hover:bg-muted hover:text-foreground"
                      title="Edit initial volume"
                    >
                      V
                    </span>
                    <span
                      role="button"
                      onClick={(e) => {
                        e.stopPropagation()
                        handleSetExpectedCount(group.name)
                      }}
                      className="ml-1 shrink-0 rounded-md p-1 text-muted-foreground hover:bg-muted hover:text-foreground"
                      title="Edit expected total"
                    >
                      <Hash size={12} />
                    </span>
                    <span
                      role="button"
                      onClick={(e) => {
                        e.stopPropagation()
                        startEditNote(group.name)
                      }}
                      className="ml-1 shrink-0 rounded-md p-1 text-muted-foreground hover:bg-muted hover:text-foreground"
                      title="Edit collection note"
                    >
                      <NotePencilIcon size={12} />
                    </span>
                    <span
                      role="button"
                      onClick={(e) => {
                        e.stopPropagation()
                        handleRenameCollection(group.name)
                      }}
                      className="shrink-0 rounded-md p-1 text-muted-foreground hover:bg-muted hover:text-foreground"
                      title="Rename collection"
                    >
                      <PencilSimple size={12} />
                    </span>
                  </button>

                  {/* Collection note row */}
                  {isEditingNote ? (
                    <div
                      className="flex items-center gap-2 border-b border-border bg-muted/30 px-5 py-3"
                      onClick={(e) => e.stopPropagation()}
                    >
                      <Input
                        autoFocus
                        value={noteDraft}
                        onChange={(e) => setNoteDraft(e.target.value)}
                        onKeyDown={(e) => {
                          if (e.key === "Enter") saveNote(group.name)
                          if (e.key === "Escape") setEditingNote(null)
                        }}
                        placeholder="Add a note for this collection…"
                        className="h-7 flex-1 text-xs"
                      />
                      <Button
                        size="sm"
                        className="h-7 text-xs"
                        onClick={() => saveNote(group.name)}
                      >
                        Save
                      </Button>
                      <Button
                        size="sm"
                        variant="ghost"
                        className="h-7 text-xs"
                        onClick={() => setEditingNote(null)}
                      >
                        <X size={12} />
                      </Button>
                    </div>
                  ) : note ? (
                    <div className="flex items-center gap-2 border-b border-border bg-muted/25 px-5 py-2 text-muted-foreground">
                      <span className="flex-1 text-xs italic">{note}</span>
                      <button
                        onClick={(e) => {
                          e.stopPropagation()
                          deleteNote(group.name)
                        }}
                        className="shrink-0 rounded p-0.5 hover:bg-muted hover:text-destructive"
                        title="Delete note"
                      >
                        <Trash size={11} />
                      </button>
                    </div>
                  ) : null}

                  {expanded && (
                    <div className="divide-y divide-border bg-card">
                      {collectionRows.length > 0 ? (
                        collectionRows.map((row) =>
                          row.kind === "book" ? (
                            <BookRow
                              key={row.key}
                              book={row.book}
                              onSelect={setDetailBook}
                              onEdit={openEdit}
                              onDelete={handleDelete}
                            />
                          ) : (
                            <MissingVolumeRow
                              key={row.key}
                              volume={row.volume}
                            />
                          )
                        )
                      ) : (
                        <div className="px-4 py-3 text-xs text-muted-foreground">
                          {canCalculateMissing
                            ? "No missing volumes in the configured range."
                            : "Set an initial volume to calculate missing volumes."}
                        </div>
                      )}
                    </div>
                  )}
                </div>
              )
            })}

            {/* Standalone books (no collection) */}
            {standalone.length > 0 && (
              <div>
                {groups.length > 0 && (
                  <div className="flex items-center gap-3 py-1">
                    <Separator className="flex-1" />
                    <span className="text-xs text-muted-foreground">
                      No collection
                    </span>
                    <Separator className="flex-1" />
                  </div>
                )}
                <div className="flat-panel divide-y divide-border overflow-hidden">
                  {standalone.map((book) => (
                    <BookRow
                      key={book.id}
                      book={book}
                      onSelect={setDetailBook}
                      onEdit={openEdit}
                      onDelete={handleDelete}
                    />
                  ))}
                </div>
              </div>
            )}
          </div>
        )}
      </div>

      <BookDetailDialog
        book={detailBook}
        onClose={() => setDetailBook(null)}
        onDelete={handleDelete}
        onEdit={openEdit}
        onReadings={(book) => {
          setReadingsBook(book)
          setDetailBook(null)
        }}
      />

      {/* Readings Dialog */}
      {readingsBook && (
        <ReadingsDialog
          book={readingsBook}
          open={!!readingsBook}
          onOpenChange={(o) => !o && setReadingsBook(null)}
        />
      )}

      <BookFormDialog
        open={showForm}
        editing={editing}
        form={form}
        saving={saving}
        existingCollections={existingCollections}
        onClose={closeForm}
        onFormChange={setForm}
        onSave={handleSave}
        onCreateMany={handleCreateMany}
      />

      <ImportDialog
        phase={importPhase}
        onConfirm={handleConfirmImport}
        onRetry={handleRetryFailedImport}
        onClose={handleImportClose}
      />
    </div>
  )
}
