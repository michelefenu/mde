# Lesson 2 — Navigation

mde has several ways to move through documents and between files. This
lesson covers all of them.

---

## Scrolling and cursor movement

| Key | Preview mode | Edit mode |
|-----|-------------|-----------|
| `Arrow Up/Down` | Scroll one line | Move cursor |
| `Page Up/Down` | Scroll one page | Scroll one page |
| `Shift+Up/Down` | Scroll 10 lines | Move 10 lines |
| `Home` | Jump to top | Beginning of line |
| `End` | Jump to bottom | End of line |

---

## The Table of Contents

Press `Ctrl+T` from either mode to open an interactive TOC panel. It
lists every heading in the current document.

**Inside the TOC panel:**

| Key | Action |
|-----|--------|
| `Up/Down` | Move the selection |
| `Enter` | Jump to that heading and close the panel |
| `Escape` | Close without jumping |

Try it now — press `Ctrl+T` and navigate to [Anchor links](#anchor-links).

---

## Anchor links

An anchor link points to a heading inside the *same* document. The
destination is the heading text converted to lowercase, with spaces
replaced by hyphens.

```markdown
[Jump to Search](#search)
[Back to top](#lesson-2--navigation)
```

- [Jump to Search](#search)
- [Jump to Link navigation](#link-navigation)
- [Back to the top of this file](#lesson-2--navigation)

In preview mode, anchor links are numbered like any other link. Follow
them with `Ctrl+L` and the number — mde scrolls to the target heading
instead of opening a browser.

---

## Link navigation

Every link in preview mode gets a number shown as `(N)` after the link
text. To follow a link:

1. Press `Ctrl+L`
2. Type the number shown next to the link
3. Press `Enter`

**External links** (http/https) open in your system browser.

**Anchor links** (`#heading`) scroll the preview to that heading.

**File links** (relative paths like `03_editing.md`) open that file in
mde, replacing the current document.

---

## Search

Press `Ctrl+F` to open the search bar at the bottom of the screen.

- Type your search term and press `Enter` to find the first match.
- Press `Ctrl+N` to jump to the next match.
- Matches are highlighted across the document.
- Press `Escape` to dismiss the search bar.

Search works in both preview mode and edit mode.

---

## Go to line

Press `Ctrl+G`, type a line number, and press `Enter` to jump directly
to that line. Useful for navigating large documents after a search in
another tool tells you a line number.

---

## Opening files

Press `Ctrl+O` to open a file. A prompt appears at the bottom — type the
path (absolute or relative to your working directory) and press `Enter`.

**Try it:** Open [Lesson 3 — Editing](03_editing.md) with `Ctrl+O` and
type `examples/03_editing.md`, or follow the link at the bottom of this
page with `Ctrl+L`.

---

## Word wrap

Press `Ctrl+W` to toggle word wrap on or off. When word wrap is on, long
lines fold at the terminal width. Your file is unchanged — wrap is a
display-only setting.

---

*Lesson 2 of 5 · [← Formatting](01_formatting.md) · [Editing →](03_editing.md)*
