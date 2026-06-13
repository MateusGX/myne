export function formatSize(bytes: number) {
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MB`
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${bytes} B`
}

export function joinPath(base: string, name: string) {
  if (base === "/") return `/${name}`
  return `${base}/${name}`
}

export function parentPath(path: string) {
  if (path === "/" || !path.includes("/", 1)) return "/"
  return path.substring(0, path.lastIndexOf("/")) || "/"
}
