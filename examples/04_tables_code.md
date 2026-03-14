# Lesson 4 — Tables & Code

Two of the most visually striking features of mde's preview mode are
**box-drawing table borders** and **syntax-highlighted code blocks**.
This lesson shows both in detail.

---

## Tables

### Basic syntax

In edit mode, tables look like plain-text grids:

```
| Column A | Column B | Column C |
|----------|----------|----------|
| value 1  | value 2  | value 3  |
```

In preview mode, the pipe characters and dashes are replaced with proper
box-drawing characters, giving you a clean bordered grid.

### Alignment

Add colons to the separator row to control column alignment:

```
| Left     | Centre   | Right    |
|:---------|:--------:|--------:|
| aligned  | centred  | aligned |
```

| Left     | Centre   | Right    |
|:---------|:--------:|--------:|
| left     | centre   | right   |
| text     | text     | text    |

### A richer example

| Language | Compiled | Garbage collected | Typical use |
|----------|:--------:|:-----------------:|-------------|
| C | yes | no | Systems, embedded |
| Go | yes | yes | Services, CLIs |
| Python | no | yes | Scripting, data |
| Rust | yes | no | Systems, safety |
| JavaScript | no | yes | Web, servers |

Toggle to edit mode (`Ctrl+P`) to see the raw pipe syntax, then back to
preview (`Escape`) to see the box-drawing version.

---

## Inline code

Wrap any word or phrase in backticks to render it as inline code:

- Run `make` to build the project.
- The main entry point is `src/main.c`.
- Use `Ctrl+S` to save and `Ctrl+Q` to quit.

In preview mode inline code appears in a distinct colour (yellow by
default). In edit mode the backticks are dimmed.

---

## Fenced code blocks

Use triple backticks to open and close a code block. Add a language name
after the opening fence for the label:

### C

```c
#include <stdio.h>
#include <stdlib.h>

/* Linked list node */
typedef struct Node {
    int value;
    struct Node *next;
} Node;

Node *push(Node *head, int val) {
    Node *n = malloc(sizeof(Node));
    n->value = val;
    n->next  = head;
    return n;
}

int main(void) {
    Node *list = NULL;
    for (int i = 1; i <= 5; i++)
        list = push(list, i);

    for (Node *n = list; n; n = n->next)
        printf("%d\n", n->value);
}
```

### Bash

```bash
#!/usr/bin/env bash
# Build mde and open this tutorial
make
./mde examples/00_start_here.md
```

### Markdown (the meta-example)

```markdown
# My document

**Bold**, *italic*, and `inline code`.

- Item one
- Item two

[Link text](https://example.com)
```

---

## Edit mode view of code blocks

Switch to edit mode now. Notice:

- The opening `` ``` `` fence and language label are dimmed.
- The code body renders in green.
- The closing `` ``` `` is also dimmed.

This keeps the language annotation visible but unobtrusive while you
write.

---

## Tips for writing tables

1. **Don't stress about alignment.** mde renders tables correctly even
   when the pipe characters are ragged in the source. A table like this:

   ```
   | Name | Age |
   |--|--|
   | Alice | 30 |
   | Bob | 25 |
   ```

   renders just as cleanly as a perfectly-aligned one.

2. **Use `Tab` for spacing.** In edit mode, `Tab` inserts four spaces,
   which helps align table columns by hand when you want to.

3. **`Ctrl+K` clears a cell fast.** Position your cursor inside a cell
   and press `Ctrl+K` to delete from the cursor to the next `|`.

---

*Lesson 4 of 5 · [← Editing](03_editing.md) · [Workflow →](05_workflow.md)*
