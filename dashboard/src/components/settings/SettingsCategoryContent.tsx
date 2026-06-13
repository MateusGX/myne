import { Label } from "@/components/ui/label"
import { Separator } from "@/components/ui/separator"
import { SettingControl } from "@/components/settings/SettingControl"
import { type Setting } from "@/lib/api"

export interface SettingsValues {
  [key: string]: number | string
}

export function SettingsCategoryContent({
  settings,
  values,
  originals,
  onChange,
}: {
  settings: Setting[]
  values: SettingsValues
  originals: SettingsValues
  onChange: (key: string, val: number | string) => void
}) {
  return (
    <div className="space-y-1">
      {settings.map((s, i) => {
        const isDirty = values[s.key] !== originals[s.key]
        return (
          <div key={s.key}>
            <div className="flex items-center justify-between rounded-xl px-3 py-3 transition-colors hover:bg-accent/25">
              <Label
                htmlFor={`setting-${s.key}`}
                className={`flex-1 pr-4 text-sm ${isDirty ? "font-medium" : ""}`}
              >
                {s.name}
                {isDirty && (
                  <span className="ml-1.5 text-xs text-primary">●</span>
                )}
              </Label>
              <div id={`setting-${s.key}`}>
                <SettingControl
                  setting={s}
                  value={values[s.key] ?? s.value}
                  onChange={(v) => onChange(s.key, v)}
                />
              </div>
            </div>
            {i < settings.length - 1 && <Separator className="opacity-60" />}
          </div>
        )
      })}
    </div>
  )
}
