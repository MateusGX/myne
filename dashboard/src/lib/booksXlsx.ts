import * as XLSX from "xlsx"
import type { Book, BookFormData } from "./api"

const BOOKS_SHEET = "Books"
const NOTES_SHEET = "Collection Notes"

const BOOK_HEADERS = ["Title", "Author", "Collection", "Volume", "Location", "Notes"]
const NOTE_HEADERS = ["Collection", "Note"]

const EXAMPLE_BOOK = ["The Hobbit", "J.R.R. Tolkien", "Middle-earth", "", "Shelf 2", "Gift from Sarah"]
const EXAMPLE_NOTE = ["Middle-earth", "Read in publication order"]

function buildWorkbook(bookRows: unknown[][], noteRows: unknown[][]) {
  const wb = XLSX.utils.book_new()
  XLSX.utils.book_append_sheet(wb, XLSX.utils.aoa_to_sheet([BOOK_HEADERS, ...bookRows]), BOOKS_SHEET)
  XLSX.utils.book_append_sheet(wb, XLSX.utils.aoa_to_sheet([NOTE_HEADERS, ...noteRows]), NOTES_SHEET)
  return wb
}

/** Download a blank .xlsx with the expected "Books" / "Collection Notes" sheets and an example row. */
export function downloadBooksTemplate() {
  const wb = buildWorkbook([EXAMPLE_BOOK], [EXAMPLE_NOTE])
  XLSX.writeFile(wb, "myne-books-template.xlsx")
}

/** Export the current library + collection notes as a 2-sheet .xlsx (mirrors the import template). */
export function exportBooksXlsx(books: Book[], collectionNotes: Record<string, string>) {
  const bookRows = books.map((b) => [b.title, b.author, b.collection, b.volume, b.location, b.notes])
  const noteRows = Object.entries(collectionNotes)
    .filter(([, note]) => note)
    .map(([name, note]) => [name, note])
  XLSX.writeFile(buildWorkbook(bookRows, noteRows), "myne-books.xlsx")
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

function findSheet(workbook: XLSX.WorkBook, name: string): XLSX.WorkSheet | undefined {
  const sheetName = workbook.SheetNames.find((n) => n.trim().toLowerCase() === name.toLowerCase())
  return sheetName ? workbook.Sheets[sheetName] : undefined
}

export type ParsedBooksImport = { books: BookFormData[]; collectionNotes: Record<string, string> }

/** Parse a .xlsx file with a "Books" sheet and an optional "Collection Notes" sheet. */
export async function parseBooksXlsx(file: File): Promise<ParsedBooksImport> {
  const buf = await file.arrayBuffer()
  const workbook = XLSX.read(buf, { type: "array" })

  const booksSheet = findSheet(workbook, BOOKS_SHEET) ?? workbook.Sheets[workbook.SheetNames[0]]
  const bookRows = booksSheet ? XLSX.utils.sheet_to_json<Record<string, unknown>>(booksSheet, { defval: "" }) : []

  const books: BookFormData[] = bookRows
    .map((row) => ({
      title: rowValue(row, "Title"),
      author: rowValue(row, "Author"),
      collection: rowValue(row, "Collection"),
      volume: rowValue(row, "Volume"),
      location: rowValue(row, "Location"),
      notes: rowValue(row, "Notes"),
    }))
    .filter((b) => b.title)

  if (books.length === 0) throw new Error("No valid books found")

  const collectionNotes: Record<string, string> = {}
  const notesSheet = findSheet(workbook, NOTES_SHEET)
  if (notesSheet) {
    for (const row of XLSX.utils.sheet_to_json<Record<string, unknown>>(notesSheet, { defval: "" })) {
      const collection = rowValue(row, "Collection")
      const note = rowValue(row, "Note")
      if (collection && note) collectionNotes[collection] = note
    }
  }

  return { books, collectionNotes }
}
