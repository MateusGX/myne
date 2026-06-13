import { useState } from "react"
import { FloppyDisk, Trash, WifiHigh } from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { deleteWifiNetwork, saveWifiNetwork, type WifiNetwork } from "@/lib/api"

interface WifiNetworkRowProps {
  net: WifiNetwork
  onSaved: () => void
  onDeleted: () => void
}

export function WifiNetworkRow({ net, onSaved, onDeleted }: WifiNetworkRowProps) {
  const [ssid, setSsid] = useState(net.ssid)
  const [password, setPassword] = useState("")
  const [saving, setSaving] = useState(false)

  const handleSave = async () => {
    if (!ssid.trim()) return
    setSaving(true)
    try {
      const data: { ssid: string; password?: string; index: number } = {
        ssid: ssid.trim(),
        index: net.index,
      }
      if (password) data.password = password
      await saveWifiNetwork(data)
      toast.success("Network saved")
      setPassword("")
      onSaved()
    } catch {
      toast.error("Failed to save network")
    } finally {
      setSaving(false)
    }
  }

  const handleDelete = async () => {
    try {
      await deleteWifiNetwork(net.index)
      toast.success("Network removed")
      onDeleted()
    } catch {
      toast.error("Failed to delete network")
    }
  }

  return (
    <div className="space-y-3 rounded-lg border border-border bg-card p-4">
      <div className="flex items-center justify-between gap-2">
        <div className="flex items-center gap-2">
          <WifiHigh size={16} className="shrink-0 text-muted-foreground" />
          <span className="text-sm font-medium">
            {net.ssid || "New Network"}
          </span>
        </div>
        {net.isLastConnected && (
          <span className="rounded-md bg-primary/10 px-2 py-1 text-xs font-semibold text-primary">Last connected</span>
        )}
      </div>
      <div className="grid gap-2 sm:grid-cols-2">
        <div className="space-y-1">
          <Label className="text-xs">SSID</Label>
          <Input
            value={ssid}
            onChange={(e) => setSsid(e.target.value)}
            className="h-8 text-xs"
            placeholder="Network name"
          />
        </div>
        <div className="space-y-1">
          <Label className="text-xs">Password</Label>
          <Input
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            className="h-8 text-xs"
            placeholder={net.hasPassword ? "(unchanged)" : "No password"}
          />
        </div>
      </div>
      <div className="flex justify-end gap-2">
        <Button
          variant="outline"
          size="sm"
          onClick={handleDelete}
          className="h-7 text-xs"
        >
          <Trash size={12} className="mr-1" />
          Remove
        </Button>
        <Button
          size="sm"
          onClick={handleSave}
          disabled={saving}
          className="h-7 text-xs"
        >
          <FloppyDisk size={12} className="mr-1" />
          {saving ? "Saving..." : "Save"}
        </Button>
      </div>
    </div>
  )
}
