import { Input } from "@/components/ui/input"
import { Switch } from "@/components/ui/switch"
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select"
import { type Setting } from "@/lib/api"

export function SettingControl({
  setting,
  value,
  onChange,
}: {
  setting: Setting
  value: number | string
  onChange: (v: number | string) => void
}) {
  if (setting.type === "toggle") {
    return (
      <Switch
        checked={Number(value) === 1}
        onCheckedChange={(c) => onChange(c ? 1 : 0)}
      />
    )
  }

  if (setting.type === "enum" && setting.options) {
    return (
      <Select
        value={setting.options[Number(value)]}
        onValueChange={(v) => {
          const index = setting.options!.indexOf(v!)
          if (index !== undefined && index >= 0) {
            onChange(index)
          }
        }}
      >
        <SelectTrigger className="h-8 w-44 text-xs">
          <SelectValue />
        </SelectTrigger>
        <SelectContent>
          {setting.options.map((opt) => (
            <SelectItem key={opt} value={opt} className="text-xs">
              {opt}
            </SelectItem>
          ))}
        </SelectContent>
      </Select>
    )
  }

  if (setting.type === "value") {
    return (
      <Input
        type="number"
        className="h-8 w-24 text-xs"
        value={String(value)}
        min={setting.min}
        max={setting.max}
        step={setting.step}
        onChange={(e) => onChange(Number(e.target.value))}
      />
    )
  }

  if (setting.type === "string") {
    const isPassword = setting.name.toLowerCase().includes("password")
    return (
      <Input
        type={isPassword ? "password" : "text"}
        className="h-8 w-44 text-xs"
        value={String(value)}
        onChange={(e) => onChange(e.target.value)}
        placeholder={isPassword ? "••••••••" : undefined}
      />
    )
  }

  return null
}
