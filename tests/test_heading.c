/* test_heading — unit tests for CommonMark 4.2/4.3 heading compliance.
   Standalone binary with own main(). */
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "render.h"
#include "render_hrule.h"
#include "render_heading.h"
#include "buffer.h"

/* ================================================================
 *  Helper: build a Buffer from a const char* array
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
 *  Group A — render_heading_content
 * ================================================================ */

/* Helper: check that render_heading_content on `line` gives a result
   matching `expected`. */
static void check_content(const char *line, const char *expected)
{
    int start, len;
    int llen = (int)strlen(line);
    render_heading_content(line, llen, &start, &len);
    assert(len == (int)strlen(expected));
    assert(strncmp(line + start, expected, (size_t)len) == 0);
}

static void test_content_basic(void)
{
    check_content("# Heading", "Heading");
}

static void test_content_closing_hash_single(void)
{
    check_content("# Heading #", "Heading");
}

static void test_content_closing_hash_multi(void)
{
    check_content("# Heading ##", "Heading");
}

static void test_content_closing_hash_mid(void)
{
    /* Middle # is not closing — only the trailing run after a space */
    check_content("# Heading # bar #", "Heading # bar");
}

static void test_content_closing_hash_no_space(void)
{
    /* No space before trailing #bar → not a closing sequence */
    check_content("# Heading #bar", "Heading #bar");
}

static void test_content_extra_spaces(void)
{
    /* Extra leading and trailing spaces in content */
    check_content("#    Content    ", "Content");
}

static void test_content_h2(void)
{
    check_content("## Sub #", "Sub");
}

static void test_content_empty_heading(void)
{
    /* "# " is a valid empty heading */
    check_content("# ", "");
}

static void test_content_indented(void)
{
    /* Up to 3 leading spaces are part of the ATX marker, not content */
    check_content("   # Indented heading #", "Indented heading");
}

/* ================================================================
 *  Group B — render_is_setext_underline (already tested in test_hrule.c,
 *  but repeated here for completeness of the heading test suite)
 * ================================================================ */

static void test_setext_basic(void)
{
    assert(render_is_setext_underline("=")   == 1);
    assert(render_is_setext_underline("===") == 1);
    assert(render_is_setext_underline("-")   == 2);
    assert(render_is_setext_underline("---") == 2);
}

static void test_setext_trailing_spaces(void)
{
    assert(render_is_setext_underline("---   ") == 2);
    assert(render_is_setext_underline("===   ") == 1);
}

static void test_setext_invalid(void)
{
    assert(render_is_setext_underline("= =")   == 0);
    assert(render_is_setext_underline("- - -") == 0);
    assert(render_is_setext_underline("")      == 0);
}

/* ================================================================
 *  Group C — render_get_block_type_ctx for setext context
 * ================================================================ */

static void test_ctx_dash_after_paragraph(void)
{
    assert(render_get_block_type_ctx("---", "Some paragraph text", 0) == BLOCK_SETEXT_H2);
}

static void test_ctx_dash_after_heading(void)
{
    /* Previous line is a heading, not a paragraph → thematic break */
    assert(render_get_block_type_ctx("---", "## My Heading", 0) == BLOCK_HRULE);
}

static void test_ctx_dash_no_prev(void)
{
    assert(render_get_block_type_ctx("---", NULL, 0) == BLOCK_HRULE);
    assert(render_get_block_type_ctx("---", "",   0) == BLOCK_HRULE);
}

/* ================================================================
 *  Group D — setext preview via preview_generate
 * ================================================================ */

static void test_setext_h1_preview(void)
{
    const char *lines[] = { "Foo", "===" };
    Buffer buf = make_buf(lines, 2);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* Expect: line 0 = "Foo" styled as H1, line 1 = PM_HLINE underline,
       line 2 = empty (suppressed "===" row). */
    assert(pb.num_lines >= 3);

    /* Line 0: text is "Foo" with CP_HEADING1 */
    PreviewLine *pl0 = &pb.lines[0];
    assert(pl0->len >= 3);
    assert(strncmp(pl0->text, "Foo", 3) == 0);
    assert(pl0->styles[0].cpair == CP_HEADING1);

    /* Line 1: PM_HLINE underline */
    PreviewLine *ul = &pb.lines[1];
    assert(ul->len > 0);
    assert(ul->styles[0].acs == PM_HLINE);

    preview_free(&pb);
    buffer_free(&buf);
}

static void test_setext_h2_preview(void)
{
    const char *lines[] = { "Bar", "---" };
    Buffer buf = make_buf(lines, 2);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    /* H2: no underline row, just the heading text + empty for "---" */
    assert(pb.num_lines >= 2);

    PreviewLine *pl0 = &pb.lines[0];
    assert(pl0->len >= 3);
    assert(strncmp(pl0->text, "Bar", 3) == 0);
    assert(pl0->styles[0].cpair == CP_HEADING2);

    /* No PM_HLINE for H2 */
    assert(pb.lines[0].styles[0].acs == 0);

    preview_free(&pb);
    buffer_free(&buf);
}

static void test_atx_closing_hash_stripped_in_preview(void)
{
    /* ATX heading with closing # must strip it from preview text */
    const char *lines[] = { "# Title #" };
    Buffer buf = make_buf(lines, 1);
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, 80);

    assert(pb.num_lines >= 1);
    PreviewLine *pl = &pb.lines[0];

    /* The visible text should be "Title", not "Title #" */
    assert(pl->len >= 5);
    assert(strncmp(pl->text, "Title", 5) == 0);
    /* Ensure no '#' appears in the preview text */
    for (int i = 0; i < pl->len; i++)
        assert(pl->text[i] != '#');

    preview_free(&pb);
    buffer_free(&buf);
}

static void test_setext_h1_gets_underline_but_h2_does_not(void)
{
    /* === underline line should be suppressed (gen_empty) and H1 gets PM_HLINE;
       --- should be suppressed and H2 gets NO PM_HLINE. */
    const char *lines1[] = { "Title", "===" };
    Buffer buf1 = make_buf(lines1, 2);
    PreviewBuffer pb1 = {0};
    preview_generate(&pb1, &buf1, 80);
    assert(pb1.num_lines >= 2);
    assert(pb1.lines[1].styles[0].acs == PM_HLINE);
    preview_free(&pb1);
    buffer_free(&buf1);

    const char *lines2[] = { "Title", "---" };
    Buffer buf2 = make_buf(lines2, 2);
    PreviewBuffer pb2 = {0};
    preview_generate(&pb2, &buf2, 80);
    /* H2: only 2 lines (heading + empty), no underline */
    assert(pb2.num_lines >= 1);
    assert(pb2.lines[0].styles[0].acs == 0);
    preview_free(&pb2);
    buffer_free(&buf2);
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void)
{
    setlocale(LC_ALL, "");

    /* Group A: render_heading_content */
    test_content_basic();
    test_content_closing_hash_single();
    test_content_closing_hash_multi();
    test_content_closing_hash_mid();
    test_content_closing_hash_no_space();
    test_content_extra_spaces();
    test_content_h2();
    test_content_empty_heading();
    test_content_indented();

    /* Group B: render_is_setext_underline */
    test_setext_basic();
    test_setext_trailing_spaces();
    test_setext_invalid();

    /* Group C: render_get_block_type_ctx */
    test_ctx_dash_after_paragraph();
    test_ctx_dash_after_heading();
    test_ctx_dash_no_prev();

    /* Group D: preview_generate */
    test_setext_h1_preview();
    test_setext_h2_preview();
    test_atx_closing_hash_stripped_in_preview();
    test_setext_h1_gets_underline_but_h2_does_not();

    printf("All heading tests passed.\n");
    return 0;
}
