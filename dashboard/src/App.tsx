import { HashRouter, Routes, Route } from "react-router-dom"
import { Toaster } from "@/components/ui/sonner"
import { Nav } from "@/components/nav"
import { HomePage } from "@/pages/home"
import { FilesPage } from "@/pages/files"
import { SettingsPage } from "@/pages/settings"
import { BooksPage } from "@/pages/books"
import { BooksDataProvider } from "@/lib/booksData"

export default function App() {
  return (
    <HashRouter>
      <div className="min-h-svh">
        <Nav />
        <main className="mx-auto w-full max-w-7xl px-4 py-6 sm:px-6 lg:px-8">
          <BooksDataProvider>
            <Routes>
              <Route path="/" element={<HomePage />} />
              <Route path="/files" element={<FilesPage />} />
              <Route path="/settings" element={<SettingsPage />} />
              <Route path="/books" element={<BooksPage />} />
            </Routes>
          </BooksDataProvider>
        </main>
        <Toaster position="bottom-right" richColors />
      </div>
    </HashRouter>
  )
}
