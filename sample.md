# Welcome to mde

**mde** is a *terminal-based* markdown editor written in C.

## Features

- **WYSIWYG rendering** of markdown in the terminal
- *Inline formatting*: bold, italic, ~~strikethrough~~, `inline code`
- Heading levels with distinct colors
- Lists (ordered and unordered)
- [Links](https://example.com) rendered with underline
- Fenced code blocks
- Search with incremental highlighting

## Quick Start

```bash
make
./mde sample.md
```

## Key Bindings

| Key       | Action               |
|-----------|----------------------|
| Ctrl+S    | Save file            |
| Ctrl+Q    | Quit (with confirm)  |
| Ctrl+P    | Toggle preview mode  |
| Ctrl+F    | Search               |
| Ctrl+N    | Find next match      |
| Ctrl+G    | Go to line number    |
| Ctrl+K    | Delete to end of line|
| Ctrl+A    | Beginning of line    |
| Ctrl+E    | End of line          |
| Ctrl+L    | Refresh screen       |

> This is a blockquote. It should appear in a distinct color
> with the `>` marker highlighted.

### Nested Formatting

You can combine **bold and *italic*** text. Here is some `inline code`
within a paragraph, and a [clickable link](https://github.com).

1. First ordered item
2. Second ordered item
3. Third ordered item

---

#### Code Block Example

```c
#include <stdio.h>

int main(void) {
    printf("Hello, world!\n");
    return 0;
}
```

That's it! Enjoy writing markdown in your terminal.
