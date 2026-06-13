import { useEffect, useState } from "react"
import { FloppyDisk, WifiHigh } from "@phosphor-icons/react"
import { toast } from "sonner"
import { Button } from "@/components/ui/button"
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs"
import { PageHeader, Toolbar } from "@/components/dashboard-layout"
import { SettingsCategoryContent, type SettingsValues } from "@/components/settings/SettingsCategoryContent"
import { WifiNetworkRow } from "@/components/settings/WifiNetworkRow"
import { NewNetworkForm } from "@/components/settings/NewNetworkForm"
import { FirmwareUpdateCard } from "@/components/settings/FirmwareUpdateCard"
import {
  getSettings,
  getWifiNetworks,
  saveSettings,
  type Setting,
  type WifiNetwork,
} from "@/lib/api"

export function SettingsPage() {
  const [settings, setSettings] = useState<Setting[]>([])
  const [values, setValues] = useState<SettingsValues>({})
  const [originals, setOriginals] = useState<SettingsValues>({})
  const [wifiNetworks, setWifiNetworks] = useState<WifiNetwork[]>([])
  const [saving, setSaving] = useState(false)
  const [loadingSettings, setLoadingSettings] = useState(true)

  const loadSettings = () => {
    setLoadingSettings(true)
    getSettings()
      .then((s) => {
        setSettings(s)
        const init: SettingsValues = {}
        s.forEach((x) => (init[x.key] = x.value))
        setValues(init)
        setOriginals(init)
      })
      .catch(() => toast.error("Failed to load settings"))
      .finally(() => setLoadingSettings(false))
  }

  const loadWifi = () => {
    getWifiNetworks()
      .then(setWifiNetworks)
      .catch(() => toast.error("Failed to load Wi-Fi networks"))
  }

  useEffect(() => {
    loadSettings()
    loadWifi()
  }, [])

  const handleChange = (key: string, val: number | string) => {
    setValues((prev) => ({ ...prev, [key]: val }))
  }

  const handleSave = async () => {
    const changes: Record<string, unknown> = {}
    for (const key of Object.keys(values)) {
      if (values[key] !== originals[key]) {
        changes[key] = values[key]
      }
    }
    if (Object.keys(changes).length === 0) {
      toast.info("No changes to save")
      return
    }
    setSaving(true)
    try {
      await saveSettings(changes)
      setOriginals({ ...values })
      toast.success("Settings saved")
    } catch {
      toast.error("Failed to save settings")
    } finally {
      setSaving(false)
    }
  }

  const categories = [...new Set(settings.map((s) => s.category))]
  const isDirty = Object.keys(values).some((k) => values[k] !== originals[k])

  if (loadingSettings) {
    return (
      <div className="space-y-6">
        <div>
          <div className="bg-muted mb-3 h-8 w-48 animate-pulse rounded-full" />
          <div className="bg-muted h-4 w-80 animate-pulse rounded-full" />
        </div>
        {Array.from({ length: 3 }).map((_, i) => (
          <div key={i} className="flat-panel p-5">
              <div className="h-32 animate-pulse rounded bg-muted" />
          </div>
        ))}
      </div>
    )
  }

  return (
    <div className="space-y-6">
      <PageHeader
        eyebrow="Configuration"
        title="Settings"
        description="Tune device preferences, Wi-Fi profiles and firmware updates from one place."
        actions={isDirty && (
          <span className="w-fit rounded-full bg-primary/10 px-3 py-1.5 text-xs font-semibold text-primary">
            Unsaved changes
          </span>
        )}
      />

      <Tabs defaultValue={categories[0] ?? ""}>
        <Toolbar className="justify-between">
          <TabsList>
            {categories.map((cat) => (
              <TabsTrigger key={cat} value={cat} className="text-xs">
                {cat}
              </TabsTrigger>
            ))}
          </TabsList>
          <Button onClick={handleSave} disabled={saving || !isDirty} size="sm">
            <FloppyDisk size={14} className="mr-1" />
            {saving ? "Saving..." : "Save"}
          </Button>
        </Toolbar>

        {categories.map((cat) => {
          const catSettings = settings.filter((s) => s.category === cat)
          return (
            <TabsContent key={cat} value={cat} className="mt-4">
              <section className="flat-panel">
                <div className="border-b border-border px-5 py-4">
                  <h2 className="text-base font-semibold tracking-tight">{cat}</h2>
                </div>
                <div className="p-3">
                  <SettingsCategoryContent
                    settings={catSettings}
                    values={values}
                    originals={originals}
                    onChange={handleChange}
                  />
                </div>
              </section>
            </TabsContent>
          )
        })}
      </Tabs>

      <section className="flat-panel">
        <div className="border-b border-border px-5 py-4">
          <h2 className="flex items-center gap-2 text-base font-semibold tracking-tight">
            <span className="flex size-9 items-center justify-center rounded-md border border-primary/20 bg-primary/10 text-primary">
              <WifiHigh size={16} />
            </span>
            Wi-Fi Networks
          </h2>
        </div>
        <div className="space-y-3 p-5">
          {wifiNetworks.length === 0 ? (
            <p className="py-2 text-center text-sm text-muted-foreground">
              No saved networks
            </p>
          ) : (
            wifiNetworks.map((net) => (
              <WifiNetworkRow
                key={net.index}
                net={net}
                onSaved={loadWifi}
                onDeleted={loadWifi}
              />
            ))
          )}
          <NewNetworkForm onAdded={loadWifi} />
        </div>
      </section>

      <FirmwareUpdateCard />
    </div>
  )
}
