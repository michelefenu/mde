/* render_heading — ATX and setext heading rendering for preview mode. */
#include "render_heading.h"
#include "utf8.h"
#include "xalloc.h"
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  render_heading_content — CommonMark 4.2 compliant content extraction
 * ================================================================ */

void render_heading_content(const char *line, int len,
                            int *out_start, int *out_len)
{
    int i = 0;

    /* Skip up to 3 leading spaces */
    int spaces = 0;
    while (i < len && line[i] == ' ' && spaces < 3) { i++; spaces++; }

    /* Skip '#' markers */
    while (i < len && line[i] == '#') i++;

    /* Skip mandatory space (or end of line for empty heading) */
    if (i < len && line[i] == ' ') i++;

    /* Skip additional leading spaces in content */
    while (i < len && line[i] == ' ') i++;

    int content_start = i;
    int content_end   = len;

    /* Strip trailing spaces */
    while (content_end > content_start && line[content_end - 1] == ' ')
        content_end--;

    /* Strip optional closing # sequence:
       the run of '#' must be preceded by a space (or be the entire content).
       "# Foo #"   → strip " #"
       "# Foo ##"  → strip " ##"
       "# Foo #bar" → do NOT strip (no space before the last #-run) */
    if (content_end > content_start) {
        int e = content_end;
        /* Walk back over trailing '#' chars */
        while (e > content_start && line[e - 1] == '#') e--;
        int hash_start = e;
        if (hash_start < content_end) {
            /* There is at least one trailing '#'.
               It must be preceded by a space, or start exactly at content_start
               (entire content is just '#' chars). */
            if (hash_start == content_start ||
                line[hash_start - 1] == ' ') {
                content_end = hash_start;
                /* Strip the preceding space */
                if (content_end > content_start && line[content_end - 1] == ' ')
                    content_end--;
            }
        }
    }

    /* Strip trailing spaces again (after removing closing #) */
    while (content_end > content_start && line[content_end - 1] == ' ')
        content_end--;

    *out_start = content_start;
    *out_len   = content_end - content_start;
    if (*out_len < 0) *out_len = 0;
}

/* ================================================================
 *  gen_heading — ATX heading preview generator
 * ================================================================ */

void gen_heading(PreviewBuffer *pb, const char *line, int len,
                 int source_row, int screen_cols, int *link_idx)
{
    int hlevel = render_heading_level(line);
    int indent = (hlevel <= 1) ? 0 : (hlevel - 1) * 2;
    if (indent > 8) indent = 8;

    short cpair;
    switch (hlevel) {
    case 1:  cpair = CP_HEADING1; break;
    case 2:  cpair = CP_HEADING2; break;
    case 3:  cpair = CP_HEADING3; break;
    default: cpair = CP_HEADING4; break;
    }

    int cstart, clen;
    render_heading_content(line, len, &cstart, &clen);

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + cstart, clen, A_BOLD, cpair, &ct, &cs, &cl, link_idx);

    int total = indent + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);

    /* H1 gets an underline */
    if (hlevel == 1 && cl > 0) {
        int uw = indent + cl;
        if (uw > screen_cols) uw = screen_cols;
        PreviewLine *ul = pv_add(pb, source_row, uw);
        pv_fill_acs(ul, 0, uw, PM_HLINE, A_BOLD, cpair);
        ul->text[uw] = '\0';
    }
}

/* ================================================================
 *  gen_setext_heading — setext heading preview generator
 * ================================================================ */

void gen_setext_heading(PreviewBuffer *pb, const char *line, int len,
                        int source_row, int hlevel, int screen_cols,
                        int *link_idx)
{
    short cpair;
    switch (hlevel) {
    case 1:  cpair = CP_HEADING1; break;
    case 2:  cpair = CP_HEADING2; break;
    default: cpair = CP_HEADING4; break;
    }

    /* Strip leading and trailing spaces from the paragraph line */
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int content_start = i;
    int content_end = len;
    while (content_end > content_start && line[content_end - 1] == ' ')
        content_end--;
    int clen = content_end - content_start;
    if (clen < 0) clen = 0;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + content_start, clen, A_BOLD, cpair,
                 &ct, &cs, &cl, link_idx);

    PreviewLine *pl = pv_add(pb, source_row, cl);
    int p = pv_copy(pl, 0, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);

    /* H1 gets an underline */
    if (hlevel == 1 && cl > 0) {
        int uw = cl;
        if (uw > screen_cols) uw = screen_cols;
        PreviewLine *ul = pv_add(pb, source_row, uw);
        pv_fill_acs(ul, 0, uw, PM_HLINE, A_BOLD, cpair);
        ul->text[uw] = '\0';
    }
}
