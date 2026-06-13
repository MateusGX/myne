import { useState } from "react"
import { Plus } from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { saveWifiNetwork } from "@/lib/api"

interface NewNetworkFormProps {
  onAdded: () => void
}

export function NewNetworkForm({ onAdded }: NewNetworkFormProps) {
  const [show, setShow] = useState(false)
  const [ssid, setSsid] = useState("")
  const [password, setPassword] = useState("")
  const [saving, setSaving] = useState(false)

  const handleAdd = async () => {
    if (!ssid.trim()) return
    setSaving(true)
    try {
      await saveWifiNetwork({
        ssid: ssid.trim(),
        password: password || undefined,
      })
      toast.success("Network added")
      setSsid("")
      setPassword("")
      setShow(false)
      onAdded()
    } catch {
      toast.error("Failed to add network")
    } finally {
      setSaving(false)
    }
  }

  if (!show) {
    return (
      <Button
        variant="outline"
        size="sm"
        onClick={() => setShow(true)}
        className="w-full"
      >
        <Plus size={14} className="mr-1" />
        Add Network
      </Button>
    )
  }

  return (
    <div className="space-y-3 rounded-lg border border-dashed border-border bg-muted/20 p-4">
      <p className="text-sm font-medium">New Network</p>
      <div className="grid gap-2 sm:grid-cols-2">
        <div className="space-y-1">
          <Label className="text-xs">SSID</Label>
          <Input
            value={ssid}
            onChange={(e) => setSsid(e.target.value)}
            className="h-8 text-xs"
            placeholder="Network name"
            autoFocus
          />
        </div>
        <div className="space-y-1">
          <Label className="text-xs">Password</Label>
          <Input
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            className="h-8 text-xs"
            placeholder="Leave empty for open network"
          />
        </div>
      </div>
      <div className="flex justify-end gap-2">
        <Button
          variant="outline"
          size="sm"
          className="h-7 text-xs"
          onClick={() => setShow(false)}
        >
          Cancel
        </Button>
        <Button
          size="sm"
          className="h-7 text-xs"
          onClick={handleAdd}
          disabled={saving || !ssid.trim()}
        >
          {saving ? "Adding..." : "Add"}
        </Button>
      </div>
    </div>
  )
}
