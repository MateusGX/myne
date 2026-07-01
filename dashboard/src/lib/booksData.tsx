import {
  type Dispatch,
  type ReactNode,
  type SetStateAction,
  useCallback,
  useRef,
  useState,
} from "react"
import {
  getBooks,
  getCollections,
  type Book,
  type Collection,
} from "@/lib/api"
import { BooksDataContext, normalizeCollection } from "@/lib/booksDataContext"

export function BooksDataProvider({ children }: { children: ReactNode }) {
  const [books, setBooks] = useState<Book[]>([])
  const [collections, setCollectionsState] = useState<Collection[]>([])
  const [loading, setLoading] = useState(false)
  const [loaded, setLoaded] = useState(false)
  const loadedRef = useRef(false)
  const inFlightRef = useRef<Promise<void> | null>(null)

  const setCollections: Dispatch<SetStateAction<Collection[]>> = useCallback(
    (value) => {
      setCollectionsState((prev) => {
        const next = typeof value === "function" ? value(prev) : value
        return next.map(normalizeCollection)
      })
    },
    []
  )

  const refresh = useCallback(async (force = true) => {
    if (!force && loadedRef.current) return
    if (inFlightRef.current) return inFlightRef.current

    const request = Promise.all([getBooks(), getCollections()])
      .then(([bookData, collectionData]) => {
        setBooks(bookData)
        setCollectionsState(collectionData.map(normalizeCollection))
        loadedRef.current = true
        setLoaded(true)
      })
      .finally(() => {
        inFlightRef.current = null
        setLoading(false)
      })

    inFlightRef.current = request
    setLoading(true)
    return request
  }, [])

  return (
    <BooksDataContext.Provider
      value={{
        books,
        collections,
        loading,
        loaded,
        refresh,
        setBooks,
        setCollections,
      }}
    >
      {children}
    </BooksDataContext.Provider>
  )
}
