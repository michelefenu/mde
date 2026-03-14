# mde

**mde** is a small, terminal-based markdown editor with two modes: **preview mode** (read-only formatted preview) and **edit mode** (raw markdown editing). The editor starts in preview mode. Press `Ctrl+P` to toggle between modes. Files are always saved as raw markdown (`.md`).

A status bar shows the filename, cursor position, line count, and the current mode indicator (`[PREVIEW]` or `[+]` for unsaved changes). Markdown delimiters (`#`, `*`, `` ` ``, etc.) are shown in a muted colour in edit mode so the content stands out.

## PREVIEW MODE

Preview mode shows a read-only rendered view of the document with syntax markers hidden.

| Key              | Action                          |
|------------------|---------------------------------|
| Ctrl+P           | Toggle to edit mode             |
| Ctrl+F           | Search (highlights matches)     |
| Ctrl+N           | Find next match                 |
| Ctrl+S           | Save file                       |
| Ctrl+G           | Go to line number               |
| Ctrl+Z           | Undo                            |
| Ctrl+Y           | Redo                            |
| Ctrl+O           | Open a different file           |
| Ctrl+L           | Open link by number             |
| Ctrl+T           | Open Table of Contents          |
| Ctrl+W           | Toggle word wrap                |
| Ctrl+Q           | Quit (press twice if unsaved)   |
| Arrow Up / Down  | Scroll one line                 |
| Page Up / Down   | Scroll one page                 |
| Shift+Up / Down  | Scroll 10 lines                 |
| Home / End       | Jump to top / bottom            |
| F1               | Show this help                  |

## EDIT MODE

Edit mode lets you edit the raw markdown text. Press `Ctrl+P` or `Escape` to return to preview mode.

| Key            | Action                          |
|----------------|---------------------------------|
| Ctrl+P         | Toggle to preview mode          |
| Ctrl+S         | Save file                       |
| Ctrl+Q         | Quit (press twice if unsaved)   |
| Ctrl+Z         | Undo                            |
| Ctrl+Y         | Redo                            |
| Ctrl+F         | Search                          |
| Ctrl+N         | Find next match                 |
| Ctrl+G         | Go to line number               |
| Ctrl+O         | Open a different file           |
| Ctrl+L         | Open link by number             |
| Ctrl+T         | Open Table of Contents          |
| Ctrl+W         | Toggle word wrap                |
| Ctrl+K         | Delete to end of line           |
| Ctrl+A         | Move to beginning of line       |
| Ctrl+E         | Move to end of line             |
| Ctrl+H         | Delete character (backspace)    |
| Tab            | Insert 4 spaces                 |
| Arrow keys     | Move cursor                     |
| Shift+Up / Down | Move 10 lines                  |
| Home / End     | Beginning / end of line         |
| Page Up / Down | Scroll by screen height         |
| Escape         | Switch to preview mode          |
| F1             | Show this help                  |

## TOC MODE

TOC mode shows a navigable Table of Contents listing all headings in the document. Open it with Ctrl+T from preview or edit mode. Pressing Enter jumps to the selected heading (in preview mode the preview scrolls to it; in edit mode the heading moves to the top of the screen).

| Key        | Action                              |
|------------|-------------------------------------|
| Up / Down  | Move selection                      |
| Enter      | Jump cursor to heading, close TOC   |
| Esc        | Close TOC without jumping           |
| Ctrl+Q     | Quit                                |

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
