import { useState } from "react"
import { Plus, Trash } from "@phosphor-icons/react"
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
  const isEditing = Boolean(editing)
  const draftHasTitle = form.title.trim().length > 0
  const createCount = queuedBooks.length + (draftHasTitle ? 1 : 0)

  function resetDraft() {
    onFormChange({
      title: "",
      author: "",
      collection: form.collection,
      volume: "",
      location: form.location,
      notes: "",
    })
  }

  function handleClose() {
    setQueuedBooks([])
    onClose()
  }

  function addDraftToQueue() {
    if (!draftHasTitle) return
    setQueuedBooks((books) => [...books, { ...form, title: form.title.trim() }])
    resetDraft()
  }

  function removeQueuedBook(index: number) {
    setQueuedBooks((books) => books.filter((_, i) => i !== index))
  }

  function handlePrimaryAction() {
    if (isEditing) {
      onSave()
      return
    }
    const booksToCreate = draftHasTitle
      ? [...queuedBooks, { ...form, title: form.title.trim() }]
      : queuedBooks
    if (booksToCreate.length === 0) return
    setQueuedBooks([])
    onCreateMany(booksToCreate)
  }

  return (
    <Dialog open={open} onOpenChange={(o) => !o && handleClose()}>
      <DialogContent className="flex max-h-[85vh] max-w-2xl flex-col">
        <DialogHeader>
          <DialogTitle>{isEditing ? "Edit Book" : "Add Books"}</DialogTitle>
        </DialogHeader>
        <div className="min-h-0 space-y-4 overflow-y-auto pr-1">
          {!isEditing && queuedBooks.length > 0 && (
            <div className="rounded-md border border-border">
              <div className="flex items-center justify-between border-b border-border px-3 py-2">
                <span className="text-xs font-semibold text-muted-foreground uppercase">
                  Queue
                </span>
                <span className="text-xs text-muted-foreground">
                  {queuedBooks.length}{" "}
                  {queuedBooks.length === 1 ? "book" : "books"}
                </span>
              </div>
              <div className="max-h-44 divide-y divide-border overflow-y-auto">
                {queuedBooks.map((book, index) => (
                  <div
                    key={`${book.title}-${index}`}
                    className="flex min-w-0 items-center gap-3 px-3 py-2"
                  >
                    <div className="min-w-0 flex-1">
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
                    </div>
                    <Button
                      variant="ghost"
                      size="sm"
                      className="h-7 w-7 p-0"
                      onClick={() => removeQueuedBook(index)}
                    >
                      <Trash size={13} />
                    </Button>
                  </div>
                ))}
              </div>
            </div>
          )}

          <div className="space-y-1">
            <Label htmlFor="book-title">Title *</Label>
            <Input
              id="book-title"
              value={form.title}
              onChange={(e) => onFormChange({ ...form, title: e.target.value })}
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
          <div className="grid grid-cols-2 gap-3">
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
            <Label htmlFor="book-notes">Notes</Label>
            <Input
              id="book-notes"
              value={form.notes}
              onChange={(e) => onFormChange({ ...form, notes: e.target.value })}
              placeholder="Optional notes"
            />
          </div>
          {!isEditing && (
            <Button
              type="button"
              variant="outline"
              className="w-full"
              onClick={addDraftToQueue}
              disabled={!draftHasTitle || saving}
            >
              <Plus size={14} className="mr-1" />
              Add Another
            </Button>
          )}
        </div>
        <DialogFooter>
          <Button variant="outline" onClick={handleClose} disabled={saving}>
            Cancel
          </Button>
          <Button
            onClick={handlePrimaryAction}
            disabled={saving || (!isEditing && createCount === 0)}
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
