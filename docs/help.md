# mde

**mde** is a small, terminal-based markdown editor. It renders markdown formatting directly in the terminal as you type: headings, emphasis, links, lists, code blocks, and blockquotes are styled in real time. You can switch to a read-only preview mode (Ctrl+P) to view the document without syntax markers. Files are always saved as raw markdown (`.md`).

The interface is modeless: you are always in edit mode. Commands are invoked
via Ctrl+key combinations. A status bar shows the filename, cursor position,
line count, and an unsaved-change indicator. Markdown delimiters (`#`, `*`,
`` ` ``, etc.) are shown in a muted colour so the content stands out.

## EDIT MODE

| Key            | Action                          |
|----------------|---------------------------------|
| Ctrl+S         | Save file                       |
| Ctrl+Q         | Quit (press twice if unsaved)   |
| Ctrl+Z         | Undo                            |
| Ctrl+Y         | Redo                            |
| Ctrl+F         | Search                          |
| Ctrl+N         | Find next match                 |
| Ctrl+G         | Go to line number               |
| Ctrl+P         | Toggle preview mode             |
| Ctrl+T         | Open Table of Contents          |
| Ctrl+W         | Toggle word wrap                |
| Ctrl+K         | Delete to end of line           |
| Ctrl+A         | Move to beginning of line       |
| Ctrl+E         | Move to end of line             |
| Ctrl+L         | Refresh screen                  |
| Ctrl+H         | Delete character (backspace)    |
| Tab            | Insert 4 spaces                 |
| Escape         | Clear search highlight          |
| Arrow keys     | Move cursor                     |
| Home / End     | Beginning / end of line         |
| Page Up / Down | Scroll by screen height         |
| F1             | Show this help                  |

## PREVIEW MODE

Preview mode shows a read-only rendered view with syntax markers hidden. Word wrap (Ctrl+W) applies in both edit and preview mode.

| Key              | Action                        |
|------------------|-------------------------------|
| j / Down         | Scroll down one line          |
| k / Up           | Scroll up one line            |
| Space / Page Dn  | Scroll down one page          |
| Page Up          | Scroll up one page            |
| g / Home         | Jump to top                   |
| G / End          | Jump to bottom                |
| o                | Open a link in web browser   |
| Ctrl+T           | Open Table of Contents        |
| Ctrl+W           | Toggle word wrap              |
| q / Esc / Ctrl+P | Return to edit mode           |
| Ctrl+Q           | Quit                          |
| F1               | Show this help                |

## TOC MODE

TOC mode shows a navigable Table of Contents listing all headings in the document. Open it with Ctrl+T from edit or preview mode. Pressing Enter jumps to the selected heading (in preview mode the preview scrolls to it; in edit mode the heading moves to the top of the screen).

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
