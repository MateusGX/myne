import { Button } from "@/components/ui/button"
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { type Book } from "@/lib/api"

export type ImportPhase =
  | { step: "preview"; books: Book[]; collectionNotes: Record<string, string> }
  | { step: "running"; done: number; total: number; current: string; kind: "book" | "note" }
  | {
      step: "done"
      created: number
      failed: number
      notesImported: number
      notesFailed: number
      failedBooks: Book[]
      failedNotes: Record<string, string>
    }

export function ImportDialog({
  phase,
  onConfirm,
  onRetry,
  onClose,
}: {
  phase: ImportPhase | null
  onConfirm: () => void
  onRetry: () => void
  onClose: () => void
}) {
  const open = phase !== null
  const canClose = phase?.step !== "running"

  return (
    <Dialog open={open} onOpenChange={(o) => !o && canClose && onClose()}>
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle>
            {phase?.step === "preview" && "Import Books"}
            {phase?.step === "running" && "Importing…"}
            {phase?.step === "done" && "Import Complete"}
          </DialogTitle>
        </DialogHeader>

        {phase?.step === "preview" && (
          <>
            <div className="min-w-0 space-y-3">
              <p className="text-sm">
                <span className="font-semibold">{phase.books.length} books</span> found in the
                file
                {Object.keys(phase.collectionNotes ?? {}).length > 0 && (
                  <>
                    {" "}and{" "}
                    <span className="font-semibold">
                      {Object.keys(phase.collectionNotes).length} collection notes
                    </span>
                  </>
                )}
                . They will be added to your existing library.
              </p>
              <div className="border-border divide-border max-h-52 divide-y overflow-y-auto rounded-md border">
                {phase.books.slice(0, 8).map((b, i) => (
                  <div key={i} className="px-3 py-2">
                    <p className="truncate text-sm font-medium">{b.title}</p>
                    {b.author && (
                      <p className="text-muted-foreground truncate text-xs">{b.author}</p>
                    )}
                  </div>
                ))}
                {phase.books.length > 8 && (
                  <div className="text-muted-foreground px-3 py-2 text-xs">
                    +{phase.books.length - 8} more…
                  </div>
                )}
              </div>
            </div>
            <DialogFooter>
              <Button variant="outline" onClick={onClose}>
                Cancel
              </Button>
              <Button onClick={onConfirm}>Import {phase.books.length} books</Button>
            </DialogFooter>
          </>
        )}

        {phase?.step === "running" && (
          <div className="min-w-0 space-y-4 py-2">
            <div className="space-y-2">
              <div className="text-muted-foreground flex justify-between text-xs">
                <span>
                  {phase.done} / {phase.total} {phase.kind === "note" ? "collection notes" : "books"}
                </span>
                <span>{phase.total > 0 ? Math.round((phase.done / phase.total) * 100) : 0}%</span>
              </div>
              <div className="bg-muted h-2 w-full overflow-hidden rounded-full">
                <div
                  className="bg-primary h-full transition-all duration-150"
                  style={{ width: `${phase.total > 0 ? Math.round((phase.done / phase.total) * 100) : 0}%` }}
                />
              </div>
            </div>
            {phase.current && (
              <p className="text-muted-foreground truncate text-xs">
                {phase.kind === "note" ? "Saving note for:" : "Adding:"}{" "}
                <span className="text-foreground font-medium">{phase.current}</span>
              </p>
            )}
          </div>
        )}

        {phase?.step === "done" && (
          <>
            <div className="space-y-2 py-1">
              <p className="text-sm">
                <span className="font-semibold">{phase.created}</span>{" "}
                {phase.created === 1 ? "book" : "books"} imported successfully.
              </p>
              {phase.failed > 0 && (
                <p className="text-destructive text-sm">
                  {phase.failed} {phase.failed === 1 ? "book" : "books"} failed to import.
                </p>
              )}
              {phase.notesImported > 0 && (
                <p className="text-sm">
                  <span className="font-semibold">{phase.notesImported}</span>{" "}
                  {phase.notesImported === 1 ? "collection note" : "collection notes"} imported
                  successfully.
                </p>
              )}
              {phase.notesFailed > 0 && (
                <p className="text-destructive text-sm">
                  {phase.notesFailed}{" "}
                  {phase.notesFailed === 1 ? "collection note" : "collection notes"} failed to
                  import.
                </p>
              )}
            </div>
            <DialogFooter>
              {(phase.failed > 0 || phase.notesFailed > 0) && (
                <Button variant="outline" onClick={onRetry}>
                  Retry failed
                </Button>
              )}
              <Button onClick={onClose}>Done</Button>
            </DialogFooter>
          </>
        )}
      </DialogContent>
    </Dialog>
  )
}
