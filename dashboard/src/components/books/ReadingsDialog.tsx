import { useEffect, useState } from "react"
import { BookOpen, CaretDown, CaretRight, Plus, Trash, X } from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { getReadings, saveReadings, type Book, type Reading, type ReadingSession } from "@/lib/api"

const STATUS_LABELS: Record<string, string> = {
  want: "Want to Read",
  reading: "Reading",
  paused: "Paused",
  finished: "Finished",
  dropped: "Dropped",
}
const STATUS_OPTIONS = ["want", "reading", "paused", "finished", "dropped"]

function newReading(): Reading {
  return { id: `${Date.now()}`, status: "reading", readingType: 0, sessions: [] }
}

export function ReadingsDialog({
  book,
  open,
  onOpenChange,
}: {
  book: Book
  open: boolean
  onOpenChange: (o: boolean) => void
}) {
  const [readings, setReadings] = useState<Reading[]>([])
  const [loading, setLoading] = useState(false)
  const [saving, setSaving] = useState(false)
  const [dirty, setDirty] = useState(false)
  const [expandedId, setExpandedId] = useState<string | null>(null)
  const [sessionDraft, setSessionDraft] = useState<
    Record<string, { date: string; position: string }>
  >({})

  useEffect(() => {
    if (!open) return
    setLoading(true)
    setDirty(false)
    setExpandedId(null)
    getReadings(book.id)
      .then(setReadings)
      .catch(() => toast.error("Failed to load readings"))
      .finally(() => setLoading(false))
  }, [open, book.id])

  function mutate(fn: (prev: Reading[]) => Reading[]) {
    setReadings(fn)
    setDirty(true)
  }

  function addReading() {
    const r = newReading()
    mutate((prev) => [...prev, r])
    setExpandedId(r.id)
  }

  function updateReading(id: string, changes: Partial<Reading>) {
    mutate((prev) => prev.map((r) => (r.id === id ? { ...r, ...changes } : r)))
  }

  function deleteReading(id: string) {
    mutate((prev) => prev.filter((r) => r.id !== id))
    if (expandedId === id) setExpandedId(null)
  }

  function addSession(readingId: string) {
    const draft = sessionDraft[readingId] ?? { date: "", position: "" }
    const pos = parseInt(draft.position, 10)
    if (isNaN(pos) || pos < 0) {
      toast.error("Enter a valid position number")
      return
    }
    const session: ReadingSession = { date: draft.date, position: pos }
    mutate((prev) =>
      prev.map((r) =>
        r.id === readingId ? { ...r, sessions: [...r.sessions, session] } : r,
      ),
    )
    setSessionDraft((f) => ({ ...f, [readingId]: { date: "", position: "" } }))
  }

  function deleteSession(readingId: string, idx: number) {
    mutate((prev) =>
      prev.map((r) =>
        r.id === readingId
          ? { ...r, sessions: r.sessions.filter((_, i) => i !== idx) }
          : r,
      ),
    )
  }

  async function handleSave() {
    setSaving(true)
    try {
      await saveReadings(book.id, readings)
      toast.success("Readings saved")
      setDirty(false)
      onOpenChange(false)
    } catch {
      toast.error("Failed to save readings")
    } finally {
      setSaving(false)
    }
  }

  function handleClose() {
    if (dirty && !confirm("Discard unsaved changes?")) return
    onOpenChange(false)
  }

  return (
    <Dialog open={open} onOpenChange={(o) => !o && handleClose()}>
      <DialogContent className="flex max-h-[85vh] max-w-lg flex-col">
        <DialogHeader>
          <DialogTitle className="pr-6 leading-snug">
            {book.title}
            <span className="text-muted-foreground ml-2 text-sm font-normal">
              — Readings
            </span>
          </DialogTitle>
        </DialogHeader>

        <div className="-mx-6 flex-1 space-y-2 overflow-y-auto px-6 pb-2">
          {loading ? (
            <div className="flex justify-center py-10">
              <span className="text-muted-foreground text-sm">Loading…</span>
            </div>
          ) : readings.length === 0 ? (
            <div className="text-muted-foreground flex flex-col items-center gap-3 py-10">
              <BookOpen size={36} weight="thin" />
              <p className="text-sm">No readings yet. Add the first one below.</p>
            </div>
          ) : (
            readings.map((reading) => {
              const isExpanded = expandedId === reading.id
              const lastSession = reading.sessions.at(-1)
              const unit = reading.readingType === 1 ? "ch." : "p."
              const statusColor =
                reading.status === "reading"
                  ? "bg-foreground text-background ring-foreground/20"
                  : reading.status === "finished"
                    ? "bg-muted text-foreground ring-border"
                    : reading.status === "dropped"
                      ? "bg-muted text-muted-foreground ring-border"
                      : "bg-muted text-muted-foreground ring-border"
              const draft = sessionDraft[reading.id] ?? { date: "", position: "" }

              return (
                <div key={reading.id} className="border-border rounded-lg border">
                  {/* Collapsed header */}
                  <button
                    className="flex w-full items-center gap-2.5 px-4 py-3 text-left"
                    onClick={() => setExpandedId(isExpanded ? null : reading.id)}
                  >
                    <span
                      className={`inline-flex shrink-0 items-center rounded-full px-2 py-0.5 text-xs font-medium ring-1 ring-inset ${statusColor}`}
                    >
                      {STATUS_LABELS[reading.status] ?? reading.status}
                    </span>
                    <span className="text-muted-foreground text-xs">
                      {reading.readingType === 1 ? "Chapter" : "Page"}
                    </span>
                    <span className="text-muted-foreground ml-auto text-xs">
                      {reading.sessions.length}{" "}
                      {reading.sessions.length === 1 ? "session" : "sessions"}
                      {lastSession && ` · ${unit} ${lastSession.position}`}
                    </span>
                    {isExpanded ? (
                      <CaretDown size={12} className="text-muted-foreground shrink-0" />
                    ) : (
                      <CaretRight size={12} className="text-muted-foreground shrink-0" />
                    )}
                  </button>

                  {/* Expanded content */}
                  {isExpanded && (
                    <div className="border-border space-y-4 border-t px-4 py-3">
                      {/* Status + Tracking */}
                      <div className="grid grid-cols-2 gap-3">
                        <div className="space-y-1">
                          <Label className="text-xs">Status</Label>
                          <select
                            value={reading.status}
                            onChange={(e) =>
                              updateReading(reading.id, { status: e.target.value })
                            }
                            className="border-border bg-background text-foreground w-full rounded-md border px-3 py-1.5 text-sm"
                          >
                            {STATUS_OPTIONS.map((s) => (
                              <option key={s} value={s}>
                                {STATUS_LABELS[s]}
                              </option>
                            ))}
                          </select>
                        </div>
                        <div className="space-y-1">
                          <Label className="text-xs">Tracking</Label>
                          <select
                            value={reading.readingType}
                            onChange={(e) =>
                              updateReading(reading.id, {
                                readingType: Number(e.target.value),
                              })
                            }
                            className="border-border bg-background text-foreground w-full rounded-md border px-3 py-1.5 text-sm"
                          >
                            <option value={0}>Page</option>
                            <option value={1}>Chapter</option>
                          </select>
                        </div>
                      </div>

                      {/* Sessions list */}
                      {reading.sessions.length > 0 && (
                        <div>
                          <p className="text-muted-foreground mb-1.5 text-xs font-medium uppercase tracking-wide">
                            Sessions
                          </p>
                          <div className="border-border divide-border divide-y rounded-md border">
                            {reading.sessions.map((s, idx) => (
                              <div
                                key={idx}
                                className="flex items-center gap-3 px-3 py-1.5"
                              >
                                <span className="text-muted-foreground flex-1 text-sm">
                                  {s.date || "—"}
                                </span>
                                <span className="text-sm">
                                  {unit} {s.position}
                                </span>
                                <button
                                  onClick={() => deleteSession(reading.id, idx)}
                                  className="text-muted-foreground hover:text-destructive"
                                >
                                  <X size={14} />
                                </button>
                              </div>
                            ))}
                          </div>
                        </div>
                      )}

                      {/* Add session */}
                      <div className="flex items-end gap-2">
                        <div className="flex-1 space-y-1">
                          <Label className="text-xs">Date</Label>
                          <Input
                            type="date"
                            value={draft.date}
                            onChange={(e) =>
                              setSessionDraft((f) => ({
                                ...f,
                                [reading.id]: { ...draft, date: e.target.value },
                              }))
                            }
                            className="h-8 text-sm"
                          />
                        </div>
                        <div className="w-24 space-y-1">
                          <Label className="text-xs">Position</Label>
                          <Input
                            type="number"
                            min={0}
                            value={draft.position}
                            onChange={(e) =>
                              setSessionDraft((f) => ({
                                ...f,
                                [reading.id]: { ...draft, position: e.target.value },
                              }))
                            }
                            onKeyDown={(e) => e.key === "Enter" && addSession(reading.id)}
                            className="h-8 text-sm"
                            placeholder="0"
                          />
                        </div>
                        <Button
                          size="sm"
                          className="h-8 shrink-0"
                          onClick={() => addSession(reading.id)}
                        >
                          <Plus size={14} />
                        </Button>
                      </div>

                      {/* Delete reading */}
                      <div className="flex justify-end border-t pt-2">
                        <Button
                          variant="ghost"
                          size="sm"
                          className="text-destructive hover:text-destructive h-7 text-xs"
                          onClick={() => deleteReading(reading.id)}
                        >
                          <Trash size={12} className="mr-1" />
                          Delete reading
                        </Button>
                      </div>
                    </div>
                  )}
                </div>
              )
            })
          )}
        </div>

        <div className="border-border flex items-center justify-between border-t pt-4">
          <Button variant="outline" size="sm" onClick={addReading}>
            <Plus size={14} className="mr-1" />
            New Reading
          </Button>
          <div className="flex gap-2">
            <Button variant="outline" onClick={handleClose} disabled={saving}>
              Cancel
            </Button>
            <Button onClick={handleSave} disabled={!dirty || saving}>
              {saving ? "Saving…" : "Save"}
            </Button>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  )
}
