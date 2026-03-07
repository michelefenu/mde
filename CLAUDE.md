# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
make        # build the `mde` binary in the project root
make clean  # remove build/ and the mde binary
```

Requires a C99 compiler and ncurses (`ncursesw` preferred). Object files go into `build/`. Dependency files (`.d`) are auto-generated there too.

A special step generates `build/help_md.h` from `docs/help.md` via `xxd -i`, which is then `#include`d by `editor.c`. Editing `docs/help.md` requires a rebuild of `build/editor.o`.

## Architecture

The project is a terminal-based Markdown editor written in C99 using ncurses.

```
src/
  main.c      — entry point: initialises locale, constructs Editor, optionally opens a file, runs the event loop
  editor.h/c  — Editor struct, event loop (editor_run), key dispatch, file I/O, search, help/preview mode switching
  buffer.h/c  — Line-based text buffer: dynamic array of individually heap-allocated Lines; all mutation goes through buffer_* functions
  render.h/c  — Markdown parser and ncurses styled output (edit mode); also generates/draws the PreviewBuffer (preview mode)
  undo.h/c    — UndoStack: append-only entry array, sequence numbers group per-keystroke operations for undo/redo
```

### Key data-flow

- `Editor` owns a `Buffer` (raw text), two `PreviewBuffer`s (preview and help), and two `UndoStack`s (undo/redo).
- On each keypress, `editor.c` mutates the `Buffer` and pushes `UndoEntry` records grouped by `undo_seq`.
- The screen is redrawn each frame: `render_draw_line` / `render_draw_line_wrapped` for edit mode; `preview_draw_line` / `preview_draw_line_wrapped` for preview/help mode.
- `render_get_block_type` classifies each line (heading, list, code fence, blockquote, etc.) for block-level styling; inline spans (bold, italic, code, links) are handled character-by-character inside `render_draw_line`.
- `PreviewBuffer` is rebuilt via `preview_generate` whenever preview mode is entered; it stores pre-computed `CharStyle` per character (including ncurses ACS box-drawing markers stored as `PM_*` sentinel bytes).

### Color pairs

Defined as an enum in `render.h` (`CP_HEADING1`…`CP_MSGBAR`). Initialised once by `render_init_colors()` called from editor startup.
