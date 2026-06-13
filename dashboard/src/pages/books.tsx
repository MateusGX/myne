import { useCallback, useEffect, useMemo, useRef, useState } from "react"
import {
  BookOpen,
  CaretDown,
  CaretRight,
  ArrowSquareIn,
  ArrowSquareOut,
  FileXls,
  NotePencilIcon,
  PencilSimple,
  Plus,
  SpinnerIcon,
  Trash,
  X,
} from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Separator } from "@/components/ui/separator"
import { EmptyState, MetricCard, PageHeader, Toolbar } from "@/components/dashboard-layout"
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { BadgeLike } from "@/components/books/BookBadge"
import { BookRow } from "@/components/books/BookRow"
import { DetailField } from "@/components/books/DetailField"
import { ReadingsDialog } from "@/components/books/ReadingsDialog"
import { ImportDialog, type ImportPhase } from "@/components/books/ImportDialog"
import {
  createBook,
  deleteBook,
  getBooks,
  getCollections,
  renameCollection,
  deleteCollectionNote,
  getCollectionNote,
  importBooks,
  setCollectionNote,
  updateBook,
  type Book,
  type BookFormData,
  type Collection,
} from "@/lib/api"
import { downloadBooksTemplate, exportBooksXlsx, parseBooksXlsx } from "@/lib/booksXlsx"
import { MAX_COLLECTION_NAME_BYTES, truncateUtf8 } from "@/lib/utils"

const emptyForm: BookFormData = {
  title: "",
  author: "",
  collection: "",
  volume: "",
  location: "",
  notes: "",
}

type CollectionGroup = { name: string; books: Book[] }

export function BooksPage() {
  const [books, setBooks] = useState<Book[]>([])
  const [loading, setLoading] = useState(true)
  const [search, setSearch] = useState("")
  const [collectionFilter, setCollectionFilter] = useState("All")
  const [expandedCollections, setExpandedCollections] = useState<Set<string>>(new Set())
  const [showForm, setShowForm] = useState(false)
  const [editing, setEditing] = useState<Book | null>(null)
  const [form, setForm] = useState<BookFormData>(emptyForm)
  const [saving, setSaving] = useState(false)
  const [importPhase, setImportPhase] = useState<ImportPhase | null>(null)
  const [detailBook, setDetailBook] = useState<Book | null>(null)
  const [readingsBook, setReadingsBook] = useState<Book | null>(null)
  const importInputRef = useRef<HTMLInputElement>(null)
  const [collections, setCollections] = useState<Collection[]>([])
  const [collectionNotes, setCollectionNotes] = useState<Record<string, string>>({})
  const [notesLoading, setNotesLoading] = useState(false)
  const [editingNote, setEditingNote] = useState<string | null>(null)
  const [noteDraft, setNoteDraft] = useState("")

  const nameToId = useMemo(() => new Map(collections.map((c) => [c.name, c.id])), [collections])

  const loadCollectionNotes = useCallback((bookList: Book[], cols: Collection[]) => {
    const byName = new Map(cols.map((c) => [c.name, c.id]))
    const ids = Array.from(
      new Set(
        bookList
          .map((b) => byName.get(b.collection))
          .filter((id): id is string => Boolean(id)),
      ),
    )
    if (ids.length === 0) {
      setCollectionNotes({})
      setNotesLoading(false)
      return
    }
    setNotesLoading(true)
    Promise.all(
      ids.map((id) =>
        getCollectionNote(id)
          .then((r) => [id, r.note] as [string, string])
          .catch(() => [id, ""] as [string, string]),
      ),
    )
      .then((entries) => setCollectionNotes(Object.fromEntries(entries)))
      .finally(() => setNotesLoading(false))
  }, [])

  const load = useCallback(() => {
    setLoading(true)
    Promise.all([getBooks(), getCollections()])
      .then(([data, cols]) => {
        setBooks(data)
        setCollections(cols)
        const colSet = new Set(data.map((b) => b.collection).filter(Boolean))
        setExpandedCollections(colSet)
        loadCollectionNotes(data, cols)
      })
      .catch(() => toast.error("Failed to load books"))
      .finally(() => setLoading(false))
  }, [loadCollectionNotes])

  useEffect(() => {
    load()
  }, [load])

  const collectionOptions = useMemo(
    () => ["All", ...Array.from(new Set(books.map((b) => b.collection).filter(Boolean))).sort()],
    [books],
  )
  const existingCollections = useMemo(
    () => Array.from(new Set(books.map((b) => b.collection).filter(Boolean))).sort(),
    [books],
  )

  const filtered = useMemo(
    () =>
      books.filter((b) => {
        const matchCol = collectionFilter === "All" || b.collection === collectionFilter
        const q = search.toLowerCase()
        const matchSearch =
          !q || b.title.toLowerCase().includes(q) || b.author.toLowerCase().includes(q)
        return matchCol && matchSearch
      }),
    [books, collectionFilter, search],
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
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([name, books]) => ({
        name,
        books: [...books].sort((a, b) => a.title.localeCompare(b.title)),
      }))
    return { groups, standalone: standalone.sort((a, b) => a.title.localeCompare(b.title)) }
  }, [filtered])

  function toggleCollection(name: string) {
    setExpandedCollections((prev) => {
      const next = new Set(prev)
      if (next.has(name)) next.delete(name)
      else next.add(name)
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
      setCollectionNotes((prev) => ({ ...prev, [id]: noteDraft }))
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
      setCollectionNotes((prev) => {
        const next = { ...prev }
        delete next[id]
        return next
      })
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
      toast.error(`Collection name too long, truncated to ${MAX_COLLECTION_NAME_BYTES} bytes`)
    }
    if (newName === name) return
    try {
      await renameCollection(id, newName)
      toast.success("Collection renamed")
      load()
    } catch {
      toast.error("Failed to rename collection")
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
      notes: book.notes,
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
    setSaving(true)
    try {
      if (editing) {
        await updateBook({ ...form, id: editing.id })
        toast.success("Book updated")
      } else {
        await createBook(form)
        toast.success("Book added")
      }
      closeForm()
      load()
    } catch {
      toast.error(editing ? "Failed to update book" : "Failed to add book")
    } finally {
      setSaving(false)
    }
  }

  async function handleDelete(book: Book) {
    if (!confirm(`Delete "${book.title}"?`)) return
    try {
      await deleteBook(book.id)
      toast.success("Book deleted")
      setDetailBook(null)
      load()
    } catch {
      toast.error("Failed to delete book")
    }
  }

  function handleExport() {
    // Build a map of collection name → note for all collections that have notes.
    const idToName = new Map(collections.map((c) => [c.id, c.name]))
    const notesMap: Record<string, string> = {}
    for (const [id, note] of Object.entries(collectionNotes)) {
      if (note) {
        const name = idToName.get(id)
        if (name) notesMap[name] = note
      }
    }
    exportBooksXlsx(books, notesMap)
  }

  function handleImportFilePicked(file: File) {
    parseBooksXlsx(file)
      .then(({ books: parsed, collectionNotes: importedNotes }) => {
        const cleaned: Book[] = parsed.map((item) => ({ id: "", ...item }))
        setImportPhase({ step: "preview", books: cleaned, collectionNotes: importedNotes })
      })
      .catch((e) => {
        if (e instanceof Error && e.message === "No valid books found") {
          toast.error("No valid books found in file")
        } else {
          toast.error("Failed to read file — check the format")
        }
      })
  }

  async function runImport(booksToImport: Book[], noteEntries: [string, string][]) {
    const booksTotal = booksToImport.length
    const notesTotal = noteEntries.length
    if (booksTotal > 0) {
      setImportPhase({ step: "running", done: 0, total: booksTotal, current: "", kind: "book" })
    }
    const result = await importBooks(booksToImport, (done, _total, current) => {
      setImportPhase({ step: "running", done, total: booksTotal, current, kind: "book" })
    })
    // Re-fetch collections so newly created collection names have registered ids.
    const cols = await getCollections().catch(() => collections)
    const byName = new Map(cols.map((c) => [c.name, c.id]))
    // Restore collection notes from the import file, with progress feedback.
    let notesImported = 0
    const failedNotes: Record<string, string> = {}
    for (let i = 0; i < noteEntries.length; i++) {
      const [name, note] = noteEntries[i]
      setImportPhase({ step: "running", done: i, total: notesTotal, current: name, kind: "note" })
      const id = byName.get(name)
      if (id && (await setCollectionNote(id, String(note)).then(() => true).catch(() => false))) {
        notesImported++
      } else {
        failedNotes[name] = note
      }
    }
    setImportPhase({
      step: "done",
      created: result.count,
      failed: result.failed,
      notesImported,
      notesFailed: Object.keys(failedNotes).length,
      failedBooks: result.failedBooks,
      failedNotes,
    })
    load()
  }

  async function handleConfirmImport() {
    if (!importPhase || importPhase.step !== "preview") return
    const { books: importBooks_, collectionNotes: importedNotes } = importPhase
    await runImport(importBooks_, Object.entries(importedNotes ?? {}))
  }

  async function handleRetryFailedImport() {
    if (!importPhase || importPhase.step !== "done") return
    const { failedBooks, failedNotes } = importPhase
    await runImport(failedBooks, Object.entries(failedNotes))
  }

  function handleImportClose() {
    setImportPhase(null)
  }

  const isEmpty = filtered.length === 0

  return (
    <div className="space-y-6">
      <PageHeader
        eyebrow="Library"
        title="Physical books"
        description="Manage your shelf, collections, reading sessions and import/export snapshots."
        actions={
          <div className="grid grid-cols-2 gap-2 sm:flex">
          <BadgeLike value={books.length} label="Books" />
          <BadgeLike value={groups.length} label="Collections" />
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
        <MetricCard label="Visible" value={loading ? "…" : filtered.length} detail={`${books.length} total books`} />
        <MetricCard label="Collections" value={groups.length} detail="Grouped series and shelves" />
        <MetricCard label="Standalone" value={standalone.length} detail="Books without collection" />
      </div>

      {/* Content */}
      {loading ? (
        <div className="space-y-3">
          {[...Array(3)].map((_, i) => (
            <div key={i} className="bg-muted h-28 animate-pulse rounded-lg" />
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
            const collId = nameToId.get(group.name)
            const note = collId ? collectionNotes[collId] ?? "" : ""
            const isEditingNote = editingNote === group.name
            return (
              <div key={group.name} className="flat-panel overflow-hidden">
                <button
                  onClick={() => toggleCollection(group.name)}
                  className={`flex w-full items-center gap-3 px-5 py-4 text-left transition-colors ${
                    expanded
                      ? "border-border bg-muted/40 border-b"
                      : "hover:bg-muted/45"
                  }`}
                >
                  {expanded ? (
                    <CaretDown size={12} className="text-muted-foreground shrink-0" />
                  ) : (
                    <CaretRight size={12} className="text-muted-foreground shrink-0" />
                  )}
                  <span className="flex-1 text-base font-semibold tracking-tight">{group.name}</span>
                  <span className="text-muted-foreground text-xs">
                    {group.books.length} {group.books.length === 1 ? "book" : "books"}
                  </span>
                  <span
                    role="button"
                    onClick={(e) => { e.stopPropagation(); startEditNote(group.name) }}
                    className="text-muted-foreground hover:text-foreground ml-1 shrink-0 rounded-md p-1 hover:bg-muted"
                    title="Edit collection note"
                  >
                    <NotePencilIcon size={12} />
                  </span>
                  <span
                    role="button"
                    onClick={(e) => { e.stopPropagation(); handleRenameCollection(group.name) }}
                    className="text-muted-foreground hover:text-foreground shrink-0 rounded-md p-1 hover:bg-muted"
                    title="Rename collection"
                  >
                    <PencilSimple size={12} />
                  </span>
                </button>

                {/* Collection note row */}
                {isEditingNote ? (
                  <div
                    className="border-border flex items-center gap-2 border-b bg-muted/30 px-5 py-3"
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
                    <Button size="sm" className="h-7 text-xs" onClick={() => saveNote(group.name)}>
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
                  <div className="border-border text-muted-foreground border-b bg-muted/25 flex items-center gap-2 px-5 py-2">
                    <span className="flex-1 text-xs italic">{note}</span>
                    <button
                      onClick={(e) => { e.stopPropagation(); deleteNote(group.name) }}
                      className="shrink-0 rounded p-0.5 hover:bg-muted hover:text-destructive"
                      title="Delete note"
                    >
                      <Trash size={11} />
                    </button>
                  </div>
                ) : notesLoading ? (
                  <div className="border-border text-muted-foreground border-b bg-muted/25 flex items-center gap-2 px-5 py-2">
                    <SpinnerIcon size={11} className="animate-spin" />
                    <span className="text-xs italic">Checking for collection note…</span>
                  </div>
                ) : null}

                {expanded && (
                  <div className="divide-border divide-y bg-card">
                    {group.books.map((book) => (
                      <BookRow
                        key={book.id}
                        book={book}
                        onSelect={setDetailBook}
                        onEdit={openEdit}
                        onDelete={handleDelete}
                      />
                    ))}
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
                  <span className="text-muted-foreground text-xs">No collection</span>
                  <Separator className="flex-1" />
                </div>
              )}
              <div className="flat-panel divide-border divide-y overflow-hidden">
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

      {/* Book Detail Dialog */}
      <Dialog open={!!detailBook} onOpenChange={(o) => !o && setDetailBook(null)}>
        <DialogContent className="max-w-sm">
          <DialogHeader>
            <DialogTitle className="leading-snug">
              {detailBook?.title}
              {detailBook?.volume && (
                <span className="text-muted-foreground ml-2 text-sm font-normal">
                  {detailBook.volume}
                </span>
              )}
            </DialogTitle>
          </DialogHeader>
          {detailBook && (
            <div className="space-y-3">
              <DetailField label="Author" value={detailBook.author} />
              <DetailField label="Collection" value={detailBook.collection} />
              <DetailField label="Location" value={detailBook.location} />
              <DetailField label="Notes" value={detailBook.notes} />
            </div>
          )}
          <DialogFooter className="gap-2 sm:justify-between">
            <Button
              variant="destructive"
              size="sm"
              onClick={() => detailBook && handleDelete(detailBook)}
            >
              <Trash size={14} className="mr-1" />
              Delete
            </Button>
            <div className="flex gap-2">
              <Button
                variant="outline"
                size="sm"
                onClick={() => {
                  setReadingsBook(detailBook)
                  setDetailBook(null)
                }}
              >
                <BookOpen size={14} className="mr-1" />
                Readings
              </Button>
              <Button size="sm" onClick={() => detailBook && openEdit(detailBook)}>
                <PencilSimple size={14} className="mr-1" />
                Edit
              </Button>
            </div>
          </DialogFooter>
        </DialogContent>
      </Dialog>

      {/* Readings Dialog */}
      {readingsBook && (
        <ReadingsDialog
          book={readingsBook}
          open={!!readingsBook}
          onOpenChange={(o) => !o && setReadingsBook(null)}
        />
      )}

      {/* Create / Edit Dialog */}
      <Dialog open={showForm} onOpenChange={(o) => !o && closeForm()}>
        <DialogContent className="max-w-md">
          <DialogHeader>
            <DialogTitle>{editing ? "Edit Book" : "Add Book"}</DialogTitle>
          </DialogHeader>
          <div className="space-y-3">
            <div className="space-y-1">
              <Label htmlFor="book-title">Title *</Label>
              <Input
                id="book-title"
                value={form.title}
                onChange={(e) => setForm({ ...form, title: e.target.value })}
                onKeyDown={(e) => e.key === "Enter" && handleSave()}
                placeholder="Book title"
                autoFocus
              />
            </div>
            <div className="space-y-1">
              <Label htmlFor="book-author">Author</Label>
              <Input
                id="book-author"
                value={form.author}
                onChange={(e) => setForm({ ...form, author: e.target.value })}
                placeholder="Author name"
              />
            </div>
            <div className="space-y-1">
              <Label htmlFor="book-collection">Collection / Series</Label>
              <Input
                id="book-collection"
                value={form.collection}
                onChange={(e) =>
                  setForm({ ...form, collection: truncateUtf8(e.target.value, MAX_COLLECTION_NAME_BYTES) })
                }
                placeholder="e.g. Middle Earth, Science Fiction"
                list="collections-list"
              />
              <datalist id="collections-list">
                {existingCollections.map((c) => (
                  <option key={c} value={c} />
                ))}
              </datalist>
            </div>
            <div className="grid grid-cols-2 gap-3">
              <div className="space-y-1">
                <Label htmlFor="book-volume">Volume</Label>
                <Input
                  id="book-volume"
                  value={form.volume}
                  onChange={(e) => setForm({ ...form, volume: e.target.value })}
                  placeholder="e.g. Vol. 1, Book 2"
                />
              </div>
              <div className="space-y-1">
                <Label htmlFor="book-location">Storage Location</Label>
                <Input
                  id="book-location"
                  value={form.location}
                  onChange={(e) => setForm({ ...form, location: e.target.value })}
                  placeholder="e.g. Shelf A-3, Box 2"
                />
              </div>
            </div>
            <div className="space-y-1">
              <Label htmlFor="book-notes">Notes</Label>
              <Input
                id="book-notes"
                value={form.notes}
                onChange={(e) => setForm({ ...form, notes: e.target.value })}
                placeholder="Optional notes"
              />
            </div>

          </div>
          <DialogFooter>
            <Button variant="outline" onClick={closeForm} disabled={saving}>
              Cancel
            </Button>
            <Button onClick={handleSave} disabled={saving}>
              {saving ? "Saving…" : editing ? "Save Changes" : "Add Book"}
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>

      <ImportDialog
        phase={importPhase}
        onConfirm={handleConfirmImport}
        onRetry={handleRetryFailedImport}
        onClose={handleImportClose}
      />
    </div>
  )
}
