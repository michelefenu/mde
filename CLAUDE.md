# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
make        # build the `mde` binary in the project root
make clean  # remove build/ and the mde binary
make test   # build and run unit tests
```

Requires a C99 compiler. No external libraries — terminal I/O uses POSIX `termios`/`ioctl` and ANSI escape sequences via `src/term/`. Object files go into `build/`. Dependency files (`.d`) are auto-generated there too.

A special step generates `build/help_md.h` from `docs/help.md` via `xxd -i`, which is then `#include`d by `help.c`. Editing `docs/help.md` requires a rebuild.

## Architecture

The project is a terminal-based Markdown editor written in C99 using POSIX terminal I/O (no ncurses dependency).

```
src/
  Core
    main.c          — entry point: locale init, editor setup, event loop
    editor.h/c      — Editor state, event loop, key dispatch, file I/O, prompt dialogs
    buffer.h/c      — Line-based text buffer: dynamic array of heap-allocated Lines

  Rendering
    render.h/c      — Block/inline markdown rendering, preview buffer construction
    render_table.h/c — Pipe-table detection, column-width computation, box-drawing
    preview_ui.h/c  — Preview mode toggle, scroll, and key dispatch

  Features
    search.h/c      — Forward/backward text search with incremental highlighting
    help.h/c        — Help overlay: load embedded help.md into preview buffer
    toc.h/c         — Table-of-contents overlay extracted from headings
    command.h/c     — File-open and link-open commands with tab completion
    links.h/c       — Markdown link extraction, heading anchor lookup, slug generation

  Terminal
    term.h/c        — POSIX terminal I/O: raw termios, ANSI SGR, key parser (replaces ncurses)

  Utilities
    undo.h/c        — Append-only undo/redo stack with sequence-grouped entries
    utf8.h/c        — UTF-8 encoding/decoding and display-width helpers
    xalloc.h        — Exit-on-failure wrappers for malloc/realloc
```

### Key data-flow

- `Editor` owns a `Buffer` (raw text), two `PreviewBuffer`s (preview and help), and two `UndoStack`s (undo/redo).
- On each keypress, `editor.c` mutates the `Buffer` and pushes `UndoEntry` records grouped by `undo_seq`.
- The screen is redrawn each frame: `render_draw_line` / `render_draw_line_wrapped` for edit mode; `preview_draw_line` / `preview_draw_line_wrapped` for preview/help mode.
- `render_get_block_type` classifies each line (heading, list, code fence, blockquote, etc.) for block-level styling; inline spans (bold, italic, code, links) are handled character-by-character inside `render_draw_line`.
- `PreviewBuffer` is rebuilt via `preview_generate` whenever preview mode is entered; it stores pre-computed `CharStyle` per character (including box-drawing markers stored as `PM_*` sentinel bytes, rendered as UTF-8 at draw time).

### Color pairs

Defined as an enum in `render.h` (`CP_HEADING1`…`CP_MSGBAR`). Initialised once by `render_init_colors()` called from editor startup.

## Testing

```bash
make test   # build and run tests/test_main.c
```

Tests live in `tests/test_main.c` and use `assert()`. Coverage includes utf8, buffer, undo, render block detection, and links. Register new test functions in `main()` of `test_main.c`.

## Code style guidelines

- Clean, readable C99; straight to the point.
- Comment non-obvious logic; every module gets a top-of-file purpose comment.
- No over-engineering: solve the current problem only.
- Write unit tests for new pure-logic functions (anything not needing terminal I/O).
- Tests use `assert()`; register in `main()` of `tests/test_main.c`.
