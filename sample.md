# mde sample file

**mde** is a terminal Markdown editor with two modes: **preview mode** (rendered
preview) and **edit mode** (raw Markdown). Press `Ctrl+P` to switch between them.

> **New to mde?** Work through the interactive tutorial in the
> [examples folder](examples/00_start_here.md) — five short lessons that
> cover formatting, navigation, editing, tables, and workflow.

---

## Inline formatting

Regular text with **bold**, *italic*, ***bold and italic***, ~~strikethrough~~,
and `inline code`. Links are [underlined](https://github.com) with a dimmed URL,
and you can jump to them with `Ctrl+L` — this is link (1).

---

## Headings

All six levels are coloured and bold in preview mode:

# H1 — top-level title
## H2 — major section
### H3 — subsection
#### H4 — minor section
##### H5 — fine detail
###### H6 — smallest heading

---

## Links and anchor navigation

External link: [mde on GitHub](https://github.com/michelefenu/mde)

Internal anchor: [Jump to Code section](#code)

In preview mode, link numbers appear after link text as `(N)`. Press `Ctrl+L`
and enter `1` to open the external link in your browser, or `2` to scroll to
the Code section heading.

---

## Lists

Unordered (three bullet styles are all equivalent):

- First item
- Second item
* Asterisk item
+ Plus item

Ordered (continuing a numbered list):

1. First step
2. Second step
3. Third step

---

## Todo items

Task lists use GFM-style checkboxes — they render with a distinct checkbox style
in preview mode:

- [x] Open task
- [x] Completed task
- [ ] Task with metadata @alice #feature ~2d 2026-03-10
- [x] Released ~1w #done

---

## Blockquotes

> Blockquotes render in a distinct cyan colour.
> Multiple lines stay visually grouped.
>
> You can include **inline formatting** inside a blockquote too.

---

## Code

Inline: build with `make`, run with `./mde sample.md`.

Fenced code block (language label is dimmed in edit mode):

```c
#include <stdio.h>

int main(void) {
    printf("Hello from mde!\n");
    return 0;
}
```

```bash
# Build and open this file
make
./mde sample.md
```

---

## Tables

Tables render with plain pipes in edit mode and box-drawing borders in preview mode.

| Feature               | Edit mode | Preview mode |
|-----------------------|-----------|--------------|
| Syntax styling        | yes       | yes          |
| Hidden delimiters     | no        | yes          |
| Box-drawing borders   | no        | yes          |
| Coloured links        | yes       | yes          |
| Link numbers `(N)`    | no        | yes          |
| Word wrap             | yes       | yes          |

---

## Images

Images are treated like links but prefixed with `!` — the `!` and delimiters are
dimmed in edit mode, and no link number is appended in preview mode.

![mde logo](https://example.com/mde-logo.png)

---

## Horizontal rules

Three styles, all rendered identically as a dimmed rule:

---

***

___

---

## Key bindings reference

| Key             | Preview mode              | Edit mode                      |
|-----------------|---------------------------|--------------------------------|
| `Ctrl+P`        | Switch to edit mode       | Switch to preview mode         |
| `Ctrl+S`        | Save file                 | Save file                      |
| `Ctrl+Q`        | Quit                      | Quit                           |
| `Ctrl+W`        | Toggle word wrap          | Toggle word wrap               |
| `Ctrl+F`        | Search                    | Search                         |
| `Ctrl+N`        | Next match                | Next match                     |
| `Ctrl+G`        | Go to line number         | Go to line number              |
| `Ctrl+Z`        | Undo                      | Undo                           |
| `Ctrl+Y`        | Redo                      | Redo                           |
| `Ctrl+O`        | Open file                 | Open file                      |
| `Ctrl+L`        | Open link by number       | Open link by number            |
| `Ctrl+T`        | Open Table of Contents    | Open Table of Contents         |
| `Ctrl+K`        | —                         | Delete to end of line          |
| `Ctrl+A` / Home | —                         | Beginning of line              |
| `Ctrl+E` / End  | —                         | End of line                    |
| `Escape`        | —                         | Switch to preview mode         |
| `F1`            | Show help                 | Show help                      |

---

## Learn more

| Resource | Description |
|----------|-------------|
| [examples/00_start_here.md](examples/00_start_here.md) | Interactive 5-lesson tutorial |
| [examples/01_formatting.md](examples/01_formatting.md) | Formatting reference |
| [examples/02_navigation.md](examples/02_navigation.md) | Navigation and links |
| [examples/03_editing.md](examples/03_editing.md) | Edit mode shortcuts |
| [examples/04_tables_code.md](examples/04_tables_code.md) | Tables and fenced code |
| [examples/05_workflow.md](examples/05_workflow.md) | Full workflow reference card |
