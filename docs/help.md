# mde

**mde** is a small, terminal-based markdown editor with a vim-like modal interface. It has two modes: **normal mode** (read-only formatted preview) and **insert mode** (raw markdown editing). The editor starts in normal mode. Press `i` to enter insert mode and `Esc` to return to normal mode. Files are always saved as raw markdown (`.md`).

A status bar shows the filename, cursor position, line count, and the current mode indicator (`[NORMAL]` or `[+]` for unsaved changes). Markdown delimiters (`#`, `*`, `` ` ``, etc.) are shown in a muted colour in insert mode so the content stands out.

## NORMAL MODE

Normal mode shows a read-only rendered view of the document with syntax markers hidden. Use vim-style keys to navigate and enter commands.

| Key              | Action                          |
|------------------|---------------------------------|
| i / a            | Enter insert mode               |
| o                | Open new line below, enter insert mode |
| /                | Search (enters insert mode at match) |
| n                | Find next match (enters insert mode) |
| N                | Find previous match (enters insert mode) |
| :                | Enter command mode              |
| u                | Undo                            |
| Ctrl+R           | Redo                            |
| j / Down         | Scroll down one line            |
| k / Up           | Scroll up one line              |
| Space / Page Dn  | Scroll down one page            |
| Page Up          | Scroll up one page              |
| g / Home         | Jump to top                     |
| G / End          | Jump to bottom                  |
| Ctrl+T           | Open Table of Contents          |
| Ctrl+W           | Toggle word wrap                |
| Ctrl+Q           | Quit (press twice if unsaved)   |
| F1               | Show this help                  |

## INSERT MODE

Insert mode lets you edit the raw markdown text. Press `Esc` to return to normal mode.

| Key            | Action                          |
|----------------|---------------------------------|
| Esc            | Return to normal mode           |
| Ctrl+S         | Save file                       |
| Ctrl+Q         | Quit (press twice if unsaved)   |
| Ctrl+Z         | Undo                            |
| Ctrl+Y         | Redo                            |
| Ctrl+F         | Search                          |
| Ctrl+N         | Find next match                 |
| Ctrl+G         | Go to line number               |
| Ctrl+P         | Return to normal mode           |
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
| F1             | Show this help                  |

## COMMAND MODE

Press `:` in normal mode to enter command mode. Type a command and press Enter.

| Command        | Action                          |
|----------------|---------------------------------|
| :w             | Save file                       |
| :q             | Quit (fails if unsaved)         |
| :q!            | Force quit without saving       |
| :wq            | Save and quit                   |
| :x             | Save and quit (same as :wq)     |
| :e *filename*  | Open a different file           |
| :set wrap      | Enable word wrap                |
| :set nowrap    | Disable word wrap               |
| :help          | Show this help                  |
| :*number*      | Jump to line number             |

## TOC MODE

TOC mode shows a navigable Table of Contents listing all headings in the document. Open it with Ctrl+T from normal or insert mode. Pressing Enter jumps to the selected heading (in normal mode the preview scrolls to it; in insert mode the heading moves to the top of the screen).

| Key      | Action                              |
|----------|-------------------------------------|
| j / Down | Move selection down                 |
| k / Up   | Move selection up                   |
| Enter    | Jump cursor to heading, close TOC   |
| q / Esc  | Close TOC without jumping           |
| Ctrl+Q   | Quit                                |

## SUPPORTED MARKDOWN

| Element          | Syntax                  | Rendering                        |
|------------------|-------------------------|----------------------------------|
| Headings         | `# H1` … `###### H6`    | Bold + colour per level          |
| Bold             | `**text**`              | Terminal bold                     |
| Italic           | `*text*`                 | Terminal italic                   |
| Bold + Italic    | `***text***`            | Bold + italic                     |
| Strikethrough    | `~~text~~`              | Dimmed text                       |
| Inline code      | `` `code` ``            | Yellow / code colour             |
| Code blocks      | Fenced with `` ``` ``   | Green / code-block colour        |
| Links            | `[text](url)`           | Underlined text, dimmed URL      |
| Images           | `![alt](url)`           | Same as links                     |
| Unordered lists  | `- item` / `* item`     | Coloured bullet                   |
| Ordered lists    | `1. item`               | Coloured number                   |
| Blockquotes      | `> text`                 | Cyan with bold `>`                |
| Horizontal rules | `---` / `***` / `___`   | Dimmed rule line                 |

## SEE ALSO

README.md — build instructions and usage examples
