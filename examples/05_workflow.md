# Lesson 5 — Workflow

You have learned all the individual pieces. This lesson shows how they
fit together in a real writing session.

---

## A typical session

### 1. Open mde

```bash
# Open an existing file
./mde my-notes.md

# Or start a new file
./mde new-document.md
```

mde starts in **preview mode**. If the file does not exist, it is created
when you first save.

### 2. Orient yourself

- Press `Ctrl+T` to open the **Table of Contents** and get a bird's-eye
  view of the document structure.
- Press `Ctrl+F` to **search** for a specific word or phrase.
- Use `Page Down` / `Page Up` to browse.

### 3. Jump to where you want to write

From the TOC, press `Enter` on the heading you want to edit. The cursor
lands at that heading. Then press `Ctrl+P` to enter **edit mode**.

### 4. Write

- Type normally. `Enter` for new lines, `Tab` for indentation.
- Press `Ctrl+K` to kill from the cursor to end of line.
- Use `Ctrl+A` / `Ctrl+E` (or `Home` / `End`) to jump within a line.
- `Ctrl+Z` undoes a mistake; `Ctrl+Y` redoes it.

### 5. Review as you go

Press `Escape` to flip to **preview mode** whenever you want to see the
rendered result. Press `Ctrl+P` to jump back into edit mode.

### 6. Save often

`Ctrl+S` saves the file. The `[+]` indicator in the status bar disappears
when the file is clean.

### 7. Follow your own links

As your document grows, use internal anchor links to cross-reference
sections. In preview mode, press `Ctrl+L` and type the link number to
jump there instantly.

### 8. Quit

`Ctrl+Q` quits. If there are unsaved changes, mde asks you to press
`Ctrl+Q` again to confirm.

---

## Reference card

### Universal (both modes)

| Key | Action |
|-----|--------|
| `Ctrl+P` | Toggle preview ↔ edit |
| `Ctrl+S` | Save |
| `Ctrl+Q` | Quit |
| `Ctrl+O` | Open file |
| `Ctrl+F` | Search |
| `Ctrl+N` | Next search match |
| `Ctrl+G` | Go to line |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+L` | Follow link by number |
| `Ctrl+T` | Table of Contents |
| `Ctrl+W` | Toggle word wrap |
| `F1` | Help |

### Edit mode only

| Key | Action |
|-----|--------|
| `Ctrl+K` | Delete to end of line |
| `Ctrl+A` / `Home` | Beginning of line |
| `Ctrl+E` / `End` | End of line |
| `Tab` | Insert 4 spaces |
| `Escape` | Return to preview |

### Preview mode only

| Key | Action |
|-----|--------|
| `Arrow Up/Down` | Scroll one line |
| `Shift+Up/Down` | Scroll 10 lines |
| `Page Up/Down` | Scroll one page |
| `Home/End` | Top / bottom of document |

---

## Explore further

- [Back to the start](00_start_here.md) — revisit the tutorial index.
- [Formatting reference](01_formatting.md) — all inline styles and block
  elements.
- [Navigation deep-dive](02_navigation.md) — anchor links, TOC, search.
- [Editing shortcuts](03_editing.md) — every key in edit mode.
- [Tables & code](04_tables_code.md) — box-drawing tables and fenced
  blocks.
- [Full help](../docs/help.md) — the built-in help file (also accessible
  via `F1`).
- [sample.md](../sample.md) — the original feature demo.

---

> **You made it!** You now know everything you need to use mde
> efficiently. Open a real file and start writing.

```bash
./mde ~/notes/today.md
```

---

*Lesson 5 of 5 · [← Tables & Code](04_tables_code.md) · [↑ Start here](00_start_here.md)*
