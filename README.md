![mde logo](images/logo-64x64.png)

# mde

![C](https://img.shields.io/badge/language-C99-blue)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macOS-lightgrey)
![License](https://img.shields.io/badge/license-BSD--2--Clause-green)

## What is mde?

mde is a terminal-based markdown editor. It operates in two modes:

- **Edit mode** - live syntax styling with dimmed delimiters. Headings,
  emphasis, links, lists, code blocks, tables, blockquotes, and
  horizontal rules are coloured as text is entered.
- **Preview mode** - read-only rendered view. Syntax markers are hidden,
  tables use box-drawing borders, and headings are indented by level.

Switching between modes is done with Ctrl+P or Escape.

Additional features include incremental search with match highlighting,
full undo/redo history, automatic list continuation with marker
auto-increment, GFM task checkboxes with metadata highlighting (`#tag`,
`@assignee`, `~duration`, dates), word wrap, table of contents
navigation, and tab-completed file open.

Files are saved as standard `.md`. The editor never writes rendered
output.

## Requirements

A C99 compiler (gcc or clang) and the ncurses development library.

- **macOS** - included with Xcode Command Line Tools
- **Debian / Ubuntu** - `sudo apt install libncurses-dev`
- **Fedora** - `sudo dnf install ncurses-devel`
- **Arch** - `sudo pacman -S ncurses`

## Compiling

```
make
```

The `mde` binary is produced in the project root. Object files are
placed in `build/`.

```
make clean          # remove build artifacts
make test           # build and run the test suite
```

## Usage

```
mde [file]
```

Without arguments, mde starts with an empty buffer. A sample file is
included for reference:

```
mde sample.md
```

## Key bindings

### Edit mode

| Key               | Action                        |
|-------------------|-------------------------------|
| Ctrl+S            | Save                          |
| Ctrl+Q            | Quit (twice if unsaved)       |
| Ctrl+P / Escape   | Preview mode                  |
| Ctrl+Z            | Undo                          |
| Ctrl+Y            | Redo                          |
| Ctrl+F            | Search                        |
| Ctrl+N            | Next match                    |
| Ctrl+G            | Go to line                    |
| Ctrl+O            | Open file                     |
| Ctrl+L            | Open link by number           |
| Ctrl+T            | Table of contents             |
| Ctrl+W            | Toggle word wrap              |
| Ctrl+K            | Delete to end of line         |
| Ctrl+A / Ctrl+E   | Beginning / end of line       |
| Tab               | Insert 4 spaces               |
| F1                | Help                          |

### Preview mode

| Key               | Action                        |
|-------------------|-------------------------------|
| Ctrl+P            | Edit mode                     |
| Ctrl+F            | Search                        |
| Ctrl+N            | Next match                    |
| Ctrl+S            | Save                          |
| Ctrl+G            | Go to line                    |
| Ctrl+O            | Open file                     |
| Ctrl+L            | Open link by number           |
| Ctrl+T            | Table of contents             |
| Ctrl+W            | Toggle word wrap              |
| Ctrl+Z / Ctrl+Y   | Undo / redo                   |
| Ctrl+Q            | Quit                          |
| Arrow keys        | Scroll                        |
| Shift+Up/Down     | Scroll 10 lines               |
| Page Up/Down      | Scroll one page               |
| Home / End        | Top / bottom                  |
| F1                | Help                          |

## Architecture

```
src/
  main.c          - entry point, locale init, event loop
  editor.h/c      - editor state, key dispatch, file I/O
  buffer.h/c      - line-based text buffer
  render.h/c      - markdown parser and ncurses rendering
  undo.h/c        - append-only undo/redo stack
  search.h/c      - incremental search with highlighting
  utf8.h/c        - UTF-8 helpers
  preview_ui.h    - preview mode rendering
```

## Documentation

The built-in help is accessible with F1 at any time. The source for
it is in `docs/help.md`.

## Copying

mde is distributed under the BSD 2-Clause license. See the LICENSE
file for the full text.

## Reporting bugs

Open an issue on the project's issue tracker. Patches are welcome.
