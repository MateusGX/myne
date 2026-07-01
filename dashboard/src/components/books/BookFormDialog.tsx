import { useState } from "react"
import { PencilSimple, Plus, Trash } from "@phosphor-icons/react"
import { Button } from "@/components/ui/button"
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { type Book, type BookFormData } from "@/lib/api"
import { MAX_COLLECTION_NAME_BYTES, truncateUtf8 } from "@/lib/utils"

interface BookFormDialogProps {
  open: boolean
  editing: Book | null
  form: BookFormData
  saving: boolean
  existingCollections: string[]
  onClose: () => void
  onFormChange: (form: BookFormData) => void
  onSave: () => void
  onCreateMany: (books: BookFormData[]) => void
}

export function BookFormDialog({
  open,
  editing,
  form,
  saving,
  existingCollections,
  onClose,
  onFormChange,
  onSave,
  onCreateMany,
}: BookFormDialogProps) {
  const [queuedBooks, setQueuedBooks] = useState<BookFormData[]>([])
  const [editingQueuedIndex, setEditingQueuedIndex] = useState<number | null>(
    null
  )
  const isEditing = Boolean(editing)
  const isEditingQueuedBook = editingQueuedIndex !== null
  const draftHasTitle = form.title.trim().length > 0
  const createCount =
    queuedBooks.length + (draftHasTitle && !isEditingQueuedBook ? 1 : 0)

  function normalizedDraft() {
    return { ...form, title: form.title.trim() }
  }

  function resetDraft() {
    onFormChange({
      title: "",
      author: "",
      collection: form.collection,
      volume: "",
      location: form.location,
      note: "",
    })
  }

  function handleClose() {
    setQueuedBooks([])
    setEditingQueuedIndex(null)
    onClose()
  }

  function addDraftToQueue() {
    if (!draftHasTitle) return
    if (editingQueuedIndex !== null) {
      setQueuedBooks((books) =>
        books.map((book, index) =>
          index === editingQueuedIndex ? normalizedDraft() : book
        )
      )
      setEditingQueuedIndex(null)
      resetDraft()
      return
    }
    setQueuedBooks((books) => [...books, normalizedDraft()])
    resetDraft()
  }

  function editQueuedBook(index: number) {
    const book = queuedBooks[index]
    if (!book) return
    setEditingQueuedIndex(index)
    onFormChange(book)
  }

  function removeQueuedBook(index: number) {
    setQueuedBooks((books) => books.filter((_, i) => i !== index))
    if (editingQueuedIndex === index) {
      setEditingQueuedIndex(null)
      resetDraft()
    } else if (editingQueuedIndex !== null && index < editingQueuedIndex) {
      setEditingQueuedIndex(editingQueuedIndex - 1)
    }
  }

  function cancelQueuedEdit() {
    setEditingQueuedIndex(null)
    resetDraft()
  }

  function handlePrimaryAction() {
    if (isEditing) {
      onSave()
      return
    }
    const booksToCreate =
      editingQueuedIndex !== null
        ? queuedBooks.map((book, index) =>
            index === editingQueuedIndex ? normalizedDraft() : book
          )
        : draftHasTitle
          ? [...queuedBooks, normalizedDraft()]
          : queuedBooks
    if (booksToCreate.length === 0) return
    setQueuedBooks([])
    setEditingQueuedIndex(null)
    onCreateMany(booksToCreate)
  }

  return (
    <Dialog open={open} onOpenChange={(o) => !o && handleClose()}>
      <DialogContent className="flex max-h-[90vh] max-w-4xl flex-col overflow-hidden p-0">
        <DialogHeader className="border-b border-border px-5 py-4">
          <DialogTitle>{isEditing ? "Edit Book" : "Add Books"}</DialogTitle>
        </DialogHeader>
        <div
          className={`grid min-h-0 flex-1 ${
            !isEditing && queuedBooks.length > 0
              ? "md:grid-cols-[18rem_minmax(0,1fr)]"
              : "grid-cols-1"
          }`}
        >
          {!isEditing && queuedBooks.length > 0 && (
            <aside className="flex min-h-0 flex-col border-b border-border bg-muted/20 md:border-r md:border-b-0">
              <div className="flex items-center justify-between border-b border-border px-4 py-3">
                <div>
                  <p className="text-xs font-semibold text-muted-foreground uppercase">
                    Queue
                  </p>
                  <p className="text-xs text-muted-foreground">
                    {queuedBooks.length}{" "}
                    {queuedBooks.length === 1 ? "book" : "books"}
                  </p>
                </div>
              </div>
              <div className="max-h-56 min-h-0 divide-y divide-border overflow-y-auto md:max-h-none">
                {queuedBooks.map((book, index) => {
                  const selected = editingQueuedIndex === index
                  return (
                    <div
                      key={`${book.title}-${index}`}
                      className={`flex min-w-0 items-center gap-2 px-4 py-3 transition-colors ${
                        selected ? "bg-accent/60" : "hover:bg-muted/55"
                      }`}
                    >
                      <button
                        type="button"
                        className="min-w-0 flex-1 text-left"
                        onClick={() => editQueuedBook(index)}
                      >
                        <p className="truncate text-sm font-medium">
                          {book.title}
                        </p>
                        {(book.author || book.collection || book.volume) && (
                          <p className="truncate text-xs text-muted-foreground">
                            {[book.author, book.collection, book.volume]
                              .filter(Boolean)
                              .join(" · ")}
                          </p>
                        )}
                      </button>
                      <Button
                        variant="ghost"
                        size="icon-xs"
                        title="Edit queued book"
                        onClick={() => editQueuedBook(index)}
                      >
                        <PencilSimple size={13} />
                      </Button>
                      <Button
                        variant="ghost"
                        size="icon-xs"
                        title="Remove queued book"
                        onClick={() => removeQueuedBook(index)}
                      >
                        <Trash size={13} />
                      </Button>
                    </div>
                  )
                })}
              </div>
            </aside>
          )}

          <div className="min-h-0 overflow-y-auto p-5">
            <div className="space-y-4">
              {isEditingQueuedBook && (
                <div className="flex items-center justify-between gap-3 rounded-md border border-border bg-muted/25 px-3 py-2">
                  <span className="truncate text-xs font-medium text-muted-foreground">
                    Editing queued book #{editingQueuedIndex + 1}
                  </span>
                  <Button
                    type="button"
                    variant="ghost"
                    size="xs"
                    onClick={cancelQueuedEdit}
                  >
                    Cancel edit
                  </Button>
                </div>
              )}

              <div className="space-y-1">
                <Label htmlFor="book-title">Title *</Label>
                <Input
                  id="book-title"
                  value={form.title}
                  onChange={(e) =>
                    onFormChange({ ...form, title: e.target.value })
                  }
                  onKeyDown={(e) => {
                    if (e.key === "Enter") {
                      if (isEditing) onSave()
                      else addDraftToQueue()
                    }
                  }}
                  placeholder="Book title"
                  autoFocus
                />
              </div>
              <div className="space-y-1">
                <Label htmlFor="book-author">Author</Label>
                <Input
                  id="book-author"
                  value={form.author}
                  onChange={(e) =>
                    onFormChange({ ...form, author: e.target.value })
                  }
                  placeholder="Author name"
                />
              </div>
              <div className="space-y-1">
                <Label htmlFor="book-collection">Collection / Series</Label>
                <Input
                  id="book-collection"
                  value={form.collection}
                  onChange={(e) =>
                    onFormChange({
                      ...form,
                      collection: truncateUtf8(
                        e.target.value,
                        MAX_COLLECTION_NAME_BYTES
                      ),
                    })
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
              <div className="grid gap-3 sm:grid-cols-2">
                <div className="space-y-1">
                  <Label htmlFor="book-volume">Volume</Label>
                  <Input
                    id="book-volume"
                    value={form.volume}
                    onChange={(e) =>
                      onFormChange({ ...form, volume: e.target.value })
                    }
                    placeholder="e.g. Vol. 1, Book 2"
                  />
                </div>
                <div className="space-y-1">
                  <Label htmlFor="book-location">Storage Location</Label>
                  <Input
                    id="book-location"
                    value={form.location}
                    onChange={(e) =>
                      onFormChange({ ...form, location: e.target.value })
                    }
                    placeholder="e.g. Shelf A-3, Box 2"
                  />
                </div>
              </div>
              <div className="space-y-1">
                <Label htmlFor="book-note">Note</Label>
                <Input
                  id="book-note"
                  value={form.note}
                  onChange={(e) =>
                    onFormChange({ ...form, note: e.target.value })
                  }
                  placeholder="Optional note"
                />
              </div>
              {!isEditing && (
                <div className="flex flex-wrap gap-2">
                  <Button
                    type="button"
                    variant="outline"
                    className="flex-1"
                    onClick={addDraftToQueue}
                    disabled={!draftHasTitle || saving}
                  >
                    <Plus size={14} className="mr-1" />
                    {isEditingQueuedBook ? "Update Queued Book" : "Add Another"}
                  </Button>
                  {isEditingQueuedBook && (
                    <Button
                      type="button"
                      variant="ghost"
                      onClick={cancelQueuedEdit}
                      disabled={saving}
                    >
                      Cancel edit
                    </Button>
                  )}
                </div>
              )}
            </div>
          </div>
        </div>
        <DialogFooter className="border-t border-border px-5 py-4">
          <Button variant="outline" onClick={handleClose} disabled={saving}>
            Cancel
          </Button>
          <Button
            onClick={handlePrimaryAction}
            disabled={
              saving ||
              (!isEditing &&
                (createCount === 0 || (isEditingQueuedBook && !draftHasTitle)))
            }
          >
            {saving
              ? "Saving…"
              : isEditing
                ? "Save Changes"
                : `Create ${createCount} ${createCount === 1 ? "Book" : "Books"}`}
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}
