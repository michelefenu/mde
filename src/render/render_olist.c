/* render_olist — Ordered list rendering for preview mode. */
#include "render_olist.h"
#include "render.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

void gen_olist(PreviewBuffer *pb, const char *line, int len,
               int source_row, int body_indent, int *link_idx,
               int display_num)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;

    /* Skip past source digits to find delimiter */
    while (i < len && isdigit((unsigned char)line[i])) i++;
    char delim = '.';
    if (i < len && (line[i] == '.' || line[i] == ')')) {
        delim = line[i];
        i++;
    }
    if (i < len && line[i] == ' ') i++;

    /* Format the display number with the original delimiter */
    char prefix_buf[32];
    int prefix_len = snprintf(prefix_buf, sizeof(prefix_buf), "%d%c ", display_num, delim);

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + i, len - i, 0, CP_DEFAULT, &ct, &cs, &cl, link_idx);

    int total = indent + prefix_len + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);
    for (int j = 0; j < prefix_len; j++) {
        pl->text[p]         = prefix_buf[j];
        pl->styles[p].attr  = A_BOLD;
        pl->styles[p].cpair = CP_LIST_MARKER;
        p++;
    }
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);
}
