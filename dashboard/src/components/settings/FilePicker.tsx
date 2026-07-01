import { type ChangeEvent, type RefObject } from "react"
import { File as FileIcon, UploadSimple, X } from "@phosphor-icons/react"
import { Button } from "@/components/ui/button"
import { Label } from "@/components/ui/label"
import { formatSize } from "@/lib/fileUtils"
import { cn } from "@/lib/utils"

interface FilePickerProps {
  id: string
  label: string
  accept: string
  selectedFile: File | null
  inputRef: RefObject<HTMLInputElement | null>
  onChange: (event: ChangeEvent<HTMLInputElement>) => void
  onClear: () => void
  disabled?: boolean
}

export function FilePicker({
  id,
  label,
  accept,
  selectedFile,
  inputRef,
  onChange,
  onClear,
  disabled = false,
}: FilePickerProps) {
  return (
    <div className="space-y-2">
      <Label htmlFor={id} className="text-xs">
        {label}
      </Label>
      <input
        id={id}
        ref={inputRef}
        type="file"
        accept={accept}
        onChange={onChange}
        disabled={disabled}
        className="sr-only"
      />
      <div
        className={cn(
          "flex min-h-14 items-center gap-2 rounded-lg border border-input bg-background p-2 transition-colors",
          selectedFile && "border-primary/35 bg-primary/5"
        )}
      >
        <Button
          type="button"
          variant="outline"
          size="sm"
          onClick={() => inputRef.current?.click()}
          disabled={disabled}
        >
          <UploadSimple size={14} />
          Choose file
        </Button>

        <div className="flex min-w-0 flex-1 items-center gap-2 px-1">
          <span
            className={cn(
              "flex size-8 shrink-0 items-center justify-center rounded-md border border-border bg-muted text-muted-foreground",
              selectedFile && "border-primary/25 bg-primary/10 text-primary"
            )}
          >
            <FileIcon size={15} />
          </span>
          <div className="min-w-0">
            <p
              className={cn(
                "truncate text-sm font-medium",
                !selectedFile && "text-muted-foreground"
              )}
            >
              {selectedFile ? selectedFile.name : "No file selected"}
            </p>
            {selectedFile && (
              <p className="text-xs text-muted-foreground">
                {formatSize(selectedFile.size)}
              </p>
            )}
          </div>
        </div>

        {selectedFile && (
          <Button
            type="button"
            variant="ghost"
            size="icon-sm"
            onClick={onClear}
            disabled={disabled}
            aria-label="Clear selected file"
          >
            <X size={14} />
          </Button>
        )}
      </div>
    </div>
  )
}
