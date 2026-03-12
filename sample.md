# mde sample file

**mde** is a terminal Markdown editor with two modes: **normal mode** (rendered
preview) and **edit mode** (raw Markdown). Press `Ctrl+P` to switch between them.

---

## Inline formatting

Regular text with **bold**, *italic*, ***bold and italic***, ~~strikethrough~~,
and `inline code`. Links are [underlined](https://github.com) with a dimmed URL,
and you can jump to them with `:open N` — this is link (1).

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

In normal mode, link numbers appear after link text as `(N)`. Type `:open 1` to
open the external link in your browser, or `:open 2` to scroll to the Code
section heading.

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

- [ ] Open task
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

| Feature               | Edit mode | Normal mode |
|-----------------------|-----------|-------------|
| Syntax styling        | yes       | yes         |
| Hidden delimiters     | no        | yes         |
| Box-drawing borders   | no        | yes         |
| Coloured links        | yes       | yes         |
| Link numbers `(N)`    | no        | yes         |
| Word wrap             | yes       | yes         |

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

## Command reference

Press `:` in normal mode to enter a command:

| Command       | Action                              |
|---------------|-------------------------------------|
| `:w`          | Save file                           |
| `:q`          | Quit (fails if unsaved)             |
| `:q!`         | Force quit without saving           |
| `:wq` / `:x`  | Save and quit                       |
| `:e filename` | Open a different file               |
| `:open N`     | Open link N (browser or anchor)     |
| `:set wrap`   | Enable word wrap                    |
| `:set nowrap` | Disable word wrap                   |
| `:help`       | Show built-in help                  |
| `:N`          | Jump to line number N               |

## Key bindings reference

| Key             | Normal mode               | Edit mode                      |
|-----------------|---------------------------|--------------------------------|
| `Ctrl+P`        | Switch to edit mode       | Switch to normal mode          |
| `Ctrl+S`        | —                         | Save file                      |
| `Ctrl+Q`        | Quit                      | Quit                           |
| `Ctrl+W`        | Toggle word wrap          | Toggle word wrap               |
| `/`             | Search                    | —                              |
| `Ctrl+F`        | —                         | Search                         |
| `n` / `N`       | Next / previous match     | —                              |
| `Ctrl+N`        | —                         | Next match                     |
| `j` / `k`       | Scroll down / up          | —                              |
| `g` / `G`       | Jump to top / bottom      | —                              |
| `Space`         | Scroll down one page      | —                              |
| `Ctrl+T`        | Open Table of Contents    | Open Table of Contents         |
| `Ctrl+Z`        | Undo                      | Undo                           |
| `Ctrl+Y`        | Redo                      | Redo                           |
| `Ctrl+G`        | —                         | Go to line number              |
| `Ctrl+K`        | —                         | Delete to end of line          |
| `Ctrl+A` / Home | —                         | Beginning of line              |
| `Ctrl+E` / End  | —                         | End of line                    |
| `F1`            | Show help                 | Show help                      |
