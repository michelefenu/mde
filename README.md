# tmde — Terminal Markdown Editor

A minimal, terminal-based markdown editor written in C that provides WYSIWYG-like
rendering of markdown formatting directly in the terminal using ncurses.

![C](https://img.shields.io/badge/language-C99-blue)

## Features

- **Live markdown rendering** — headings, bold, italic, strikethrough, inline code,
  links, lists, blockquotes, code blocks, and horizontal rules are styled in real time
- **Syntax markers dimmed** — markdown delimiters (`#`, `*`, `` ` ``, etc.) are shown
  in a muted colour so the content stands out
- **Incremental search** — Ctrl+F with live highlighting, Ctrl+N for next match
- **Status bar** — filename, cursor position, line count, and unsaved-change indicator
- **Modeless editing** — always in edit mode, commands via Ctrl+key combinations
- **Saves raw markdown** — files are saved as standard `.md`, never rendered output

## Requirements

- A C99 compiler (gcc, clang, etc.)
- ncurses development library
  - **macOS**: included with Xcode Command Line Tools
  - **Debian / Ubuntu**: `sudo apt install libncurses-dev`
  - **Fedora**: `sudo dnf install ncurses-devel`
  - **Arch**: `sudo pacman -S ncurses`

## Building

```bash
make
```

This produces the `tmde` binary in the project root.

## Usage

```bash
# Open an existing file
./tmde README.md

# Create a new file
./tmde

# Try the included sample
./tmde sample.md
```

## Key Bindings

| Key           | Action                          |
|---------------|---------------------------------|
| Ctrl+S        | Save (prompts for name if new)  |
| Ctrl+Q        | Quit (confirm if unsaved)       |
| Ctrl+F        | Incremental search              |
| Ctrl+N        | Find next search match          |
| Ctrl+G        | Go to line number               |
| Ctrl+K        | Delete from cursor to end of line |
| Ctrl+A / Home | Move to beginning of line       |
| Ctrl+E / End  | Move to end of line             |
| Ctrl+L        | Force screen refresh            |
| Tab           | Insert 4 spaces                 |
| Page Up/Down  | Scroll by page                  |
| Arrow keys    | Navigate                        |
| Escape        | Clear search highlighting       |

## Supported Markdown

| Element          | Syntax                  | Rendering                        |
|------------------|-------------------------|----------------------------------|
| Headings         | `# H1` through `###### H6` | Bold + colour per level      |
| Bold             | `**text**`              | Terminal bold                    |
| Italic           | `*text*`                | Terminal italic                  |
| Bold + Italic    | `***text***`            | Bold + italic                    |
| Strikethrough    | `~~text~~`              | Dimmed text                      |
| Inline code      | `` `code` ``            | Yellow / code colour             |
| Code blocks      | Fenced with `` ``` ``   | Green / code-block colour        |
| Links            | `[text](url)`           | Underlined text, dimmed URL      |
| Images           | `![alt](url)`           | Same as links                    |
| Unordered lists  | `- item` / `* item`     | Coloured bullet                  |
| Ordered lists    | `1. item`               | Coloured number                  |
| Blockquotes      | `> text`                | Cyan with bold `>`               |
| Horizontal rules | `---` / `***` / `___`   | Dimmed rule line                 |

## Architecture

```
src/
  main.c      — entry point & argument handling
  editor.h/c  — editor state, event loop, key handling, file I/O, search
  buffer.h/c  — line-based text buffer (dynamic array of lines)
  render.h/c  — markdown parser & ncurses styled output
```

The text buffer stores the document as a dynamic array of individually allocated
lines. The render engine does a multi-pass analysis of each visible line:
inline code spans first, then emphasis (bold/italic), strikethrough, and links.
Block-level elements (headings, lists, quotes, code blocks) are detected and
styled separately.

## License

MIT
