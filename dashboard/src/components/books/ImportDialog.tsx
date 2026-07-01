import { Button } from "@/components/ui/button"
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { type Book } from "@/lib/api"
import { type CollectionMetadata } from "@/lib/booksXlsx"

export type ImportMode = "import" | "create"

export type ImportPhase =
  | {
      step: "preview"
      mode?: ImportMode
      books: Book[]
      collectionMetadata: Record<string, CollectionMetadata>
    }
  | {
      step: "running"
      mode?: ImportMode
      done: number
      total: number
      current: string
      kind: "book" | "metadata"
    }
  | {
      step: "done"
      mode?: ImportMode
      created: number
      failed: number
      metadataImported: number
      metadataFailed: number
      failedBooks: Book[]
      failedMetadata: Record<string, CollectionMetadata>
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
  const mode = phase?.mode ?? "import"

  return (
    <Dialog open={open} onOpenChange={(o) => !o && canClose && onClose()}>
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle>
            {phase?.step === "preview" &&
              (mode === "create" ? "Create Books" : "Import Books")}
            {phase?.step === "running" &&
              (mode === "create" ? "Creating…" : "Importing…")}
            {phase?.step === "done" &&
              (mode === "create" ? "Create Complete" : "Import Complete")}
          </DialogTitle>
        </DialogHeader>

        {phase?.step === "preview" && (
          <>
            <div className="min-w-0 space-y-3">
              <p className="text-sm">
                <span className="font-semibold">
                  {phase.books.length} books
                </span>{" "}
                {mode === "create" ? "queued" : "found in the file"}
                {mode === "import" &&
                  Object.keys(phase.collectionMetadata ?? {}).length > 0 && (
                    <>
                      {" "}
                      and{" "}
                      <span className="font-semibold">
                        {Object.keys(phase.collectionMetadata).length}{" "}
                        collection metadata rows
                      </span>
                    </>
                  )}
                . They will be added to your existing library.
              </p>
              <div className="max-h-52 divide-y divide-border overflow-y-auto rounded-md border border-border">
                {phase.books.slice(0, 8).map((b, i) => (
                  <div key={i} className="px-3 py-2">
                    <p className="truncate text-sm font-medium">{b.title}</p>
                    {b.author && (
                      <p className="truncate text-xs text-muted-foreground">
                        {b.author}
                      </p>
                    )}
                  </div>
                ))}
                {phase.books.length > 8 && (
                  <div className="px-3 py-2 text-xs text-muted-foreground">
                    +{phase.books.length - 8} more…
                  </div>
                )}
              </div>
            </div>
            <DialogFooter>
              <Button variant="outline" onClick={onClose}>
                Cancel
              </Button>
              <Button onClick={onConfirm}>
                {mode === "create" ? "Create" : "Import"} {phase.books.length}{" "}
                books
              </Button>
            </DialogFooter>
          </>
        )}

        {phase?.step === "running" && (
          <div className="min-w-0 space-y-4 py-2">
            <div className="space-y-2">
              <div className="flex justify-between text-xs text-muted-foreground">
                <span>
                  {phase.done} / {phase.total}{" "}
                  {phase.kind === "metadata" ? "collection metadata" : "books"}
                </span>
                <span>
                  {phase.total > 0
                    ? Math.round((phase.done / phase.total) * 100)
                    : 0}
                  %
                </span>
              </div>
              <div className="h-2 w-full overflow-hidden rounded-full bg-muted">
                <div
                  className="h-full bg-primary transition-all duration-150"
                  style={{
                    width: `${phase.total > 0 ? Math.round((phase.done / phase.total) * 100) : 0}%`,
                  }}
                />
              </div>
            </div>
            {phase.current && (
              <p className="truncate text-xs text-muted-foreground">
                {phase.kind === "metadata" ? "Saving metadata for:" : "Adding:"}{" "}
                <span className="font-medium text-foreground">
                  {phase.current}
                </span>
              </p>
            )}
          </div>
        )}

        {phase?.step === "done" && (
          <>
            <div className="space-y-2 py-1">
              <p className="text-sm">
                <span className="font-semibold">{phase.created}</span>{" "}
                {phase.created === 1 ? "book" : "books"}{" "}
                {mode === "create" ? "created" : "imported"} successfully.
              </p>
              {phase.failed > 0 && (
                <p className="text-sm text-destructive">
                  {phase.failed} {phase.failed === 1 ? "book" : "books"} failed
                  to {mode === "create" ? "create" : "import"}.
                </p>
              )}
              {phase.metadataImported > 0 && (
                <p className="text-sm">
                  <span className="font-semibold">
                    {phase.metadataImported}
                  </span>{" "}
                  {phase.metadataImported === 1
                    ? "collection metadata row"
                    : "collection metadata rows"}{" "}
                  imported successfully.
                </p>
              )}
              {phase.metadataFailed > 0 && (
                <p className="text-sm text-destructive">
                  {phase.metadataFailed}{" "}
                  {phase.metadataFailed === 1
                    ? "collection metadata row"
                    : "collection metadata rows"}{" "}
                  failed to import.
                </p>
              )}
            </div>
            <DialogFooter>
              {(phase.failed > 0 || phase.metadataFailed > 0) && (
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
