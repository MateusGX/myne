export function BadgeLike({ value, label }: { value: number; label: string }) {
  return (
    <div className="flat-card min-w-28 px-4 py-3">
      <p className="text-xl font-semibold tracking-tight">{value}</p>
      <p className="text-muted-foreground text-xs font-medium">{label}</p>
    </div>
  )
}
