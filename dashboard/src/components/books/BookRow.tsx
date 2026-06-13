import { BookOpen, PencilSimple, Trash } from "@phosphor-icons/react"
import { Button } from "@/components/ui/button"
import { type Book } from "@/lib/api"

export function BookRow({
  book,
  onSelect,
  onEdit,
  onDelete,
}: {
  book: Book
  onSelect: (book: Book) => void
  onEdit: (book: Book) => void
  onDelete: (book: Book) => void
}) {
  return (
    <div
      className="group flex cursor-pointer items-center gap-4 px-4 py-3 transition-colors hover:bg-muted/45"
      onClick={() => onSelect(book)}
    >
      <div className="flex size-10 shrink-0 items-center justify-center rounded-md border border-primary/20 bg-primary/10 text-primary">
        <BookOpen size={18} />
      </div>
      <div className="min-w-0 flex-1">
        <div className="flex items-baseline gap-1.5">
          <span className="truncate text-sm font-medium">{book.title}</span>
          {book.volume && (
            <span className="text-muted-foreground shrink-0 text-xs">{book.volume}</span>
          )}
        </div>
        {book.author && (
          <p className="text-muted-foreground truncate text-xs">{book.author}</p>
        )}
      </div>
      {book.location && (
        <span className="text-muted-foreground hidden shrink-0 text-xs sm:block">
          {book.location}
        </span>
      )}
      <div
        className="flex shrink-0 items-center gap-0.5 opacity-0 transition-opacity group-hover:opacity-100"
        onClick={(e) => e.stopPropagation()}
      >
        <Button
          variant="ghost"
          size="sm"
          onClick={() => onEdit(book)}
          className="h-7 w-7 p-0"
        >
          <PencilSimple size={13} />
        </Button>
        <Button
          variant="ghost"
          size="sm"
          onClick={() => onDelete(book)}
          className="text-destructive hover:text-destructive h-7 w-7 p-0"
        >
          <Trash size={13} />
        </Button>
      </div>
    </div>
  )
}
