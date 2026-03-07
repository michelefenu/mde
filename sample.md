# mde sample file

This file demonstrates the markdown elements that **mde** renders in the terminal.
Open it, edit it, and press **Ctrl+P** to toggle preview mode.

---

## Inline formatting

Regular text with **bold**, *italic*, ***bold and italic***, ~~strikethrough~~,
and `inline code`. You can also have [links](https://example.com) with underlined
text and a dimmed URL.

---

## Lists

Unordered lists (try pressing Enter at the end of an item):

- First item
- Second item
- Third item

Ordered lists (auto-increments on Enter):

1. First step
2. Second step
3. Third step

Lists support multiple bullet styles:

* asterisk item
+ plus item

---

## Blockquotes

> Blockquotes are rendered in a distinct colour.
> Multiple lines stay grouped together.

---

## Code

Inline: use `make` to build the project.

Fenced code block with syntax highlighting:

```c
#include <stdio.h>

int main(void) {
    printf("Hello from mde!\n");
    return 0;
}
```

```bash
make
./mde sample.md
```

---

## Headings

# H1 heading
## H2 heading
### H3 heading
#### H4 heading
##### H5 heading
###### H6 heading

---

## Tables

Tables render with plain pipes in edit mode and box-drawing borders in preview mode.

| Feature              | Edit mode | Preview mode |
|----------------------|-----------|--------------|
| Syntax styling       | yes       | yes          |
| Hidden delimiters    | no        | yes          |
| List autocompletion  | yes       | n/a          |
| Word wrap            | yes       | yes          |

---

## Key bindings reference

| Key               | Action                                  |
|-------------------|-----------------------------------------|
| Ctrl+S            | Save                                    |
| Ctrl+Q            | Quit                                    |
| Ctrl+P            | Toggle preview mode                     |
| Ctrl+W            | Toggle word wrap                        |
| Ctrl+F            | Incremental search                      |
| Ctrl+N            | Next match                              |
| Ctrl+G            | Go to line                              |
| Ctrl+K            | Delete to end of line                   |
| Ctrl+Z / Ctrl+Y   | Undo / redo                             |
| Ctrl+A / Home     | Beginning of line                       |
| Ctrl+E / End      | End of line                             |
| Enter             | New line (continues list if applicable) |

---

## Horizontal rules

Three styles, all rendered the same:

---

***

___
