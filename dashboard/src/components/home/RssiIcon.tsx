import { WifiHigh, WifiLow, WifiMedium, WifiNone } from "@phosphor-icons/react"

export function RssiIcon({ rssi, className }: { rssi: number; className?: string }) {
  if (!Number.isFinite(rssi) || rssi === 0) return <WifiNone className={className} />
  if (rssi >= -65) return <WifiHigh className={className} />
  if (rssi >= -75) return <WifiMedium className={className} />
  if (rssi >= -85) return <WifiLow className={className} />
  return <WifiNone className={className} />
}
