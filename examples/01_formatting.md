# Lesson 1 — Formatting

> **Tip:** Press `Ctrl+P` to toggle between preview and edit mode as you
> read. Seeing the raw Markdown alongside the rendered result is the
> fastest way to learn the syntax.

---

## Headings

Headings use `#` prefix characters. Six levels are supported; each gets
its own colour in preview mode.

# H1 — Document title
## H2 — Major section
### H3 — Subsection
#### H4 — Minor section
##### H5 — Fine detail
###### H6 — Smallest heading

---

## Inline text styles

| Style | Syntax | Result |
|-------|--------|--------|
| Bold | `**word**` | **bold** |
| Italic | `*word*` | *italic* |
| Bold + italic | `***word***` | ***bold and italic*** |
| Strikethrough | `~~word~~` | ~~crossed out~~ |
| Inline code | `` `code` `` | `code` |

Mix them freely: **you can have *nested* styles** and even
***all three at once***.

Strikethrough is great for ~~tasks you already finished~~ or
~~decisions you reversed~~.

---

## Links

Basic link syntax: `[display text](destination)`

- External URL: [mde on GitHub](https://github.com/michelefenu/mde)
- Anchor in this file: [jump to Lists](#lists)
- Link to another tutorial file: [open Lesson 2](02_navigation.md)

In preview mode each link gets a number `(N)` appended. Press `Ctrl+L`,
type the number, and press `Enter` to follow it.

---

## Lists

### Unordered lists

All three bullet markers work and render identically:

- Dashes are the most common
- Asterisks also work
* Like this one
+ And plus signs too

### Ordered lists

1. First item
2. Second item
3. Third item

### Task lists (GFM checkboxes)

- [x] Write the tutorial introduction
- [x] Add formatting examples
- [ ] Review with a friend
- [ ] Ship it

---

## Blockquotes

Use `>` to mark a blockquote. They render in cyan with a bold `>` marker.

> "The best editor is the one that gets out of your way."

Blockquotes can span multiple lines and contain **inline formatting**:

> This is a multi-line blockquote.
> It keeps going on the next line.
>
> After a blank `>` line you get a new paragraph inside the quote.
> You can even nest *italic* and `code` inside.

---

## Images

Images share the link syntax but with a `!` prefix:

```
![alt text](path/or/url)
```

The `!` and delimiters are dimmed in edit mode. In preview mode no link
number is shown (images are not followable).

---

## Horizontal rules

Three dashes, asterisks, or underscores all draw the same dimmed rule:

---

***

___

---

*Lesson 1 of 5 · [← Start here](00_start_here.md) · [Navigation →](02_navigation.md)*
