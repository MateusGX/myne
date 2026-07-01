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
      collectionMetadata: CollectionMetadata[]
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
      updated: number
      failed: number
      metadataImported: number
      metadataFailed: number
      failedBooks: Book[]
      failedMetadata: CollectionMetadata[]
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
  const collectionRows =
    phase?.step === "preview" ? (phase.collectionMetadata ?? []).length : 0

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
                {mode === "import" && collectionRows > 0 && (
                    <>
                      {" "}
                      and{" "}
                      <span className="font-semibold">
                        {collectionRows} collection rows
                      </span>
                    </>
                  )}
                . They will be saved to your existing library.
              </p>
              <div className="max-h-52 divide-y divide-border overflow-y-auto rounded-md border border-border">
                {phase.books.length > 0
                  ? phase.books.slice(0, 8).map((b, i) => (
                      <div key={i} className="px-3 py-2">
                        <p className="truncate text-sm font-medium">
                          {b.title}
                        </p>
                        {b.author && (
                          <p className="truncate text-xs text-muted-foreground">
                            {b.author}
                          </p>
                        )}
                      </div>
                    ))
                  : phase.collectionMetadata.slice(0, 8).map((collection, i) => (
                      <div key={collection.id || `${collection.name}-${i}`} className="px-3 py-2">
                        <p className="truncate text-sm font-medium">
                          {collection.name || collection.id}
                        </p>
                        <p className="truncate text-xs text-muted-foreground">
                          Collection row
                        </p>
                      </div>
                    ))}
                {phase.books.length > 8 && (
                  <div className="px-3 py-2 text-xs text-muted-foreground">
                    +{phase.books.length - 8} more…
                  </div>
                )}
                {phase.books.length === 0 && collectionRows > 8 && (
                  <div className="px-3 py-2 text-xs text-muted-foreground">
                    +{collectionRows - 8} more…
                  </div>
                )}
              </div>
            </div>
            <DialogFooter>
              <Button variant="outline" onClick={onClose}>
                Cancel
              </Button>
              <Button onClick={onConfirm}>
                {mode === "create" ? "Create" : "Import"}{" "}
                {phase.books.length > 0
                  ? `${phase.books.length} books`
                  : `${collectionRows} collection rows`}
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
                  {phase.kind === "metadata" ? "collection rows" : "books"}
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
                {phase.kind === "metadata" ? "Saving collection:" : "Saving:"}{" "}
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
              {mode === "import" && phase.updated > 0 && (
                <p className="text-sm text-muted-foreground">
                  {phase.updated} {phase.updated === 1 ? "book was" : "books were"} updated.
                </p>
              )}
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
                    ? "collection row"
                    : "collection rows"}{" "}
                  imported successfully.
                </p>
              )}
              {phase.metadataFailed > 0 && (
                <p className="text-sm text-destructive">
                  {phase.metadataFailed}{" "}
                  {phase.metadataFailed === 1
                    ? "collection row"
                    : "collection rows"}{" "}
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
