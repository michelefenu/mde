/* render_ulist — Unordered list rendering for preview mode. */
#include "render_ulist.h"
#include "render.h"
#include <stdlib.h>

void gen_ulist(PreviewBuffer *pb, const char *line, int len,
               int source_row, int body_indent, int *link_idx)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;
    i++;                                       /* skip marker */
    if (i < len && line[i] == ' ') i++;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + i, len - i, 0, CP_DEFAULT, &ct, &cs, &cl, link_idx);

    int total = indent + 2 + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);
    pv_set_acs(pl, p, PM_BULLET, A_BOLD, CP_LIST_MARKER);
    p++;
    pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);
}
