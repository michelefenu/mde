![mde logo](images/logo-64x64.png)

# mde

![C](https://img.shields.io/badge/language-C99-blue)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macOS-lightgrey)
![License](https://img.shields.io/badge/license-BSD--2--Clause-green)

## What is mde?

mde is a terminal-based markdown editor. It lets you edit and preview
markdown files without leaving the terminal.

In edit mode, syntax is styled as you type: headings, emphasis, links,
lists, code blocks, tables, and more are coloured in place, with
delimiters dimmed so the content stays readable. Preview mode renders
the document read-only with syntax markers hidden and tables drawn with
box-drawing characters.

Lists are continued automatically on Enter with correct markers and
indentation. Ordered lists are auto-incremented. Todo items (`- [ ]`,
`- [x]`) support metadata tokens like `#tag`, `@assignee`, `~duration`,
and dates.

## Requirements

A C99 compiler (gcc or clang) and the ncurses development library.

- **macOS**: included with Xcode Command Line Tools
- **Debian / Ubuntu**: `sudo apt install libncurses-dev`
- **Fedora**: `sudo dnf install ncurses-devel`
- **Arch**: `sudo pacman -S ncurses`

## Compiling

```
make
```

This produces the `mde` binary in the project root. Object files go
into `build/`. To remove all build artifacts:

```
make clean
```

To build and run the test suite:

```
make test
```

## Usage

```
mde [file]
```

If no file is given, mde starts with an empty buffer. A sample file is
included:

```
mde sample.md
```

## Key bindings

mde operates in two modes: edit and preview. Press Ctrl+P or Escape to
switch between them.

### Edit mode

| Key               | Action                          |
|-------------------|---------------------------------|
| Ctrl+S            | Save                            |
| Ctrl+Q            | Quit (press twice if unsaved)   |
| Ctrl+P / Escape   | Switch to preview mode          |
| Ctrl+Z            | Undo                            |
| Ctrl+Y            | Redo                            |
| Ctrl+F            | Search                          |
| Ctrl+N            | Next match                      |
| Ctrl+G            | Go to line                      |
| Ctrl+O            | Open file (tab completion)      |
| Ctrl+L            | Open link by number             |
| Ctrl+T            | Table of contents               |
| Ctrl+W            | Toggle word wrap                |
| Ctrl+K            | Delete to end of line           |
| Ctrl+A / Ctrl+E   | Beginning / end of line         |
| Tab               | Insert 4 spaces                 |
| F1                | Help                            |

### Preview mode

| Key               | Action                          |
|-------------------|---------------------------------|
| Ctrl+P            | Switch to edit mode             |
| Ctrl+F            | Search                          |
| Ctrl+N            | Next match                      |
| Ctrl+S            | Save                            |
| Ctrl+G            | Go to line                      |
| Ctrl+O            | Open file                       |
| Ctrl+L            | Open link by number             |
| Ctrl+T            | Table of contents               |
| Ctrl+W            | Toggle word wrap                |
| Ctrl+Z / Ctrl+Y   | Undo / redo                     |
| Ctrl+Q            | Quit                            |
| Arrow keys        | Scroll                          |
| Shift+Up/Down     | Scroll 10 lines                 |
| Page Up/Down      | Scroll one page                 |
| Home / End        | Top / bottom of document        |
| F1                | Help                            |

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

Press F1 inside mde for a built-in help reference, or read `docs/help.md`
directly.

## Copying

mde is distributed under the BSD 2-Clause license. See the LICENSE file
for the full text.

## Reporting bugs

Open an issue on the project's issue tracker. Patches are welcome.
