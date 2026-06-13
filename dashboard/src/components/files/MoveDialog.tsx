import { useEffect, useState } from "react"
import { ArrowLeft, Folder } from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { getFiles, moveFile, type FileInfo } from "@/lib/api"
import { joinPath, parentPath } from "@/lib/fileUtils"
import { Breadcrumb } from "@/components/files/Breadcrumb"

interface MoveDialogProps {
  open: boolean
  onClose: () => void
  sourcePath: string
  onMoved: () => void
}

export function MoveDialog({ open, onClose, sourcePath, onMoved }: MoveDialogProps) {
  const [browsePath, setBrowsePath] = useState("/")
  const [folders, setFolders] = useState<FileInfo[]>([])
  const [loading, setLoading] = useState(false)

  useEffect(() => {
    if (!open) return
    setBrowsePath("/")
  }, [open])

  useEffect(() => {
    if (!open) return
    setLoading(true)
    getFiles(browsePath)
      .then((f) => setFolders(f.filter((x) => x.isDirectory)))
      .catch(() => setFolders([]))
      .finally(() => setLoading(false))
  }, [open, browsePath])

  const handleMove = async () => {
    try {
      await moveFile(sourcePath, browsePath)
      toast.success("Moved successfully")
      onMoved()
      onClose()
    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : "Move failed"
      toast.error(msg)
    }
  }

  const sourceName = sourcePath.split("/").pop() ?? ""

  return (
    <Dialog open={open} onOpenChange={(o) => !o && onClose()}>
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle>Move "{sourceName}"</DialogTitle>
        </DialogHeader>
          <div className="space-y-3">
          <Breadcrumb path={browsePath} onNavigate={setBrowsePath} />
          <div className="border-border max-h-60 overflow-y-auto rounded-lg border bg-card">
            {browsePath !== "/" && (
              <button
                onClick={() => setBrowsePath(parentPath(browsePath))}
                className="hover:bg-accent/60 flex w-full items-center gap-2 px-3 py-2 text-sm"
              >
                <ArrowLeft size={14} />
                ..
              </button>
            )}
            {loading ? (
              <p className="text-muted-foreground px-3 py-4 text-center text-xs">Loading...</p>
            ) : folders.length === 0 ? (
              <p className="text-muted-foreground px-3 py-4 text-center text-xs">Empty folder</p>
            ) : (
              folders.map((f) => (
                <button
                  key={f.name}
                  onClick={() => setBrowsePath(joinPath(browsePath, f.name))}
                  className="hover:bg-accent/60 flex w-full items-center gap-2 px-3 py-2 text-sm"
                >
                  <Folder size={14} className="text-muted-foreground" />
                  {f.name}
                </button>
              ))
            )}
          </div>
          <p className="text-muted-foreground text-xs">
            Destination: <span className="font-mono">{browsePath}</span>
          </p>
        </div>
        <DialogFooter>
          <Button variant="outline" onClick={onClose}>
            Cancel
          </Button>
          <Button onClick={handleMove}>Move Here</Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}
