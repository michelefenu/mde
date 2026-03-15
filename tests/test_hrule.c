/* test_hrule — unit tests for CommonMark Section 4.1 thematic break detection
   and setext underline detection. Standalone binary with own main(). */
#include <assert.h>
#include <stdio.h>
#include "render_hrule.h"
#include "render.h"

/* ── render_is_thematic_break ── */

static void test_thematic_break_basic(void)
{
    assert(render_is_thematic_break("---")    == 1);
    assert(render_is_thematic_break("***")    == 1);
    assert(render_is_thematic_break("___")    == 1);
}

static void test_thematic_break_spaced(void)
{
    assert(render_is_thematic_break("- - - - -") == 1);
    assert(render_is_thematic_break(" *  *  *")  == 1);
    assert(render_is_thematic_break("  ***")     == 1);
}

static void test_thematic_break_too_short(void)
{
    assert(render_is_thematic_break("--") == 0);
    assert(render_is_thematic_break("**") == 0);
    assert(render_is_thematic_break("__") == 0);
}

static void test_thematic_break_with_other_chars(void)
{
    assert(render_is_thematic_break("---a") == 0);
    assert(render_is_thematic_break("- -a") == 0);
}

static void test_thematic_break_equals_not_valid(void)
{
    /* === is NOT a thematic break in CommonMark */
    assert(render_is_thematic_break("===") == 0);
    assert(render_is_thematic_break("= = =") == 0);
}

/* ── render_is_setext_underline ── */

static void test_setext_underline_h2(void)
{
    assert(render_is_setext_underline("---")    == 2);
    assert(render_is_setext_underline("---  ")  == 2);
    assert(render_is_setext_underline("-")      == 2);
    assert(render_is_setext_underline("------") == 2);
}

static void test_setext_underline_h1(void)
{
    assert(render_is_setext_underline("===")    == 1);
    assert(render_is_setext_underline("=")      == 1);
    assert(render_is_setext_underline("======") == 1);
}

static void test_setext_underline_not_valid(void)
{
    /* Spaces between chars → thematic break only, not setext */
    assert(render_is_setext_underline("- - -") == 0);
    /* * is not a setext char */
    assert(render_is_setext_underline("***")   == 0);
    assert(render_is_setext_underline("")      == 0);
}

/* ── render_get_block_type_ctx ── */

static void test_ctx_dash_after_paragraph(void)
{
    /* "---" after paragraph text → setext H2, not thematic break */
    assert(render_get_block_type_ctx("---", "Some paragraph text", 0) == BLOCK_SETEXT_H2);
}

static void test_ctx_equals_after_paragraph(void)
{
    assert(render_get_block_type_ctx("===", "Some paragraph text", 0) == BLOCK_SETEXT_H1);
}

static void test_ctx_dash_no_prev(void)
{
    /* No previous line → standalone thematic break */
    assert(render_get_block_type_ctx("---", NULL,  0) == BLOCK_HRULE);
    assert(render_get_block_type_ctx("---", "",    0) == BLOCK_HRULE);
}

static void test_ctx_dash_after_heading(void)
{
    /* Previous line is a heading, not paragraph → thematic break */
    assert(render_get_block_type_ctx("---", "## My Heading", 0) == BLOCK_HRULE);
}

static void test_ctx_spaced_dashes_after_paragraph(void)
{
    /* "- - -" has spaces between → thematic break even after paragraph */
    assert(render_get_block_type_ctx("- - -", "Some paragraph text", 0) == BLOCK_HRULE);
}

static void test_ctx_in_code_block(void)
{
    /* Inside a code block, context is ignored */
    assert(render_get_block_type_ctx("---", "Some paragraph text", 1) != BLOCK_SETEXT_H2);
}

int main(void)
{
    test_thematic_break_basic();
    test_thematic_break_spaced();
    test_thematic_break_too_short();
    test_thematic_break_with_other_chars();
    test_thematic_break_equals_not_valid();

    test_setext_underline_h2();
    test_setext_underline_h1();
    test_setext_underline_not_valid();

    test_ctx_dash_after_paragraph();
    test_ctx_equals_after_paragraph();
    test_ctx_dash_no_prev();
    test_ctx_dash_after_heading();
    test_ctx_spaced_dashes_after_paragraph();
    test_ctx_in_code_block();

    printf("All hrule tests passed.\n");
    return 0;
}
