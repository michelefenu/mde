/* test_list — Unit tests for CommonMark list item rendering (Sections 5.2–5.3).
   Tests multi-paragraph (loose) list items, nested sub-lists, and ICBs inside
   list items via the recursive VLine-stripping approach in preview_gen_vlines. */
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "render.h"
#include "buffer.h"

/* ------------------------------------------------------------------ */
/*  Helper: build a PreviewBuffer from an array of raw markdown lines  */
/* ------------------------------------------------------------------ */

static PreviewBuffer make_preview(const char **lines, int nlines, int cols)
{
    Buffer buf;
    buffer_init(&buf);
    if (nlines > 0) {
        buffer_insert_line(&buf, 0, lines[0], (int)strlen(lines[0]));
        buffer_delete_line(&buf, 1);  /* remove initial empty line */
        for (int i = 1; i < nlines; i++)
            buffer_insert_line(&buf, i, lines[i], (int)strlen(lines[i]));
    }
    PreviewBuffer pb = {0};
    preview_generate(&pb, &buf, cols);
    buffer_free(&buf);
    return pb;
}

/* Returns 1 if any CharStyle in PreviewLine has the given acs marker. */
static int line_has_acs(PreviewLine *pl, unsigned char acs)
{
    for (int i = 0; i < pl->len; i++) {
        if (pl->styles[i].acs == acs) return 1;
    }
    return 0;
}

/* Returns 1 if any CharStyle in PreviewLine has the given cpair. */
static int line_has_cpair(PreviewLine *pl, short cpair)
{
    for (int i = 0; i < pl->len; i++) {
        if (pl->styles[i].cpair == cpair) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Test A: single-item bullet list                                    */
/* ------------------------------------------------------------------ */

static void test_single_bullet(void)
{
    const char *lines[] = { "- item one" };
    PreviewBuffer pb = make_preview(lines, 1, 80);

    assert(pb.num_lines == 1);
    assert(line_has_acs(&pb.lines[0], PM_BULLET));

    preview_free(&pb);
    printf("  test_single_bullet: OK\n");
}

/* ------------------------------------------------------------------ */
/*  Test B: tight multi-item bullet list                               */
/* ------------------------------------------------------------------ */

static void test_tight_list(void)
{
    const char *lines[] = { "- alpha", "- beta", "- gamma" };
    PreviewBuffer pb = make_preview(lines, 3, 80);

    assert(pb.num_lines == 3);
    assert(line_has_acs(&pb.lines[0], PM_BULLET));
    assert(line_has_acs(&pb.lines[1], PM_BULLET));
    assert(line_has_acs(&pb.lines[2], PM_BULLET));

    preview_free(&pb);
    printf("  test_tight_list: OK\n");
}

/* ------------------------------------------------------------------ */
/*  Test C: loose bullet list item (continuation paragraph)           */
/* ------------------------------------------------------------------ */

static void test_loose_bullet(void)
{
    /* - first para
       <blank>
         second para   (2-space indent = W for "- ") */
    const char *lines[] = { "- first para", "", "  second para" };
    PreviewBuffer pb = make_preview(lines, 3, 80);

    /* Expect: bullet+first, empty, indent+second — no code box */
    assert(pb.num_lines == 3);

    /* First line has the bullet */
    assert(line_has_acs(&pb.lines[0], PM_BULLET));

    /* No PM_ULCORNER anywhere (would appear if treated as ICB code block) */
    for (int i = 0; i < pb.num_lines; i++)
        assert(!line_has_acs(&pb.lines[i], PM_ULCORNER));

    preview_free(&pb);
    printf("  test_loose_bullet: OK\n");
}

/* ------------------------------------------------------------------ */
/*  Test D: nested sub-list                                            */
/* ------------------------------------------------------------------ */

static void test_nested_list(void)
{
    /* - outer
         - inner   (2-space indent = W for "- ") */
    const char *lines[] = { "- outer", "  - inner" };
    PreviewBuffer pb = make_preview(lines, 2, 80);

    assert(pb.num_lines == 2);
    /* Both lines have a bullet */
    assert(line_has_acs(&pb.lines[0], PM_BULLET));
    assert(line_has_acs(&pb.lines[1], PM_BULLET));

    /* No code box */
    for (int i = 0; i < pb.num_lines; i++)
        assert(!line_has_acs(&pb.lines[i], PM_ULCORNER));

    preview_free(&pb);
    printf("  test_nested_list: OK\n");
}

/* ------------------------------------------------------------------ */
/*  Test E: ICB inside a list item (4 extra spaces beyond W)          */
/* ------------------------------------------------------------------ */

static void test_icb_in_list(void)
{
    /* - item
       <blank>
             code block   (6 spaces = W(2) + 4 → ICB inside item) */
    const char *lines[] = { "- item", "", "      code block" };
    PreviewBuffer pb = make_preview(lines, 3, 80);

    /* Should produce >= 3 preview lines including a code box */
    assert(pb.num_lines >= 3);

    /* At least one line must have PM_ULCORNER (top of code box) */
    int found_box = 0;
    for (int i = 0; i < pb.num_lines; i++)
        if (line_has_acs(&pb.lines[i], PM_ULCORNER)) { found_box = 1; break; }
    assert(found_box);

    preview_free(&pb);
    printf("  test_icb_in_list: OK\n");
}

/* ------------------------------------------------------------------ */
/*  Test F: ordered list                                               */
/* ------------------------------------------------------------------ */

static void test_ordered_list(void)
{
    const char *lines[] = { "1. first", "2. second" };
    PreviewBuffer pb = make_preview(lines, 2, 80);

    assert(pb.num_lines == 2);
    assert(line_has_cpair(&pb.lines[0], CP_LIST_MARKER));
    assert(line_has_cpair(&pb.lines[1], CP_LIST_MARKER));

    preview_free(&pb);
    printf("  test_ordered_list: OK\n");
}

/* ------------------------------------------------------------------ */
/*  Test G: loose ordered list item                                    */
/* ------------------------------------------------------------------ */

static void test_loose_ordered(void)
{
    /* 1. para one
       <blank>
          para two   (3-space indent = W for "1. ") */
    const char *lines[] = { "1. para one", "", "   para two" };
    PreviewBuffer pb = make_preview(lines, 3, 80);

    assert(pb.num_lines == 3);

    /* No code box */
    for (int i = 0; i < pb.num_lines; i++)
        assert(!line_has_acs(&pb.lines[i], PM_ULCORNER));

    /* First line has LIST_MARKER */
    assert(line_has_cpair(&pb.lines[0], CP_LIST_MARKER));

    preview_free(&pb);
    printf("  test_loose_ordered: OK\n");
}

/* ------------------------------------------------------------------ */
/*  Test H: 4-space continuation is NOT a code block when W=4         */
/* ------------------------------------------------------------------ */

static void test_continuation_not_icb(void)
{
    /* "1.  item" → W=4 (leading 0 + "1." + 2 spaces)
       "    continuation" → 4 spaces = W, so it's a continuation paragraph */
    const char *lines[] = { "1.  item", "", "    continuation" };
    PreviewBuffer pb = make_preview(lines, 3, 80);

    assert(pb.num_lines == 3);

    /* No code box */
    for (int i = 0; i < pb.num_lines; i++)
        assert(!line_has_acs(&pb.lines[i], PM_ULCORNER));

    preview_free(&pb);
    printf("  test_continuation_not_icb: OK\n");
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    setlocale(LC_ALL, "");
    printf("test_list:\n");

    test_single_bullet();
    test_tight_list();
    test_loose_bullet();
    test_nested_list();
    test_icb_in_list();
    test_ordered_list();
    test_loose_ordered();
    test_continuation_not_icb();

    printf("  All list tests passed.\n");
    return 0;
}
