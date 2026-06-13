import { useLocation, useNavigate } from "react-router-dom"
import { Books, Files, Gear, HouseLine, Moon, Sun, WifiHigh } from "@phosphor-icons/react"
import { useTheme } from "@/components/theme-provider"
import { cn } from "@/lib/utils"

type Route = "/" | "/files" | "/settings" | "/books"

const links: { href: Route; label: string; Icon: React.ComponentType<{ className?: string; size?: number }> }[] = [
  { href: "/", label: "Home", Icon: HouseLine },
  { href: "/files", label: "Files", Icon: Files },
  { href: "/books", label: "Books", Icon: Books },
  { href: "/settings", label: "Settings", Icon: Gear },
]

export function Nav() {
  const location = useLocation()
  const navigate = useNavigate()
  const current = location.pathname as Route
  const { theme, setTheme } = useTheme()
  const isDark = theme === "dark"

  return (
    <header className="sticky top-0 z-10 border-b border-border bg-background">
      <div className="mx-auto flex max-w-7xl flex-col gap-3 px-4 py-3 sm:px-6 lg:flex-row lg:items-center lg:px-8">
        <button
          onClick={() => navigate("/")}
          className="flex w-fit items-center gap-3 rounded-md px-1 py-1 text-left"
        >
          <span className="flex size-10 items-center justify-center rounded-md border border-primary/20 bg-primary/10 text-primary">
            <WifiHigh size={20} weight="bold" />
          </span>
          <span>
            <span className="block text-sm font-semibold tracking-tight">Myne</span>
            <span className="text-muted-foreground block text-xs">Device dashboard</span>
          </span>
        </button>
        <div className="flex flex-wrap items-center gap-2 lg:ml-auto">
          <nav className="flex gap-1 overflow-x-auto rounded-lg border border-border bg-card p-1">
            {links.map(({ href, label, Icon }) => (
              <button
                key={href}
                onClick={() => navigate(href)}
                className={cn(
                  "flex items-center gap-2 rounded-md px-3 py-2 text-xs font-semibold transition-colors",
                  current === href
                    ? "bg-primary text-primary-foreground"
                    : "text-muted-foreground hover:bg-muted hover:text-foreground",
                )}
              >
                <Icon size={16} />
                {label}
              </button>
            ))}
            <button
              type="button"
              onClick={() => setTheme(isDark ? "light" : "dark")}
              className="ml-1 flex size-9 shrink-0 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-muted hover:text-foreground"
              title={isDark ? "Switch to light theme" : "Switch to dark theme"}
              aria-label={isDark ? "Switch to light theme" : "Switch to dark theme"}
            >
              {isDark ? <Sun size={18} /> : <Moon size={18} />}
            </button>
          </nav>
        </div>
      </div>
    </header>
  )
}

export type { Route }
