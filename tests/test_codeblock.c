#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"
#include "render.h"

/* ================================================================
 *  Helper: build a Buffer from an array of C strings
 * ================================================================ */

static Buffer make_buf(const char **lines, int n)
{
    Buffer buf;
    buffer_init(&buf);
    if (n > 0) {
        buffer_insert_line(&buf, 0, lines[0], (int)strlen(lines[0]));
        buffer_delete_line(&buf, 1);
        for (int i = 1; i < n; i++)
            buffer_insert_line(&buf, i, lines[i], (int)strlen(lines[i]));
    }
    return buf;
}

/* ================================================================
 *  A. render_parse_fence unit tests
 * ================================================================ */

static void test_parse_fence_basic(void)
{
    char fc;

    /* Plain backtick fence */
    assert(render_parse_fence("```", &fc) == 3);
    assert(fc == '`');

    /* Backtick fence with info string: length is 4 */
    assert(render_parse_fence("````python", &fc) == 4);
    assert(fc == '`');

    /* Tilde fence */
    assert(render_parse_fence("~~~", &fc) == 3);
    assert(fc == '~');

    /* Leading spaces (up to 3) are allowed */
    assert(render_parse_fence("  ```", &fc) == 3);
    assert(fc == '`');

    /* 4 leading spaces = indented code block, not a fence */
    assert(render_parse_fence("    ```", &fc) == 0);

    /* Too short */
    assert(render_parse_fence("``", &fc) == 0);
    assert(render_parse_fence("~~", &fc) == 0);

    /* NULL out_fc is safe */
    assert(render_parse_fence("```", NULL) == 3);
}

/* ================================================================
 *  B. Closing fence compliance (preview_generate integration)
 * ================================================================ */

/* Opening ``` cannot be closed by ~~~ */
static void test_close_fence_wrong_char(void)
{
    const char *lines[] = {
        "```",
        "code",
        "~~~",
        "still code",
        "```"
    };
    Buffer buf = make_buf(lines, 5);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* The ~~~ line should be treated as content, not a closer.
       The block closes on the final ```.
       Box contains "code", "~~~", "still code":
       top border + 3 content + bottom border = 5 */
    assert(pb.num_lines == 5);

    preview_free(&pb);
    buffer_free(&buf);
}

/* Closing fence must be >= opening fence length */
static void test_close_fence_too_short(void)
{
    /* Opening ```` (4 backticks) cannot be closed by ``` (3) */
    const char *lines[] = {
        "````",
        "code line",
        "```",          /* too short — content, not closer */
        "````"          /* correct closer */
    };
    Buffer buf = make_buf(lines, 4);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* top + ("code line", "```") + bottom = 4 lines */
    assert(pb.num_lines == 4);

    preview_free(&pb);
    buffer_free(&buf);
}

/* Closing fence with an info string is content, not a closer */
static void test_close_fence_with_info_string(void)
{
    const char *lines[] = {
        "```",
        "code",
        "``` python",   /* info string — treated as content */
        "```"           /* actual closer */
    };
    Buffer buf = make_buf(lines, 4);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* top + ("code", "``` python") + bottom = 4 */
    assert(pb.num_lines == 4);

    preview_free(&pb);
    buffer_free(&buf);
}

/* Unclosed fence: all remaining lines are content */
static void test_unclosed_fence(void)
{
    const char *lines[] = {
        "```",
        "line one",
        "line two"
    };
    Buffer buf = make_buf(lines, 3);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* top + ("line one", "line two") + bottom = 4 */
    assert(pb.num_lines == 4);

    preview_free(&pb);
    buffer_free(&buf);
}

/* ================================================================
 *  C. Indented code block (preview_generate integration)
 * ================================================================ */

/* 4-space indent produces a code box */
static void test_icb_four_spaces(void)
{
    const char *lines[] = { "    code here" };
    Buffer buf = make_buf(lines, 1);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* top border + 1 content line + bottom border = 3 lines */
    assert(pb.num_lines == 3);
    /* Content line has PM_VLINE borders */
    assert(pb.lines[1].styles[0].acs == PM_VLINE);

    preview_free(&pb);
    buffer_free(&buf);
}

/* Tab indent also produces a code box */
static void test_icb_tab_indent(void)
{
    const char *lines[] = { "\tcode here" };
    Buffer buf = make_buf(lines, 1);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    assert(pb.num_lines == 3);
    assert(pb.lines[1].styles[0].acs == PM_VLINE);

    preview_free(&pb);
    buffer_free(&buf);
}

/* 3-space indent is NOT an indented code block */
static void test_icb_three_spaces_not_code(void)
{
    const char *lines[] = { "   not a code block" };
    Buffer buf = make_buf(lines, 1);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* Should render as a single paragraph line, no box borders */
    assert(pb.num_lines == 1);
    assert(pb.lines[0].styles[0].acs == 0);

    preview_free(&pb);
    buffer_free(&buf);
}

/* Cannot interrupt a paragraph */
static void test_icb_cannot_interrupt_paragraph(void)
{
    const char *lines[] = {
        "paragraph text",
        "    indented line"
    };
    Buffer buf = make_buf(lines, 2);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* Both lines render as paragraphs — no code box */
    assert(pb.num_lines == 2);
    assert(pb.lines[0].styles[0].acs == 0);
    assert(pb.lines[1].styles[0].acs == 0);

    preview_free(&pb);
    buffer_free(&buf);
}

/* Blank line resets prev_para; next indented line IS a code block */
static void test_icb_after_blank(void)
{
    const char *lines[] = {
        "paragraph",
        "",
        "    code after blank"
    };
    Buffer buf = make_buf(lines, 3);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* paragraph + empty + top_border + content + bottom_border = 5 */
    assert(pb.num_lines == 5);
    /* The code box top border is at index 2 */
    assert(pb.lines[2].styles[0].acs == PM_ULCORNER);

    preview_free(&pb);
    buffer_free(&buf);
}

/* Multiple consecutive indented lines form one box */
static void test_icb_multiple_lines(void)
{
    const char *lines[] = {
        "    line one",
        "    line two",
        "    line three"
    };
    Buffer buf = make_buf(lines, 3);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* top + 3 content + bottom = 5 */
    assert(pb.num_lines == 5);

    preview_free(&pb);
    buffer_free(&buf);
}

/* Blank lines within the block are included; trailing blanks are stripped */
static void test_icb_internal_blank_trailing_stripped(void)
{
    const char *lines[] = {
        "    first",
        "",             /* blank — included as content since first indented line came before */
        "    second",
        ""              /* trailing blank — stripped from the box */
    };
    Buffer buf = make_buf(lines, 4);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* The box contains: "first", "", "second" = 3 content lines.
       The trailing blank at row 3 is consumed by the ICB scan loop
       (it falls inside the block span) but excluded from the box.
       No extra empty line is emitted after the box.
       => top + 3 content + bottom = 5 */
    assert(pb.num_lines == 5);

    preview_free(&pb);
    buffer_free(&buf);
}

/* 4-space-indented list sub-item after a list item is NOT a code block */
static void test_icb_list_subitem_not_code(void)
{
    const char *lines[] = {
        "* overview",
        "    * philosophy",
        "    * html"
    };
    Buffer buf = make_buf(lines, 3);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* Should render as 3 plain list item lines, no code box borders */
    assert(pb.num_lines == 3);
    for (int i = 0; i < 3; i++)
        assert(pb.lines[i].styles[0].acs != PM_ULCORNER &&
               pb.lines[i].styles[0].acs != PM_VLINE);

    preview_free(&pb);
    buffer_free(&buf);
}

/* ================================================================
 *  D. Interaction: 4-space inside fenced block is content, not new block
 * ================================================================ */

static void test_icb_inside_fenced_ignored(void)
{
    const char *lines[] = {
        "```",
        "    indented inside fence",
        "```"
    };
    Buffer buf = make_buf(lines, 3);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* top border + 1 content line + bottom border = 3 */
    assert(pb.num_lines == 3);
    /* Content line has PM_VLINE (fenced block border), not ULCORNER */
    assert(pb.lines[1].styles[0].acs == PM_VLINE);

    preview_free(&pb);
    buffer_free(&buf);
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void)
{
    setlocale(LC_ALL, "");

    /* A. render_parse_fence */
    test_parse_fence_basic();

    /* B. Closing fence compliance */
    test_close_fence_wrong_char();
    test_close_fence_too_short();
    test_close_fence_with_info_string();
    test_unclosed_fence();

    /* C. Indented code block */
    test_icb_four_spaces();
    test_icb_tab_indent();
    test_icb_three_spaces_not_code();
    test_icb_cannot_interrupt_paragraph();
    test_icb_after_blank();
    test_icb_multiple_lines();
    test_icb_internal_blank_trailing_stripped();

    /* D. Interaction */
    test_icb_list_subitem_not_code();
    test_icb_inside_fenced_ignored();

    printf("All codeblock tests passed.\n");
    return 0;
}
