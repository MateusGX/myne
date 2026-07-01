import { BookOpen, PencilSimple, Trash } from "@phosphor-icons/react"
import { Button } from "@/components/ui/button"
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { DetailField } from "@/components/books/DetailField"
import { type Book } from "@/lib/api"

interface BookDetailDialogProps {
  book: Book | null
  onClose: () => void
  onDelete: (book: Book) => void
  onEdit: (book: Book) => void
  onReadings: (book: Book) => void
}

export function BookDetailDialog({
  book,
  onClose,
  onDelete,
  onEdit,
  onReadings,
}: BookDetailDialogProps) {
  return (
    <Dialog open={!!book} onOpenChange={(o) => !o && onClose()}>
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle className="leading-snug">
            {book?.title}
            {book?.volume && (
              <span className="ml-2 text-sm font-normal text-muted-foreground">
                {book.volume}
              </span>
            )}
          </DialogTitle>
        </DialogHeader>
        {book && (
          <div className="space-y-3">
            <DetailField label="Author" value={book.author} />
            <DetailField label="Collection" value={book.collection} />
            <DetailField label="Location" value={book.location} />
            <DetailField label="Note" value={book.note} />
          </div>
        )}
        <DialogFooter className="gap-2 sm:justify-between">
          <Button
            variant="destructive"
            size="sm"
            onClick={() => book && onDelete(book)}
          >
            <Trash size={14} className="mr-1" />
            Delete
          </Button>
          <div className="flex gap-2">
            <Button
              variant="outline"
              size="sm"
              onClick={() => book && onReadings(book)}
            >
              <BookOpen size={14} className="mr-1" />
              Readings
            </Button>
            <Button size="sm" onClick={() => book && onEdit(book)}>
              <PencilSimple size={14} className="mr-1" />
              Edit
            </Button>
          </div>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}
