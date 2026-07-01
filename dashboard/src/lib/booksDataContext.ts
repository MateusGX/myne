import { createContext, type Dispatch, type SetStateAction } from "react"
import type { Book, Collection } from "@/lib/api"

export type BooksDataContextValue = {
  books: Book[]
  collections: Collection[]
  loading: boolean
  loaded: boolean
  refresh: (force?: boolean) => Promise<void>
  setBooks: Dispatch<SetStateAction<Book[]>>
  setCollections: Dispatch<SetStateAction<Collection[]>>
}

export const BooksDataContext =
  createContext<BooksDataContextValue | null>(null)

export function normalizeCollection(collection: Collection): Collection {
  return {
    ...collection,
    expectedCount: collection.expectedCount ?? 0,
    initialVolume: collection.initialVolume ?? 0,
    note: collection.note ?? "",
  }
}
