import { useCallback, useEffect, useRef, useState } from "react"
import {
  ArrowLeft,
  File,
  Folder,
  FolderPlus,
  PencilSimple,
  Trash,
  UploadSimple,
  X,
  ArrowsOut,
  Download,
} from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Progress } from "@/components/ui/progress"
import { Badge } from "@/components/ui/badge"
import { EmptyState, PageHeader, Toolbar } from "@/components/dashboard-layout"
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog"
import { Breadcrumb } from "@/components/files/Breadcrumb"
import { MoveDialog } from "@/components/files/MoveDialog"
import { formatSize, joinPath, parentPath } from "@/lib/fileUtils"
import {
  convertToJpeg,
  createFolder,
  deleteItems,
  downloadUrl,
  getFiles,
  isImageFile,
  renameFile,
  type FileInfo,
  uploadViaWebSocket,
} from "@/lib/api"

interface UploadState {
  files: File[]
  progress: number
  current: string
  uploading: boolean
}

export function FilesPage() {
  const [path, setPath] = useState("/")
  const [files, setFiles] = useState<FileInfo[]>([])
  const [loading, setLoading] = useState(true)
  const [selected, setSelected] = useState<Set<string>>(new Set())
  const [uploadState, setUploadState] = useState<UploadState>({
    files: [],
    progress: 0,
    current: "",
    uploading: false,
  })
  const [showUpload, setShowUpload] = useState(false)
  const [showNewFolder, setShowNewFolder] = useState(false)
  const [newFolderName, setNewFolderName] = useState("")
  const [renameTarget, setRenameTarget] = useState<string | null>(null)
  const [renameName, setRenameName] = useState("")
  const [moveTarget, setMoveTarget] = useState<string | null>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)

  const load = useCallback(
    (p: string) => {
      setLoading(true)
      setSelected(new Set())
      getFiles(p)
        .then((f) => {
          const sorted = [...f].sort((a, b) => {
            if (a.isDirectory !== b.isDirectory) return a.isDirectory ? -1 : 1
            return a.name.localeCompare(b.name)
          })
          setFiles(sorted)
          setPath(p)
        })
        .catch(() => toast.error("Failed to load files"))
        .finally(() => setLoading(false))
    },
    [],
  )

  useEffect(() => {
    load("/")
  }, [load])

  const navigate = (p: string) => load(p)

  const toggleSelect = (name: string) => {
    setSelected((prev) => {
      const next = new Set(prev)
      if (next.has(name)) next.delete(name)
      else next.add(name)
      return next
    })
  }

  const handleDelete = async () => {
    if (selected.size === 0) return
    const paths = [...selected].map((n) => joinPath(path, n))
    try {
      await deleteItems(paths)
      toast.success(`Deleted ${selected.size} item(s)`)
      load(path)
    } catch {
      toast.error("Delete failed")
    }
  }

  const handleCreateFolder = async () => {
    if (!newFolderName.trim()) return
    try {
      await createFolder(path, newFolderName.trim())
      toast.success("Folder created")
      setShowNewFolder(false)
      setNewFolderName("")
      load(path)
    } catch {
      toast.error("Failed to create folder")
    }
  }

  const handleRename = async () => {
    if (!renameTarget || !renameName.trim()) return
    try {
      await renameFile(joinPath(path, renameTarget), renameName.trim())
      toast.success("Renamed successfully")
      setRenameTarget(null)
      setRenameName("")
      load(path)
    } catch {
      toast.error("Rename failed")
    }
  }

  const handleUpload = async () => {
    if (uploadState.files.length === 0) return
    setUploadState((s) => ({ ...s, uploading: true, progress: 0 }))
    for (const file of uploadState.files) {
      let uploadFile = file
      if (isImageFile(file) && file.type !== "image/jpeg") {
        try {
          uploadFile = await convertToJpeg(file)
        } catch {
          toast.error(`Failed to convert ${file.name}`)
          continue
        }
      }
      setUploadState((s) => ({ ...s, current: uploadFile.name, progress: 0 }))
      try {
        await uploadViaWebSocket(uploadFile, path, (recv, total) => {
          setUploadState((s) => ({
            ...s,
            progress: total > 0 ? Math.round((recv / total) * 100) : 0,
          }))
        })
        toast.success(`Uploaded ${uploadFile.name}`)
      } catch (e: unknown) {
        const msg = e instanceof Error ? e.message : "Upload failed"
        toast.error(`${file.name}: ${msg}`)
      }
    }
    setUploadState({ files: [], progress: 0, current: "", uploading: false })
    setShowUpload(false)
    load(path)
  }

  const sorted = files

  return (
    <div className="space-y-6">
      <PageHeader
        eyebrow="File manager"
        title="Device storage"
        description="Upload, organize, rename and move files on the Myne SD card."
        actions={<Badge variant="secondary" className="w-fit font-mono">{path}</Badge>}
      />

      <Toolbar className="justify-between">
        <div className="flex items-center gap-2">
          {path !== "/" && (
            <Button
              variant="ghost"
              size="sm"
              onClick={() => navigate(parentPath(path))}
            >
              <ArrowLeft size={14} />
            </Button>
          )}
          <Breadcrumb path={path} onNavigate={navigate} />
        </div>

        <div className="flex flex-wrap items-center gap-2">
          {selected.size > 0 && (
            <>
              <Badge variant="secondary">{selected.size} selected</Badge>
              <Button
                variant="destructive"
                size="sm"
                onClick={handleDelete}
              >
                <Trash size={14} />
              </Button>
            </>
          )}
          <Button
            variant="outline"
            size="sm"
            onClick={() => setShowNewFolder(true)}
          >
            <FolderPlus size={14} />
          </Button>
          <Button size="sm" onClick={() => setShowUpload(true)}>
            <UploadSimple size={14} className="mr-1" />
            Upload
          </Button>
        </div>
      </Toolbar>

      {/* File list */}
      <section className="flat-panel overflow-hidden">
        {loading ? (
          <div className="flex flex-col gap-2 p-4">
            {Array.from({ length: 4 }).map((_, i) => (
              <div key={i} className="bg-muted h-12 animate-pulse rounded-xl" />
            ))}
          </div>
        ) : sorted.length === 0 ? (
          <EmptyState title="Empty folder" description="Upload files or create a folder to start organizing this path." />
        ) : (
          <table className="w-full text-sm">
            <thead>
              <tr className="border-border bg-muted/40 border-b">
                <th className="w-10 px-4 py-3" />
                <th className="px-4 py-3 text-left text-xs font-semibold uppercase tracking-wide text-muted-foreground">Name</th>
                <th className="hidden px-4 py-3 text-right text-xs font-semibold uppercase tracking-wide text-muted-foreground sm:table-cell">
                  Size
                </th>
                <th className="px-4 py-3" />
              </tr>
            </thead>
            <tbody>
              {sorted.map((f) => {
                const isSelected = selected.has(f.name)
                const fullPath = joinPath(path, f.name)
                return (
                  <tr
                    key={f.name}
                    className={`border-border border-b transition-colors last:border-0 ${isSelected ? "bg-accent/60" : "hover:bg-muted/45"}`}
                  >
                    <td className="px-4 py-3">
                      {!f.isDirectory && (
                        <input
                          type="checkbox"
                          checked={isSelected}
                          onChange={() => toggleSelect(f.name)}
                          className="accent-primary"
                        />
                      )}
                    </td>
                    <td className="px-4 py-3">
                      <button
                        className="flex min-w-0 items-center gap-3 text-left"
                        onClick={() =>
                          f.isDirectory ? navigate(fullPath) : toggleSelect(f.name)
                        }
                      >
                        {f.isDirectory ? (
                          <span className="flex size-9 shrink-0 items-center justify-center rounded-md border border-border bg-muted text-foreground">
                            <Folder size={17} />
                          </span>
                        ) : (
                          <span className="flex size-9 shrink-0 items-center justify-center rounded-md border bg-muted text-muted-foreground">
                            <File size={17} />
                          </span>
                        )}
                        <span className="max-w-[200px] truncate font-medium sm:max-w-xs">
                          {f.name}
                        </span>
                      </button>
                    </td>
                    <td className="text-muted-foreground hidden px-4 py-3 text-right sm:table-cell">
                      {f.isDirectory ? "—" : formatSize(f.size)}
                    </td>
                    <td className="px-4 py-3">
                      <div className="flex items-center justify-end gap-1">
                        {!f.isDirectory && (
                          <a
                            href={downloadUrl(fullPath)}
                            download={f.name}
                            title="Download"
                          >
                            <Button variant="ghost" size="sm" className="h-7 w-7 p-0">
                              <Download size={13} />
                            </Button>
                          </a>
                        )}
                        <Button
                          variant="ghost"
                          size="sm"
                          className="h-7 w-7 p-0"
                          title="Rename"
                          onClick={() => {
                            setRenameTarget(f.name)
                            setRenameName(f.name)
                          }}
                        >
                          <PencilSimple size={13} />
                        </Button>
                        {!f.isDirectory && (
                          <Button
                            variant="ghost"
                            size="sm"
                            className="h-7 w-7 p-0"
                            title="Move"
                            onClick={() => setMoveTarget(fullPath)}
                          >
                            <ArrowsOut size={13} />
                          </Button>
                        )}
                      </div>
                    </td>
                  </tr>
                )
              })}
            </tbody>
          </table>
        )}
      </section>

      {/* Upload dialog */}
      <Dialog open={showUpload} onOpenChange={(o) => !o && !uploadState.uploading && setShowUpload(false)}>
        <DialogContent className="max-w-sm">
          <DialogHeader>
            <DialogTitle>Upload Files</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            <div
              className="border-border hover:border-primary rounded-lg border border-dashed bg-muted/20 p-8 text-center transition-colors"
              onDragOver={(e) => e.preventDefault()}
              onDrop={(e) => {
                e.preventDefault()
                const dropped = Array.from(e.dataTransfer.files)
                setUploadState((s) => ({ ...s, files: dropped }))
              }}
            >
              <UploadSimple size={24} className="text-muted-foreground mx-auto mb-2" />
              <p className="text-muted-foreground text-sm">
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
                      className="bg-muted flex items-center justify-between rounded px-3 py-1.5 text-xs"
                    >
                      <span className="truncate">
                        {willConvert ? (
                          <>
                            {f.name}
                            <span className="text-muted-foreground"> → {finalName}</span>
                          </>
                        ) : (
                          f.name
                        )}
                      </span>
                      <span className="text-muted-foreground ml-2 shrink-0">
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
                  <span className="text-muted-foreground truncate">{uploadState.current}</span>
                  <span>{uploadState.progress}%</span>
                </div>
                <Progress value={uploadState.progress} className="h-1.5" />
              </div>
            )}
          </div>
          <DialogFooter>
            <Button
              variant="outline"
              onClick={() => setShowUpload(false)}
              disabled={uploadState.uploading}
            >
              <X size={14} className="mr-1" /> Cancel
            </Button>
            <Button
              onClick={handleUpload}
              disabled={uploadState.files.length === 0 || uploadState.uploading}
            >
              {uploadState.uploading ? "Uploading..." : "Upload"}
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>

      {/* New folder dialog */}
      <Dialog open={showNewFolder} onOpenChange={(o) => !o && setShowNewFolder(false)}>
        <DialogContent className="max-w-sm">
          <DialogHeader>
            <DialogTitle>New Folder</DialogTitle>
          </DialogHeader>
          <div className="space-y-3">
            <Label htmlFor="folder-name">Folder name</Label>
            <Input
              id="folder-name"
              value={newFolderName}
              onChange={(e) => setNewFolderName(e.target.value)}
              onKeyDown={(e) => e.key === "Enter" && handleCreateFolder()}
              placeholder="My Folder"
              autoFocus
            />
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={() => setShowNewFolder(false)}>
              Cancel
            </Button>
            <Button onClick={handleCreateFolder}>Create</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>

      {/* Rename dialog */}
      <Dialog open={!!renameTarget} onOpenChange={(o) => !o && setRenameTarget(null)}>
        <DialogContent className="max-w-sm">
          <DialogHeader>
            <DialogTitle>Rename</DialogTitle>
          </DialogHeader>
          <div className="space-y-3">
            <Label htmlFor="rename-input">New name</Label>
            <Input
              id="rename-input"
              value={renameName}
              onChange={(e) => setRenameName(e.target.value)}
              onKeyDown={(e) => e.key === "Enter" && handleRename()}
              autoFocus
            />
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={() => setRenameTarget(null)}>
              Cancel
            </Button>
            <Button onClick={handleRename}>Rename</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>

      {/* Move dialog */}
      <MoveDialog
        open={!!moveTarget}
        onClose={() => setMoveTarget(null)}
        sourcePath={moveTarget ?? ""}
        onMoved={() => load(path)}
      />
    </div>
  )
}
