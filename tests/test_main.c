#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "../src/utf8.h"
#include "../src/buffer.h"
#include "../src/undo.h"
#include "../src/render.h"

/* ================================================================
 *  utf8 tests
 * ================================================================ */

static void test_utf8_clen(void)
{
    /* ASCII */
    assert(utf8_clen('A') == 1);
    assert(utf8_clen(0x00) == 1);
    assert(utf8_clen(0x7F) == 1);

    /* Stray continuation byte returns 1 */
    assert(utf8_clen(0x80) == 1);
    assert(utf8_clen(0xBF) == 1);

    /* 2-byte lead */
    assert(utf8_clen(0xC0) == 2);
    assert(utf8_clen(0xDF) == 2);

    /* 3-byte lead */
    assert(utf8_clen(0xE0) == 3);
    assert(utf8_clen(0xEF) == 3);

    /* 4-byte lead */
    assert(utf8_clen(0xF0) == 4);
    assert(utf8_clen(0xF7) == 4);
}

static void test_utf8_char_bytes(void)
{
    /* ASCII */
    assert(utf8_char_bytes("hello", 5, 0) == 1);

    /* 2-byte: e.g. U+00E9 = C3 A9 */
    assert(utf8_char_bytes("\xC3\xA9", 2, 0) == 2);

    /* 3-byte: e.g. U+4E16 = E4 B8 96 */
    assert(utf8_char_bytes("\xE4\xB8\x96", 3, 0) == 3);

    /* 4-byte: e.g. U+1F600 = F0 9F 98 80 */
    assert(utf8_char_bytes("\xF0\x9F\x98\x80", 4, 0) == 4);

    /* Clamp to remaining length */
    assert(utf8_char_bytes("\xC3\xA9", 1, 0) == 1);

    /* Past end returns 0 */
    assert(utf8_char_bytes("abc", 3, 5) == 0);
}

static void test_utf8_prev_char(void)
{
    /* At position 0, returns 0 */
    assert(utf8_prev_char("abc", 0) == 0);

    /* ASCII: previous of pos=2 in "abc" is 1 */
    assert(utf8_prev_char("abc", 2) == 1);

    /* Multi-byte: U+00E9 (C3 A9) followed by 'x'
       prev_char from pos=3 should land on 2 (the 'x' after the 2-byte char)
       Wait: "C3 A9 78" — pos 2 is 'x', prev from 2 goes to... */
    {
        const char *s = "\xC3\xA9x";  /* bytes: C3 A9 78 */
        assert(utf8_prev_char(s, 3) == 2);  /* prev of end -> 'x' at 2 */
        assert(utf8_prev_char(s, 2) == 0);  /* prev of 'x' -> start of e-acute */
    }
}

static void test_wchar_to_utf8(void)
{
    char out[4];

    /* ASCII */
    assert(wchar_to_utf8('A', out) == 1);
    assert(out[0] == 'A');

    /* 2-byte: U+00E9 (e with acute) */
    assert(wchar_to_utf8(0xE9, out) == 2);
    assert((unsigned char)out[0] == 0xC3);
    assert((unsigned char)out[1] == 0xA9);

    /* 3-byte: U+4E16 (Chinese character) */
    assert(wchar_to_utf8(0x4E16, out) == 3);
    assert((unsigned char)out[0] == 0xE4);
    assert((unsigned char)out[1] == 0xB8);
    assert((unsigned char)out[2] == 0x96);

    /* 4-byte: U+1F600 (grinning face emoji) */
    assert(wchar_to_utf8(0x1F600, out) == 4);
    assert((unsigned char)out[0] == 0xF0);
    assert((unsigned char)out[1] == 0x9F);
    assert((unsigned char)out[2] == 0x98);
    assert((unsigned char)out[3] == 0x80);
}

static void test_utf8_char_width(void)
{
    /* ASCII is width 1 */
    assert(utf8_char_width("A", 1, 0) == 1);

    /* CJK character U+4E16 is width 2 */
    assert(utf8_char_width("\xE4\xB8\x96", 3, 0) == 2);
}

/* ================================================================
 *  buffer tests
 * ================================================================ */

static void test_buffer_init_free(void)
{
    Buffer buf;
    buffer_init(&buf);
    assert(buf.num_lines == 1);
    assert(buf.lines[0].len == 0);
    assert(strcmp(buf.lines[0].data, "") == 0);
    buffer_free(&buf);
}

static void test_buffer_insert_delete_line(void)
{
    Buffer buf;
    buffer_init(&buf);

    buffer_insert_line(&buf, 1, "hello", 5);
    assert(buf.num_lines == 2);
    assert(strcmp(buf.lines[1].data, "hello") == 0);

    buffer_insert_line(&buf, 1, "world", 5);
    assert(buf.num_lines == 3);
    assert(strcmp(buf.lines[1].data, "world") == 0);
    assert(strcmp(buf.lines[2].data, "hello") == 0);

    /* Insert at beginning */
    buffer_insert_line(&buf, 0, "first", 5);
    assert(buf.num_lines == 4);
    assert(strcmp(buf.lines[0].data, "first") == 0);

    /* Delete middle line */
    buffer_delete_line(&buf, 1);
    assert(buf.num_lines == 3);
    assert(strcmp(buf.lines[0].data, "first") == 0);
    assert(strcmp(buf.lines[1].data, "world") == 0);

    buffer_free(&buf);
}

static void test_buffer_insert_delete_char(void)
{
    Buffer buf;
    buffer_init(&buf);

    /* Insert chars to build "abc" */
    buffer_insert_char(&buf, 0, 0, 'a');
    buffer_insert_char(&buf, 0, 1, 'b');
    buffer_insert_char(&buf, 0, 2, 'c');
    assert(buf.lines[0].len == 3);
    assert(strcmp(buf.lines[0].data, "abc") == 0);

    /* Insert in the middle: "abXc" */
    buffer_insert_char(&buf, 0, 2, 'X');
    assert(strcmp(buf.lines[0].data, "abXc") == 0);

    /* Delete char (backspace at col=3 removes 'X') */
    buffer_delete_char(&buf, 0, 3);
    assert(strcmp(buf.lines[0].data, "abc") == 0);

    buffer_free(&buf);
}

static void test_buffer_delete_char_joins_lines(void)
{
    Buffer buf;
    buffer_init(&buf);

    buffer_insert_line(&buf, 0, "hello", 5);
    buffer_insert_line(&buf, 1, "world", 5);
    buffer_delete_line(&buf, 2);  /* remove the initial empty line */
    assert(buf.num_lines == 2);

    /* Backspace at col=0, row=1 joins with row=0 */
    int joined = buffer_delete_char(&buf, 1, 0);
    assert(joined == 1);
    assert(buf.num_lines == 1);
    assert(strcmp(buf.lines[0].data, "helloworld") == 0);

    buffer_free(&buf);
}

static void test_buffer_insert_newline(void)
{
    Buffer buf;
    buffer_init(&buf);

    /* Set up "helloworld" on line 0 */
    buffer_insert_line(&buf, 0, "helloworld", 10);
    buffer_delete_line(&buf, 1);  /* remove initial empty line */

    /* Split at col=5 */
    buffer_insert_newline(&buf, 0, 5);
    assert(buf.num_lines == 2);
    assert(strcmp(buf.lines[0].data, "hello") == 0);
    assert(buf.lines[0].len == 5);
    assert(strcmp(buf.lines[1].data, "world") == 0);
    assert(buf.lines[1].len == 5);

    buffer_free(&buf);
}

static void test_buffer_truncate_line(void)
{
    Buffer buf;
    buffer_init(&buf);

    buffer_insert_line(&buf, 0, "hello world", 11);
    buffer_truncate_line(&buf, 0, 5);
    assert(buf.lines[0].len == 5);
    assert(strcmp(buf.lines[0].data, "hello") == 0);

    /* Truncate at position beyond length — no-op */
    buffer_truncate_line(&buf, 0, 100);
    assert(buf.lines[0].len == 5);

    buffer_free(&buf);
}

/* ================================================================
 *  undo tests
 * ================================================================ */

static void test_undo_push_pop(void)
{
    UndoStack s;
    undo_stack_init(&s);

    undo_push(&s, UNDO_INSERT, 0, 0, "a", 1, 0, 0, 1);
    undo_push(&s, UNDO_INSERT, 0, 1, "b", 1, 0, 0, 2);

    /* LIFO: pop gives seq=2 first */
    UndoEntry *e = undo_pop(&s);
    assert(e != NULL);
    assert(e->seq == 2);

    e = undo_pop(&s);
    assert(e != NULL);
    assert(e->seq == 1);

    /* Pop from empty returns NULL */
    e = undo_pop(&s);
    assert(e == NULL);

    undo_stack_free(&s);
}

static void test_undo_top_seq(void)
{
    UndoStack s;
    undo_stack_init(&s);

    /* Empty stack returns -1 */
    assert(undo_top_seq(&s) == -1);

    undo_push(&s, UNDO_INSERT, 0, 0, "x", 1, 0, 0, 42);
    assert(undo_top_seq(&s) == 42);

    undo_push(&s, UNDO_DELETE, 1, 0, "y", 1, 0, 0, 99);
    assert(undo_top_seq(&s) == 99);

    undo_stack_free(&s);
}

static void test_undo_clear(void)
{
    UndoStack s;
    undo_stack_init(&s);

    undo_push(&s, UNDO_INSERT, 0, 0, "a", 1, 0, 0, 1);
    undo_push(&s, UNDO_INSERT, 0, 1, "b", 1, 0, 0, 2);
    assert(!undo_empty(&s));

    undo_stack_clear(&s);
    assert(undo_empty(&s));
    assert(undo_top_seq(&s) == -1);

    undo_stack_free(&s);
}

/* ================================================================
 *  render tests (block detection — pure string logic, no ncurses)
 * ================================================================ */

static void test_render_heading_level(void)
{
    assert(render_heading_level("# Heading") == 1);
    assert(render_heading_level("## Heading") == 2);
    assert(render_heading_level("### Heading") == 3);
    assert(render_heading_level("#### H4") == 4);
    assert(render_heading_level("##### H5") == 5);
    assert(render_heading_level("###### H6") == 6);

    /* 7+ hashes is not a valid heading */
    assert(render_heading_level("####### H7") == 0);

    /* No space after # — not a heading */
    assert(render_heading_level("#NotAHeading") == 0);

    /* Leading spaces (up to 3) allowed */
    assert(render_heading_level("  ## Indented") == 2);

    /* Bare # with nothing after is valid */
    assert(render_heading_level("#") == 1);

    /* Not a heading */
    assert(render_heading_level("hello") == 0);
    assert(render_heading_level("") == 0);
}

static void test_render_is_code_fence(void)
{
    assert(render_is_code_fence("```") == 1);
    assert(render_is_code_fence("```python") == 1);
    assert(render_is_code_fence("~~~") == 1);
    assert(render_is_code_fence("~~~~") == 1);

    /* Too few */
    assert(render_is_code_fence("``") == 0);
    assert(render_is_code_fence("~~") == 0);

    /* Leading spaces (up to 3) */
    assert(render_is_code_fence("  ```") == 1);

    /* Not a fence */
    assert(render_is_code_fence("hello") == 0);
}

static void test_render_get_block_type(void)
{
    /* Headings */
    assert(render_get_block_type("# H1", 0) == BLOCK_HEADING);

    /* Code fence */
    assert(render_get_block_type("```", 0) == BLOCK_CODE_FENCE);
    assert(render_get_block_type("```", 1) == BLOCK_CODE_FENCE);

    /* Inside code block, non-fence line is content */
    assert(render_get_block_type("just text", 1) == BLOCK_CODE_CONTENT);

    /* Empty line */
    assert(render_get_block_type("", 0) == BLOCK_EMPTY);
    assert(render_get_block_type("   ", 0) == BLOCK_EMPTY);

    /* Blockquote */
    assert(render_get_block_type("> quote", 0) == BLOCK_BLOCKQUOTE);
    assert(render_get_block_type(">", 0) == BLOCK_BLOCKQUOTE);

    /* Unordered list */
    assert(render_get_block_type("- item", 0) == BLOCK_LIST_UNORDERED);
    assert(render_get_block_type("* item", 0) == BLOCK_LIST_UNORDERED);
    assert(render_get_block_type("+ item", 0) == BLOCK_LIST_UNORDERED);

    /* Ordered list */
    assert(render_get_block_type("1. item", 0) == BLOCK_LIST_ORDERED);
    assert(render_get_block_type("99) item", 0) == BLOCK_LIST_ORDERED);

    /* Horizontal rule */
    assert(render_get_block_type("---", 0) == BLOCK_HRULE);
    assert(render_get_block_type("***", 0) == BLOCK_HRULE);
    assert(render_get_block_type("___", 0) == BLOCK_HRULE);

    /* Plain paragraph */
    assert(render_get_block_type("hello world", 0) == BLOCK_PARAGRAPH);
}

static void test_render_byte_to_col(void)
{
    /* ASCII: byte offset == column offset */
    assert(render_byte_to_col("hello", 5, 3) == 3);
    assert(render_byte_to_col("hello", 5, 0) == 0);

    /* Multi-byte: U+00E9 (2 bytes) has display width 1 */
    {
        const char *s = "\xC3\xA9X";  /* e-acute + X */
        assert(render_byte_to_col(s, 3, 2) == 1);  /* after e-acute: col 1 */
        assert(render_byte_to_col(s, 3, 3) == 2);  /* after X: col 2 */
    }

    /* CJK: U+4E16 (3 bytes) has display width 2 */
    {
        const char *s = "\xE4\xB8\x96X";  /* CJK char + X */
        assert(render_byte_to_col(s, 4, 3) == 2);  /* after CJK: col 2 */
        assert(render_byte_to_col(s, 4, 4) == 3);  /* after X: col 3 */
    }
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void)
{
    setlocale(LC_ALL, "");

    /* utf8 */
    test_utf8_clen();
    test_utf8_char_bytes();
    test_utf8_prev_char();
    test_wchar_to_utf8();
    test_utf8_char_width();

    /* buffer */
    test_buffer_init_free();
    test_buffer_insert_delete_line();
    test_buffer_insert_delete_char();
    test_buffer_delete_char_joins_lines();
    test_buffer_insert_newline();
    test_buffer_truncate_line();

    /* undo */
    test_undo_push_pop();
    test_undo_top_seq();
    test_undo_clear();

    /* render */
    test_render_heading_level();
    test_render_is_code_fence();
    test_render_get_block_type();
    test_render_byte_to_col();

    printf("All tests passed.\n");
    return 0;
}
