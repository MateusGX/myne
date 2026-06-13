import type { ReactNode } from "react"
import { cn } from "@/lib/utils"

type PageHeaderProps = {
  eyebrow: string
  title: string
  description?: string
  actions?: ReactNode
}

export function PageHeader({ eyebrow, title, description, actions }: PageHeaderProps) {
  return (
    <section className="dashboard-section flex flex-col justify-between gap-4 md:flex-row md:items-end">
      <div className="min-w-0">
        <p className="text-muted-foreground text-xs font-semibold uppercase tracking-wider">
          {eyebrow}
        </p>
        <h1 className="mt-2 text-3xl font-semibold tracking-tight text-foreground md:text-4xl">
          {title}
        </h1>
        {description && (
          <p className="text-muted-foreground mt-2 max-w-2xl text-sm leading-6">
            {description}
          </p>
        )}
      </div>
      {actions && <div className="flex shrink-0 flex-wrap items-center gap-2">{actions}</div>}
    </section>
  )
}

export function Toolbar({ className, children }: { className?: string; children: ReactNode }) {
  return (
    <section className={cn("flat-panel flex flex-wrap items-center gap-3 p-3", className)}>
      {children}
    </section>
  )
}

export function MetricCard({
  label,
  value,
  detail,
  icon,
  className,
}: {
  label: string
  value: ReactNode
  detail?: ReactNode
  icon?: ReactNode
  className?: string
}) {
  return (
    <div className={cn("flat-card p-4", className)}>
      <div className="flex items-start justify-between gap-3">
        <div>
          <p className="text-muted-foreground text-xs font-medium">{label}</p>
          <div className="mt-2 text-2xl font-semibold tracking-tight">{value}</div>
        </div>
        {icon && (
          <div className="grid size-9 shrink-0 place-items-center rounded-md border bg-muted/40 text-muted-foreground">
            {icon}
          </div>
        )}
      </div>
      {detail && <div className="text-muted-foreground mt-2 text-xs leading-5">{detail}</div>}
    </div>
  )
}

export function EmptyState({
  icon,
  title,
  description,
}: {
  icon?: ReactNode
  title: string
  description?: string
}) {
  return (
    <div className="flat-panel flex flex-col items-center justify-center px-6 py-16 text-center">
      {icon && <div className="text-muted-foreground mb-4">{icon}</div>}
      <p className="font-medium">{title}</p>
      {description && (
        <p className="text-muted-foreground mt-1 max-w-sm text-sm">{description}</p>
      )}
    </div>
  )
}
