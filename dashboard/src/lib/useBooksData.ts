import { useContext } from "react"
import { BooksDataContext } from "@/lib/booksDataContext"

export function useBooksData() {
  const context = useContext(BooksDataContext)
  if (!context) {
    throw new Error("useBooksData must be used inside BooksDataProvider")
  }
  return context
}
