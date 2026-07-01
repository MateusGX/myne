import { useRef, useState } from "react"
import { HardDrive } from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Progress } from "@/components/ui/progress"
import { FilePicker } from "@/components/settings/FilePicker"
import { flashFirmware, getStatus, uploadViaWebSocket } from "@/lib/api"

type FirmwareState =
  | { phase: "idle" }
  | { phase: "uploading"; progress: number }
  | { phase: "ready"; fileName: string }
  | { phase: "flashing" }
  | { phase: "rebooting" }
  | { phase: "done" }
  | { phase: "error"; message: string }

export function FirmwareUpdateCard() {
  const [state, setState] = useState<FirmwareState>({ phase: "idle" })
  const [selectedFile, setSelectedFile] = useState<File | null>(null)
  const inputRef = useRef<HTMLInputElement>(null)

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0]
    if (!file) return
    if (!file.name.endsWith(".bin")) {
      toast.error("Please select a .bin firmware file")
      if (inputRef.current) inputRef.current.value = ""
      return
    }
    setSelectedFile(file)
    setState({ phase: "ready", fileName: file.name })
  }

  const handleUploadAndFlash = async () => {
    if (!selectedFile) return

    setState({ phase: "uploading", progress: 0 })
    try {
      await uploadViaWebSocket(
        selectedFile,
        "/",
        (recv, total) => {
          const pct = total > 0 ? Math.round((recv / total) * 100) : 0
          setState({ phase: "uploading", progress: pct })
        },
        "firmware_update.bin"
      )
    } catch (err) {
      setState({
        phase: "error",
        message: err instanceof Error ? err.message : "Upload failed",
      })
      return
    }

    setState({ phase: "flashing" })
    try {
      await flashFirmware()
    } catch {
      setState({ phase: "error", message: "Flash request failed" })
      return
    }

    setState({ phase: "rebooting" })

    // Poll /api/status until device responds after reboot
    const POLL_INTERVAL = 2000
    const TIMEOUT = 3 * 60 * 1000
    const deadline = Date.now() + TIMEOUT
    let gotOffline = false

    const poll = async () => {
      while (Date.now() < deadline) {
        await new Promise((r) => setTimeout(r, POLL_INTERVAL))
        try {
          await getStatus()
          if (gotOffline) {
            setState({ phase: "done" })
            return
          }
        } catch {
          gotOffline = true
        }
      }
      setState({
        phase: "error",
        message: "Device did not come back online within 3 minutes",
      })
    }
    poll()
  }

  const reset = () => {
    setSelectedFile(null)
    setState({ phase: "idle" })
    if (inputRef.current) inputRef.current.value = ""
  }

  return (
    <section className="flat-panel">
      <div className="border-b border-border px-5 py-4">
        <h2 className="flex items-center gap-2 text-base font-semibold tracking-tight">
          <span className="flex size-9 items-center justify-center rounded-md border border-primary/20 bg-primary/10 text-primary">
            <HardDrive size={16} />
          </span>
          Firmware Update
        </h2>
      </div>
      <div className="space-y-4 p-5">
        {(state.phase === "idle" || state.phase === "ready") && (
          <div className="space-y-3">
            <FilePicker
              id="firmware-file"
              label="Firmware file (.bin)"
              accept=".bin"
              selectedFile={selectedFile}
              inputRef={inputRef}
              onChange={handleFileChange}
              onClear={reset}
            />
            {state.phase === "ready" && (
              <div className="flex flex-wrap justify-end gap-2">
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
                  onClick={handleUploadAndFlash}
                >
                  Flash & Restart
                </Button>
              </div>
            )}
          </div>
        )}

        {state.phase === "uploading" && (
          <div className="space-y-2">
            <div className="flex justify-between text-xs">
              <span className="text-muted-foreground">Uploading firmware…</span>
              <span className="font-medium">{state.progress}%</span>
            </div>
            <Progress value={state.progress} className="h-2" />
          </div>
        )}

        {state.phase === "flashing" && (
          <p className="text-center text-sm text-muted-foreground">
            Flashing firmware… do not power off
          </p>
        )}

        {state.phase === "rebooting" && (
          <p className="text-center text-sm text-muted-foreground">
            Device restarting… waiting for it to come back online
          </p>
        )}

        {state.phase === "done" && (
          <div className="space-y-3 text-center">
            <p className="text-sm font-medium">Update complete!</p>
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
