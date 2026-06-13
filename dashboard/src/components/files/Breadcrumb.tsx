interface BreadcrumbProps {
  path: string
  onNavigate: (p: string) => void
}

export function Breadcrumb({ path, onNavigate }: BreadcrumbProps) {
  const parts = path === "/" ? [] : path.split("/").filter(Boolean)
  return (
    <div className="flex flex-wrap items-center gap-1 text-sm">
      <button
        onClick={() => onNavigate("/")}
        className="rounded-full bg-primary/10 px-2.5 py-1 font-semibold text-primary hover:bg-primary/15"
      >
        root
      </button>
      {parts.map((part, i) => {
        const to = "/" + parts.slice(0, i + 1).join("/")
        const isLast = i === parts.length - 1
        return (
          <span key={to} className="flex items-center gap-1">
            <span className="text-muted-foreground">/</span>
            {isLast ? (
            <span className="rounded-full bg-muted px-2.5 py-1 font-semibold">{part}</span>
          ) : (
            <button
              onClick={() => onNavigate(to)}
              className="rounded-full px-2.5 py-1 text-primary hover:bg-primary/10"
            >
              {part}
            </button>
            )}
          </span>
        )
      })}
    </div>
  )
}
