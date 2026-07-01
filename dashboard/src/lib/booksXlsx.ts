import * as XLSX from "xlsx"
import type { Book, Collection } from "./api"

const BOOKS_SHEET = "Books"
const COLLECTION_SHEET = "Collection"
const LEGACY_NOTES_SHEET = "Collection Notes"

const BOOK_HEADERS = ["ID", "Title", "Author", "Collection", "Volume", "Location", "Notes"]
const COLLECTION_HEADERS = ["ID", "Collection", "Note", "Expected Count", "Initial Volume"]

const EXAMPLE_BOOK = ["", "The Hobbit", "J.R.R. Tolkien", "Middle-earth", "", "Shelf 2", "Gift from Sarah"]
const EXAMPLE_COLLECTION = ["", "Middle-earth", "Read in publication order", 7, 1]

export type CollectionMetadata = {
  id: string
  name: string
  note: string
  expectedCount: number
  initialVolume: number
}

function buildWorkbook(bookRows: unknown[][], collectionRows: unknown[][]) {
  const wb = XLSX.utils.book_new()
  XLSX.utils.book_append_sheet(wb, XLSX.utils.aoa_to_sheet([BOOK_HEADERS, ...bookRows]), BOOKS_SHEET)
  XLSX.utils.book_append_sheet(
    wb,
    XLSX.utils.aoa_to_sheet([COLLECTION_HEADERS, ...collectionRows]),
    COLLECTION_SHEET,
  )
  return wb
}

/** Download a blank .xlsx with the expected "Books" / "Collection" sheets and an example row. */
export function downloadBooksTemplate() {
  const wb = buildWorkbook([EXAMPLE_BOOK], [EXAMPLE_COLLECTION])
  XLSX.writeFile(wb, "myne-books-template.xlsx")
}

/** Export the current library + collection metadata as a 2-sheet .xlsx (mirrors the import template). */
export function exportBooksXlsx(
  books: Book[],
  collections: Collection[],
  collectionNotes: Record<string, string>,
) {
  const bookRows = books.map((b) => [b.id, b.title, b.author, b.collection, b.volume, b.location, b.notes])
  const collectionRows = collections.map((collection) => [
    collection.id,
    collection.name,
    collectionNotes[collection.id] ?? "",
    collection.expectedCount || "",
    collection.initialVolume || "",
  ])
  XLSX.writeFile(buildWorkbook(bookRows, collectionRows), "myne-books.xlsx")
}

// Case/whitespace-insensitive lookup so manually-edited templates still parse.
function rowValue(row: Record<string, unknown>, key: string): string {
  for (const rowKey of Object.keys(row)) {
    if (rowKey.trim().toLowerCase() === key.toLowerCase()) {
      const v = row[rowKey]
      return v === undefined || v === null ? "" : String(v).trim()
    }
  }
  return ""
}

function rowInt(row: Record<string, unknown>, key: string): number {
  const parsed = Math.floor(Number(rowValue(row, key)))
  return Number.isFinite(parsed) ? Math.max(0, parsed) : 0
}

function findSheet(workbook: XLSX.WorkBook, name: string): XLSX.WorkSheet | undefined {
  const sheetName = workbook.SheetNames.find((n) => n.trim().toLowerCase() === name.toLowerCase())
  return sheetName ? workbook.Sheets[sheetName] : undefined
}

export type ParsedBooksImport = {
  books: Book[]
  collectionMetadata: CollectionMetadata[]
}

/** Parse a .xlsx file with a "Books" sheet and an optional "Collection" metadata sheet. */
export async function parseBooksXlsx(file: File): Promise<ParsedBooksImport> {
  const buf = await file.arrayBuffer()
  const workbook = XLSX.read(buf, { type: "array" })

  const booksSheet = findSheet(workbook, BOOKS_SHEET) ?? workbook.Sheets[workbook.SheetNames[0]]
  const bookRows = booksSheet ? XLSX.utils.sheet_to_json<Record<string, unknown>>(booksSheet, { defval: "" }) : []

  const books: Book[] = bookRows
    .map((row) => ({
      id: rowValue(row, "ID"),
      title: rowValue(row, "Title"),
      author: rowValue(row, "Author"),
      collection: rowValue(row, "Collection"),
      volume: rowValue(row, "Volume"),
      location: rowValue(row, "Location"),
      notes: rowValue(row, "Notes"),
    }))
    .filter((b) => b.title)

  const collectionMetadata: CollectionMetadata[] = []
  const collectionSheet = findSheet(workbook, COLLECTION_SHEET) ?? findSheet(workbook, LEGACY_NOTES_SHEET)
  if (collectionSheet) {
    for (const row of XLSX.utils.sheet_to_json<Record<string, unknown>>(collectionSheet, { defval: "" })) {
      const id = rowValue(row, "ID")
      const collection = rowValue(row, "Collection")
      const note = rowValue(row, "Note")
      const expectedCount = rowInt(row, "Expected Count")
      const initialVolume = rowInt(row, "Initial Volume")
      if (id || collection || note || expectedCount > 0 || initialVolume > 0) {
        collectionMetadata.push({
          id,
          name: collection,
          note,
          expectedCount,
          initialVolume,
        })
      }
    }
  }

  if (books.length === 0 && collectionMetadata.length === 0) throw new Error("No valid rows found")

  return { books, collectionMetadata }
}
