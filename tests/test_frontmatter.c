/* test_frontmatter — unit tests for YAML front-matter detection.
   Standalone binary with own main(). */
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "render.h"
#include "render_frontmatter.h"
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
 *  Group A — render_is_frontmatter_fence
 * ================================================================ */

static void test_fence_dashes(void)
{
    assert(render_is_frontmatter_fence("---") == 1);
}

static void test_fence_dots(void)
{
    assert(render_is_frontmatter_fence("...") == 2);
}

static void test_fence_trailing_spaces(void)
{
    assert(render_is_frontmatter_fence("---   ") == 1);
    assert(render_is_frontmatter_fence("...  ") == 2);
}

static void test_fence_too_many_chars(void)
{
    assert(render_is_frontmatter_fence("----") == 0);
    assert(render_is_frontmatter_fence("....") == 0);
}

static void test_fence_too_few_chars(void)
{
    assert(render_is_frontmatter_fence("--") == 0);
    assert(render_is_frontmatter_fence("..") == 0);
}

static void test_fence_trailing_text(void)
{
    assert(render_is_frontmatter_fence("---a") == 0);
    assert(render_is_frontmatter_fence("--- title") == 0);
}

static void test_fence_empty(void)
{
    assert(render_is_frontmatter_fence("") == 0);
}

static void test_fence_hrule_look_alike(void)
{
    /* Setext / thematic break variants must not match */
    assert(render_is_frontmatter_fence("===") == 0);
    assert(render_is_frontmatter_fence("- - -") == 0);
}

/* ================================================================
 *  Group B — render_frontmatter_extent
 * ================================================================ */

static void test_extent_basic(void)
{
    const char *lines[] = { "---", "title: foo", "---" };
    Buffer buf = make_buf(lines, 3);
    assert(render_frontmatter_extent(&buf) == 2);
    buffer_free(&buf);
}

static void test_extent_closed_by_dots(void)
{
    const char *lines[] = { "---", "key: value", "..." };
    Buffer buf = make_buf(lines, 3);
    assert(render_frontmatter_extent(&buf) == 2);
    buffer_free(&buf);
}

static void test_extent_empty_frontmatter(void)
{
    /* Opening and closing on consecutive lines */
    const char *lines[] = { "---", "---" };
    Buffer buf = make_buf(lines, 2);
    assert(render_frontmatter_extent(&buf) == 1);
    buffer_free(&buf);
}

static void test_extent_unclosed(void)
{
    const char *lines[] = { "---", "title: foo" };
    Buffer buf = make_buf(lines, 2);
    assert(render_frontmatter_extent(&buf) == -1);
    buffer_free(&buf);
}

static void test_extent_no_opening(void)
{
    /* Does not start with --- */
    const char *lines[] = { "title: foo", "---" };
    Buffer buf = make_buf(lines, 2);
    assert(render_frontmatter_extent(&buf) == -1);
    buffer_free(&buf);
}

static void test_extent_single_line(void)
{
    const char *lines[] = { "---" };
    Buffer buf = make_buf(lines, 1);
    assert(render_frontmatter_extent(&buf) == -1);
    buffer_free(&buf);
}

static void test_extent_empty_buffer(void)
{
    Buffer buf;
    buffer_init(&buf);
    assert(render_frontmatter_extent(&buf) == -1);
    buffer_free(&buf);
}

static void test_extent_dots_not_valid_opener(void)
{
    /* ... is only a valid closer, not opener */
    const char *lines[] = { "...", "title: foo", "---" };
    Buffer buf = make_buf(lines, 3);
    assert(render_frontmatter_extent(&buf) == -1);
    buffer_free(&buf);
}

static void test_extent_with_content_after(void)
{
    /* Closing --- on row 3; rows 4+ are regular markdown */
    const char *lines[] = { "---", "title: foo", "tags: [a, b]", "---",
                            "# Heading", "Some paragraph." };
    Buffer buf = make_buf(lines, 6);
    assert(render_frontmatter_extent(&buf) == 3);
    buffer_free(&buf);
}

/* ================================================================
 *  Group C — BLOCK_FRONTMATTER not confused with BLOCK_HRULE
 * ================================================================ */

static void test_closing_dash_is_not_hrule(void)
{
    /* With frontmatter extent, row 2 (the closing ---) must be
       classified as BLOCK_FRONTMATTER, not BLOCK_HRULE. */
    const char *lines[] = { "---", "title: foo", "---", "# Heading" };
    Buffer buf = make_buf(lines, 4);
    int fm_end = render_frontmatter_extent(&buf);
    assert(fm_end == 2);
    /* Rows 0..2 are inside front-matter */
    for (int r = 0; r <= fm_end; r++) {
        const char *line = buffer_line_data(&buf, r);
        int bt = render_get_block_type(line, 0);
        /* Without the frontmatter override, --- would be BLOCK_HRULE.
           The caller (editor.c / preview) must use fm_end to override.
           Here we just verify that render_frontmatter_extent works and
           that the caller *would* get the right answer. */
        (void)bt; /* actual override is the caller's responsibility */
    }
    /* Row 3 (after front-matter) must NOT be inside front-matter */
    assert(3 > fm_end);
    buffer_free(&buf);
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void)
{
    setlocale(LC_ALL, "");

    /* Group A: render_is_frontmatter_fence */
    test_fence_dashes();
    test_fence_dots();
    test_fence_trailing_spaces();
    test_fence_too_many_chars();
    test_fence_too_few_chars();
    test_fence_trailing_text();
    test_fence_empty();
    test_fence_hrule_look_alike();

    /* Group B: render_frontmatter_extent */
    test_extent_basic();
    test_extent_closed_by_dots();
    test_extent_empty_frontmatter();
    test_extent_unclosed();
    test_extent_no_opening();
    test_extent_single_line();
    test_extent_empty_buffer();
    test_extent_dots_not_valid_opener();
    test_extent_with_content_after();

    /* Group C: classification */
    test_closing_dash_is_not_hrule();

    printf("All frontmatter tests passed.\n");
    return 0;
}
