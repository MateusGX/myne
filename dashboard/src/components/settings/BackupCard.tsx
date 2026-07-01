import { useRef, useState } from "react"
import { Archive, DownloadSimple, UploadSimple } from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Progress } from "@/components/ui/progress"
import {
  BACKUP_RESTORE_DIR,
  BACKUP_RESTORE_FILENAME,
  backupDownloadUrl,
  restoreBackup,
  uploadViaWebSocket,
} from "@/lib/api"

type BackupState =
  | { phase: "idle" }
  | { phase: "ready"; fileName: string }
  | { phase: "uploading"; progress: number }
  | { phase: "restoring" }
  | { phase: "done" }
  | { phase: "error"; message: string }

export function BackupCard() {
  const [state, setState] = useState<BackupState>({ phase: "idle" })
  const [selectedFile, setSelectedFile] = useState<File | null>(null)
  const inputRef = useRef<HTMLInputElement>(null)

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0]
    if (!file) return
    if (!file.name.endsWith(".ndjson")) {
      toast.error("Please select a .ndjson backup file")
      return
    }
    setSelectedFile(file)
    setState({ phase: "ready", fileName: file.name })
  }

  const reset = () => {
    setSelectedFile(null)
    setState({ phase: "idle" })
    if (inputRef.current) inputRef.current.value = ""
  }

  const handleRestore = async () => {
    if (!selectedFile) return
    if (
      !confirm(
        "Restore this backup? Files with the same path will be overwritten."
      )
    ) {
      return
    }

    setState({ phase: "uploading", progress: 0 })
    try {
      await uploadViaWebSocket(
        selectedFile,
        BACKUP_RESTORE_DIR,
        (recv, total) => {
          const pct = total > 0 ? Math.round((recv / total) * 100) : 0
          setState({ phase: "uploading", progress: pct })
        },
        BACKUP_RESTORE_FILENAME
      )
    } catch (err) {
      setState({
        phase: "error",
        message: err instanceof Error ? err.message : "Backup upload failed",
      })
      return
    }

    setState({ phase: "restoring" })
    try {
      const result = await restoreBackup()
      if (!result.ok) {
        throw new Error(result.error || "Backup restore failed")
      }
      toast.success("Backup restored")
      setState({ phase: "done" })
    } catch (err) {
      setState({
        phase: "error",
        message: err instanceof Error ? err.message : "Backup restore failed",
      })
    }
  }

  return (
    <section className="flat-panel">
      <div className="border-b border-border px-5 py-4">
        <h2 className="flex items-center gap-2 text-base font-semibold tracking-tight">
          <span className="flex size-9 items-center justify-center rounded-md border border-primary/20 bg-primary/10 text-primary">
            <Archive size={16} />
          </span>
          Backup & Restore
        </h2>
      </div>
      <div className="space-y-4 p-5">
        <div className="flex flex-wrap items-center gap-2">
          <a href={backupDownloadUrl()} download>
            <Button variant="outline" size="sm">
              <DownloadSimple size={14} className="mr-1" />
              Download backup
            </Button>
          </a>
        </div>

        {(state.phase === "idle" || state.phase === "ready") && (
          <div className="space-y-3">
            <div className="space-y-1">
              <Label className="text-xs">Backup file (.ndjson)</Label>
              <Input
                ref={inputRef}
                type="file"
                accept=".ndjson,application/x-ndjson,application/json"
                onChange={handleFileChange}
                className="h-8 text-xs"
              />
            </div>
            {state.phase === "ready" && (
              <div className="flex items-center justify-between gap-2">
                <p className="truncate text-xs text-muted-foreground">
                  {state.fileName}
                </p>
                <div className="flex shrink-0 gap-2">
                  <Button
                    variant="outline"
                    size="sm"
                    className="h-7 text-xs"
                    onClick={reset}
                  >
                    Cancel
                  </Button>
                  <Button
                    size="sm"
                    className="h-7 text-xs"
                    onClick={handleRestore}
                  >
                    <UploadSimple size={13} />
                    Restore
                  </Button>
                </div>
              </div>
            )}
          </div>
        )}

        {state.phase === "uploading" && (
          <div className="space-y-2">
            <div className="flex justify-between text-xs">
              <span className="text-muted-foreground">Uploading backup...</span>
              <span className="font-medium">{state.progress}%</span>
            </div>
            <Progress value={state.progress} className="h-2" />
          </div>
        )}

        {state.phase === "restoring" && (
          <p className="text-center text-sm text-muted-foreground">
            Restoring backup...
          </p>
        )}

        {state.phase === "done" && (
          <div className="space-y-3 text-center">
            <p className="text-sm font-medium">Backup restored</p>
            <Button
              size="sm"
              className="h-7 text-xs"
              onClick={() => window.location.reload()}
            >
              Reload page
            </Button>
          </div>
        )}

        {state.phase === "error" && (
          <div className="space-y-3">
            <p className="text-sm text-destructive">{state.message}</p>
            <Button
              variant="outline"
              size="sm"
              className="h-7 text-xs"
              onClick={reset}
            >
              Try again
            </Button>
          </div>
        )}
      </div>
    </section>
  )
}
