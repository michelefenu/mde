![mde logo](images/logo-64x64.png)

# mde

A terminal-based markdown editor written in C. Edit and preview markdown files without leaving the terminal.

![C](https://img.shields.io/badge/language-C99-blue)

## Features

- **Live syntax styling**: headings, bold, italic, strikethrough, inline code, links, lists, blockquotes, code blocks, and horizontal rules are coloured and styled as you type; markdown delimiters are dimmed so the content stays readable
- **Normal mode**: Ctrl+P renders a read-only view with syntax markers hidden, man-page-style heading indentation, and box-drawing table borders; vim-style `/` search with highlighted matches and `n`/`N` navigation without leaving normal mode
- **Todo items**: GFM-style task checkboxes (`- [ ]` open, `- [x]` done) with coloured styling; metadata tokens highlighted — `#tag`, `@assignee`, `~duration`, and `yyyy-mm-dd` dates
- **List autocompletion**: pressing Enter on a list item starts the next item automatically — `- `, `* `, `+ ` for unordered; incremented numbers for ordered (`1.` → `2.`, `1)` → `2)`) with indentation preserved; pressing Enter on an empty list item exits the list
- **Word wrap**: Ctrl+W toggles character-level wrapping at the terminal width, in both edit and normal mode
- **Table of Contents**: Ctrl+T opens a navigable TOC panel listing all headings; press j/k to navigate, Enter to jump to the selected heading
- **Incremental search**: Ctrl+F with live match highlighting, Ctrl+N to jump to the next match; `/` search in normal mode with `n`/`N` navigation
- **Undo/redo**: full undo history with Ctrl+Z / Ctrl+Y; operations like list continuation undo atomically
- **Status bar**: filename, cursor position, line count, dirty indicator

## Requirements

- A C99 compiler (gcc or clang)
- ncurses development library
  - **macOS**: included with Xcode Command Line Tools
  - **Debian / Ubuntu**: `sudo apt install libncurses-dev`
  - **Fedora**: `sudo dnf install ncurses-devel`
  - **Arch**: `sudo pacman -S ncurses`

## Building

```bash
make
```

Produces the `mde` binary in the project root. To clean build artifacts:

```bash
make clean
```

## Usage

```bash
./mde file.md       # open an existing file
./mde               # start with an empty buffer
./mde sample.md     # try the included sample
```

## Key Bindings

### Edit Mode

![Edit mode](images/edit-mode.png)
*Edit mode: markdown syntax with dimmed delimiters*

| Key               | Action                                  |
|-------------------|-----------------------------------------|
| Ctrl+P         | Toggle to normal mode           |
| Ctrl+S         | Save file                       |
| Ctrl+Q         | Quit (press twice if unsaved)   |
| Ctrl+Z         | Undo                            |
| Ctrl+Y         | Redo                            |
| Ctrl+F         | Search                          |
| Ctrl+N         | Find next match                 |
| Ctrl+G         | Go to line number               |
| Ctrl+T         | Open Table of Contents          |
| Ctrl+W         | Toggle word wrap                |
| Ctrl+K         | Delete to end of line           |
| Ctrl+A         | Move to beginning of line       |
| Ctrl+E         | Move to end of line             |
| Ctrl+L         | Refresh screen                  |
| Ctrl+H         | Delete character (backspace)    |
| Tab            | Insert 4 spaces                 |
| Arrow keys     | Move cursor                     |
| Home / End     | Beginning / end of line         |
| Page Up / Down | Scroll by screen height         |
| F1             | Show help                       |

### Normal Mode

![Preview mode](images/preview-mode.png)
*Normal mode: read-only rendered view with vim-like keybindings*

| Key              | Action                                 |
|------------------|----------------------------------------|
| Ctrl+P           | Toggle to edit mode                    |
| /                | Search (highlights matches in preview) |
| n                | Find next match                        |
| N                | Find previous match                    |
| :                | Enter command mode                     |
| u                | Undo                                   |
| Ctrl+R           | Redo                                   |
| j / Down         | Scroll down one line                   |
| k / Up           | Scroll up one line                     |
| Space / Page Dn  | Scroll down one page                   |
| Page Up          | Scroll up one page                     |
| g / Home         | Jump to top                            |
| G / End          | Jump to bottom                         |
| Ctrl+T           | Open Table of Contents                 |
| Ctrl+W           | Toggle word wrap                       |
| Ctrl+Q           | Quit (press twice if unsaved)          |
| F1               | Show help                              |

### Command Mode

Press `:` in normal mode to enter command mode.

| Command       | Action                          |
|---------------|---------------------------------|
| :w            | Save file                       |
| :q            | Quit (fails if unsaved)         |
| :q!           | Force quit without saving       |
| :wq           | Save and quit                   |
| :x            | Save and quit (same as :wq)     |
| :e *filename* | Open a different file           |
| :set wrap     | Enable word wrap                |
| :set nowrap   | Disable word wrap               |
| :help         | Show help                       |
| :open *N*     | Open link N (browser or anchor) |
| :*number*     | Jump to line number             |

## Supported Markdown

| Element          | Syntax                          | Notes                          |
|------------------|---------------------------------|--------------------------------|
| Headings         | `# H1` through `###### H6`     | Bold, coloured per level       |
| Bold             | `**text**`                      |                                |
| Italic           | `*text*`                        |                                |
| Bold + italic    | `***text***`                    |                                |
| Strikethrough    | `~~text~~`                      | Rendered dimmed                |
| Inline code      | `` `code` ``                    |                                |
| Code blocks      | Fenced with `` ``` ``           |                                |
| Links            | `[text](url)`                   | Text underlined, URL dimmed    |
| Images           | `![alt](url)`                   | Same as links                  |
| Unordered lists  | `- item`, `* item`, `+ item`    | Autocompletion on Enter        |
| Ordered lists    | `1. item`, `1) item`            | Auto-increments on Enter       |
| Todo items       | `- [ ] task`, `- [x] done`      | Checkbox coloured; `#tag` `@name` `~dur` `yyyy-mm-dd` highlighted |
| Blockquotes      | `> text`                        |                                |
| Horizontal rules | `---`, `***`, `___`             |                                |
| Tables           | GFM pipe syntax                 | Box-drawing borders in preview |

## Architecture

```
src/
  main.c        — entry point: locale init, constructs Editor, opens file, runs event loop
  editor.h/c    — Editor struct, event loop, key dispatch, file I/O, search, mode switching
  buffer.h/c    — line-based text buffer; dynamic array of heap-allocated lines
  render.h/c    — markdown parser and ncurses output for edit mode; generates PreviewBuffer
  undo.h/c      — append-only undo stack; sequence numbers group per-keystroke operations
  search.h/c    — incremental search with match tracking
  utf8.h/c      — UTF-8 character boundary helpers
  preview_ui.h  — preview and help mode rendering
```

Files are saved as standard `.md`; the editor never writes rendered output.

## License

MIT
