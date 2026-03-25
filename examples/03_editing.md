# Lesson 3 — Editing

This lesson is best experienced in **edit mode**. Press `Ctrl+P` now to
switch, then come back to preview (`Escape`) to read the explanations.

---

## Entering and leaving edit mode

| Action | Key |
|--------|-----|
| Switch to edit mode | `Ctrl+P` (from preview) |
| Return to preview | `Escape` or `Ctrl+P` (from edit) |

The status bar at the bottom shows `[PREVIEW]` in preview mode and a `[+]`
indicator when there are unsaved changes in edit mode.

---

## Cursor movement in edit mode

| Key | Action |
|-----|--------|
| Arrow keys | Move one character/line |
| `Ctrl+A` or `Home` | Beginning of line |
| `Ctrl+E` or `End` | End of line |
| `Shift+Up/Down` | Move 10 lines at once |
| `Page Up/Down` | Scroll by one screen |

---

## Editing text

| Key | Action |
|-----|--------|
| Any printable key | Insert character at cursor |
| `Backspace` / `Ctrl+H` | Delete character to the left |
| `Ctrl+K` | Delete from cursor to end of line |
| `Tab` | Insert 4 spaces |
| `Enter` | Insert new line |

`Ctrl+K` is handy for clearing the rest of a heading or list item when
you want to retype it from scratch.

---

## Undo and redo

mde groups every keystroke into undo units, so a single `Ctrl+Z` undoes
one logical editing action.

| Key | Action |
|-----|--------|
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |

Both work in **both modes** — you can undo from preview without switching
to edit first.

---

## Saving

| Key | Action |
|-----|--------|
| `Ctrl+S` | Save the file |

The `[+]` indicator in the status bar disappears once the file is saved.
If you try to quit (`Ctrl+Q`) with unsaved changes, mde asks you to
confirm by pressing `Ctrl+Q` a second time.

---

## Opening a different file

Press `Ctrl+O` from any mode. A prompt appears at the bottom:

```
Open file: _
```

Type the path and press `Enter`. The current document is replaced (you
will be prompted if it has unsaved changes).

---

## A practical exercise

Let's put editing into practice.

1. Press `Ctrl+P` to enter edit mode.
2. Scroll to the task list below.
3. Change `[ ]` to `[x]` on the first unchecked item.
4. Press `Escape` to return to preview — notice the checkbox is now filled.
5. Press `Ctrl+Z` to undo the change, then `Ctrl+Y` to redo it.
6. Press `Ctrl+S` to save.

### Exercise checklist

- [x] Opened this file in mde
- [x] Read the editing shortcuts
- [x] Toggled a checkbox in edit mode
- [ ] Used undo and redo
- [ ] Saved the file

---

*Lesson 3 of 5 · [← Navigation](02_navigation.md) · [Tables & Code →](04_tables_code.md)*
