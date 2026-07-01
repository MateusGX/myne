import { type Dispatch, type SetStateAction, useRef } from "react"
import { UploadSimple, X } from "@phosphor-icons/react"
import { Button } from "@/components/ui/button"
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { Progress } from "@/components/ui/progress"
import { formatSize } from "@/lib/fileUtils"
import { isImageFile } from "@/lib/api"

export interface UploadState {
  files: File[]
  progress: number
  current: string
  uploading: boolean
}

interface UploadDialogProps {
  open: boolean
  uploadState: UploadState
  onClose: () => void
  onUpload: () => void
  setUploadState: Dispatch<SetStateAction<UploadState>>
}

export function UploadDialog({
  open,
  uploadState,
  onClose,
  onUpload,
  setUploadState,
}: UploadDialogProps) {
  const fileInputRef = useRef<HTMLInputElement>(null)

  return (
    <Dialog
      open={open}
      onOpenChange={(o) => !o && !uploadState.uploading && onClose()}
    >
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle>Upload Files</DialogTitle>
        </DialogHeader>
        <div className="space-y-4">
          <div
            className="rounded-lg border border-dashed border-border bg-muted/20 p-8 text-center transition-colors hover:border-primary"
            onDragOver={(e) => e.preventDefault()}
            onDrop={(e) => {
              e.preventDefault()
              const dropped = Array.from(e.dataTransfer.files)
              setUploadState((s) => ({ ...s, files: dropped }))
            }}
          >
            <UploadSimple
              size={24}
              className="mx-auto mb-2 text-muted-foreground"
            />
            <p className="text-sm text-muted-foreground">
              Drop files here or{" "}
              <button
                className="text-primary hover:underline"
                onClick={() => fileInputRef.current?.click()}
              >
                browse
              </button>
            </p>
            <input
              ref={fileInputRef}
              type="file"
              multiple
              className="hidden"
              onChange={(e) => {
                const chosen = Array.from(e.target.files ?? [])
                setUploadState((s) => ({ ...s, files: chosen }))
              }}
            />
          </div>

          {uploadState.files.length > 0 && (
            <div className="space-y-1">
              {uploadState.files.map((f) => {
                const willConvert = isImageFile(f) && f.type !== "image/jpeg"
                const finalName = willConvert
                  ? f.name.replace(/\.[^.]+$/, "") + ".jpg"
                  : f.name
                return (
                  <div
                    key={f.name}
                    className="flex items-center justify-between rounded bg-muted px-3 py-1.5 text-xs"
                  >
                    <span className="truncate">
                      {willConvert ? (
                        <>
                          {f.name}
                          <span className="text-muted-foreground">
                            {" "}
                            → {finalName}
                          </span>
                        </>
                      ) : (
                        f.name
                      )}
                    </span>
                    <span className="ml-2 shrink-0 text-muted-foreground">
                      {formatSize(f.size)}
                    </span>
                  </div>
                )
              })}
            </div>
          )}

          {uploadState.uploading && (
            <div className="space-y-1.5">
              <div className="flex justify-between text-xs">
                <span className="truncate text-muted-foreground">
                  {uploadState.current}
                </span>
                <span>{uploadState.progress}%</span>
              </div>
              <Progress value={uploadState.progress} className="h-1.5" />
            </div>
          )}
        </div>
        <DialogFooter>
          <Button
            variant="outline"
            onClick={onClose}
            disabled={uploadState.uploading}
          >
            <X size={14} className="mr-1" /> Cancel
          </Button>
          <Button
            onClick={onUpload}
            disabled={uploadState.files.length === 0 || uploadState.uploading}
          >
            {uploadState.uploading ? "Uploading..." : "Upload"}
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}
