export function DetailField({ label, value }: { label: string; value: string }) {
  if (!value) return null
  return (
    <div className="space-y-0.5">
      <p className="text-muted-foreground text-xs font-medium uppercase tracking-wide">{label}</p>
      <p className="text-sm">{value}</p>
    </div>
  )
}
