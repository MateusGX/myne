import { useEffect, useState } from "react"
import { Cpu, HardDrive, Timer, WifiHigh, WifiSlash } from "@phosphor-icons/react"
import { Badge } from "@/components/ui/badge"
import { Progress } from "@/components/ui/progress"
import { Skeleton } from "@/components/ui/skeleton"
import { EmptyState, MetricCard, PageHeader } from "@/components/dashboard-layout"
import { RssiIcon } from "@/components/home/RssiIcon"
import { getStatus, type Status } from "@/lib/api"

function formatUptime(seconds: number) {
  if (!Number.isFinite(seconds) || seconds <= 0) return "—"
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  const s = seconds % 60
  if (h > 0) return `${h}h ${m}m`
  if (m > 0) return `${m}m ${s}s`
  return `${s}s`
}

function formatBytes(bytes: number) {
  if (!Number.isFinite(bytes) || bytes <= 0) return "—"
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MB`
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${bytes} B`
}

function rssiLabel(rssi: number) {
  if (!Number.isFinite(rssi) || rssi === 0) return "Unavailable"
  if (rssi >= -55) return "Excellent"
  if (rssi >= -65) return "Good"
  if (rssi >= -75) return "Fair"
  return "Weak"
}

export function HomePage() {
  const [status, setStatus] = useState<Status | null>(null)
  const [error, setError] = useState(false)

  useEffect(() => {
    getStatus()
      .then(setStatus)
      .catch(() => setError(true))
  }, [])

  if (error) {
    return (
      <EmptyState
        icon={<WifiSlash size={36} />}
        title="Device unreachable"
        description="Make sure you're connected to the device network."
      />
    )
  }

  if (!status) {
    return (
      <div className="space-y-6">
        <div>
          <Skeleton className="mb-3 h-8 w-48 rounded-full" />
          <Skeleton className="h-4 w-80 rounded-full" />
        </div>
        <div className="flat-panel min-h-64 p-6">
            <Skeleton className="mb-3 h-5 w-32" />
            <Skeleton className="mb-2 h-8 w-48" />
            <Skeleton className="h-4 w-24" />
        </div>
        <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-4">
          {Array.from({ length: 4 }).map((_, i) => (
            <div key={i} className="flat-card p-5">
                <Skeleton className="mb-2 h-4 w-16" />
                <Skeleton className="h-7 w-24" />
            </div>
          ))}
        </div>
      </div>
    )
  }

  const isAp = status.mode === "AP"
  const hasStorageUsed = status.storageUsed > 0
  const hasStorageTotal = status.storageTotal > 0
  const hasSignal = !isAp && Number.isFinite(status.rssi) && status.rssi !== 0
  const storagePercent =
    hasStorageTotal
      ? Math.min(100, Math.round((status.storageUsed / status.storageTotal) * 100))
      : 0
  const storageDetail = hasStorageTotal
    ? `of ${formatBytes(status.storageTotal)} used`
    : hasStorageUsed
      ? "Used space"
      : "Storage details unavailable"
  const storageStatus = hasStorageTotal
    ? `${storagePercent}% full`
    : hasStorageUsed
      ? "Total capacity unavailable"
      : "Waiting for storage stats"

  return (
    <div className="space-y-6">
      <PageHeader
        eyebrow="Live device"
        title="Myne dashboard"
        description="Monitor connectivity, storage headroom and runtime from the web dashboard."
        actions={
          <Badge variant={isAp ? "secondary" : "default"} className="w-fit">
          {isAp ? "Hotspot mode" : "Connected Wi-Fi"}
          </Badge>
        }
      />

      <section className="flat-panel grid gap-5 p-5 lg:grid-cols-[1.35fr_.65fr]">
          <div className="rounded-lg border border-border bg-muted/25 p-5">
            <div className="flex flex-wrap items-center justify-between gap-3">
              <div className="flex items-center gap-2">
                <span className="h-2.5 w-2.5 rounded-full bg-primary" />
                <span className="text-primary text-sm font-semibold">Online</span>
              </div>
              <RssiIcon rssi={isAp ? -50 : status.rssi} className="text-primary" />
            </div>

            <div className="mt-8">
              <p className="text-muted-foreground text-xs font-semibold uppercase tracking-wide">Device IP address</p>
              <p className="mt-2 break-all font-mono text-3xl font-bold tracking-tight sm:text-4xl">
                {status.ip || "Unavailable"}
              </p>
            </div>

            <p className="text-muted-foreground mt-4 max-w-lg text-sm">
              Use this address to access Myne from devices on the same network.
            </p>
          </div>

          <div className="grid gap-3 sm:grid-cols-2 lg:grid-cols-1">
            <div className="rounded-lg border border-border bg-card p-5">
              <p className="text-muted-foreground text-xs font-semibold uppercase tracking-wide">Firmware</p>
              <p className="mt-2 text-xl font-semibold tracking-tight">{status.version || "Unknown"}</p>
            </div>
            <div className="rounded-lg border border-border bg-card p-5">
              <p className="text-muted-foreground text-xs font-semibold uppercase tracking-wide">Network mode</p>
              <p className="mt-2 text-xl font-semibold tracking-tight">{isAp ? "Access Point" : "Station"}</p>
            </div>
          </div>
      </section>

      <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-4">
        <MetricCard
          label="Storage"
          value={formatBytes(status.storageUsed)}
          icon={<HardDrive size={18} />}
          detail={
            <>
              {storageDetail}
              {hasStorageTotal && <Progress value={storagePercent} className="mt-4 h-2" />}
              <p className={hasStorageTotal ? "mt-2" : "mt-4"}>{storageStatus}</p>
            </>
          }
        />
        <MetricCard label="Uptime" value={formatUptime(status.uptime)} icon={<Timer size={18} />} />
        <MetricCard
          label="Signal"
          value={hasSignal ? `${status.rssi} dBm` : "—"}
          icon={isAp ? <WifiHigh size={18} /> : <RssiIcon rssi={status.rssi} />}
          detail={isAp ? "Hotspot mode" : rssiLabel(status.rssi)}
        />
        <MetricCard label="Processor" value="ESP32-C3" icon={<Cpu size={18} />} detail="160 MHz · RISC-V" />
      </div>
    </div>
  )
}
