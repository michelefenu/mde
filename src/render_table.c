#include "render_table.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TBL_COLS 32

typedef struct {
    char *cells[MAX_TBL_COLS];
    int   cell_lens[MAX_TBL_COLS];
    int   num_cells;
    int   is_sep;
} TRow;

int is_table_line(const char *line, int len)
{
    int pipes = 0;
    for (int i = 0; i < len; i++)
        if (line[i] == '|') pipes++;
    return pipes >= 1;
}

static void parse_table_row(const char *line, int len, TRow *row)
{
    memset(row, 0, sizeof(*row));
    row->is_sep = 1;

    int i = 0;
    while (i < len && line[i] == ' ') i++;
    if (i < len && line[i] == '|') i++;

    while (i < len && row->num_cells < MAX_TBL_COLS) {
        int cs = i;
        while (i < len && line[i] != '|') i++;
        int ce = i;

        while (cs < ce && line[cs] == ' ') cs++;
        while (ce > cs && line[ce - 1] == ' ') ce--;
        int clen = (ce > cs) ? ce - cs : 0;

        row->cells[row->num_cells]     = strndup(line + cs, clen);
        row->cell_lens[row->num_cells] = clen;

        int sep = (clen > 0);
        for (int j = cs; j < ce; j++)
            if (line[j] != '-' && line[j] != ':' && line[j] != ' ')
                { sep = 0; break; }
        if (clen == 0) sep = 0;
        if (!sep) row->is_sep = 0;

        row->num_cells++;
        if (i < len && line[i] == '|') i++;
        else break;
    }
    if (row->num_cells == 0) row->is_sep = 0;
}

static void free_trow(TRow *r)
{
    for (int c = 0; c < r->num_cells; c++) free(r->cells[c]);
}

/* kind: 0 = top  ┌─┬─┐   1 = mid  ├─┼─┤   2 = bot  └─┴─┘ */
static void gen_table_border(PreviewBuffer *pb, int *cw, int ncols,
                             int kind, int source_row, int body_indent)
{
    int total = body_indent + 1;
    for (int c = 0; c < ncols; c++) total += cw[c] + 2 + 1;

    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
    unsigned char lc, mc, rc;
    switch (kind) {
    case 0: lc = PM_ULCORNER; mc = PM_TTEE; rc = PM_URCORNER; break;
    case 1: lc = PM_LTEE;     mc = PM_PLUS; rc = PM_RTEE;     break;
    default:lc = PM_LLCORNER; mc = PM_BTEE; rc = PM_LRCORNER; break;
    }

    pv_set_acs(pl, p, lc, 0, CP_DIMMED); p++;
    for (int c = 0; c < ncols; c++) {
        p = pv_fill_acs(pl, p, cw[c] + 2, PM_HLINE, 0, CP_DIMMED);
        unsigned char sep = (c < ncols - 1) ? mc : rc;
        pv_set_acs(pl, p, sep, 0, CP_DIMMED); p++;
    }
    pl->len = p; pl->text[p] = '\0';
}

static void gen_table_content(PreviewBuffer *pb, TRow *row, int *cw,
                              int ncols, int is_hdr, int source_row,
                              int body_indent)
{
    int total = body_indent + 1;
    for (int c = 0; c < ncols; c++) total += cw[c] + 2 + 1;

    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
    attr_t ca = is_hdr ? A_BOLD : 0;
    short  cc = is_hdr ? CP_HEADING2 : CP_DEFAULT;

    pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
    for (int c = 0; c < ncols; c++) {
        pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
        int cl   = (c < row->num_cells) ? row->cell_lens[c] : 0;
        const char *ct = (c < row->num_cells) ? row->cells[c] : "";
        for (int j = 0; j < cw[c]; j++) {
            pl->text[p]         = (j < cl) ? ct[j] : ' ';
            pl->styles[p].attr  = ca;
            pl->styles[p].cpair = cc;
            p++;
        }
        pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
    }
    pl->len = p; pl->text[p] = '\0';
}

void gen_table_block(PreviewBuffer *pb, Buffer *buf,
                     int start, int end, int screen_cols,
                     int body_indent)
{
    int nr = end - start;
    TRow *rows = calloc(nr, sizeof(TRow));
    int max_cols = 0;

    for (int i = 0; i < nr; i++) {
        parse_table_row(buffer_line_data(buf, start + i),
                        buffer_line_len(buf, start + i), &rows[i]);
        if (rows[i].num_cells > max_cols)
            max_cols = rows[i].num_cells;
    }

    int cw[MAX_TBL_COLS];
    memset(cw, 0, sizeof(cw));
    for (int i = 0; i < nr; i++) {
        if (rows[i].is_sep) continue;
        for (int c = 0; c < rows[i].num_cells; c++)
            if (rows[i].cell_lens[c] > cw[c])
                cw[c] = rows[i].cell_lens[c];
    }
    for (int c = 0; c < max_cols; c++)
        if (cw[c] < 3) cw[c] = 3;

    (void)screen_cols;

    gen_table_border(pb, cw, max_cols, 0, start, body_indent);

    int found_sep = 0;
    for (int i = 0; i < nr; i++) {
        if (rows[i].is_sep) {
            gen_table_border(pb, cw, max_cols, 1, start + i, body_indent);
            found_sep = 1;
        } else {
            gen_table_content(pb, &rows[i], cw, max_cols,
                              !found_sep, start + i, body_indent);
        }
    }

    gen_table_border(pb, cw, max_cols, 2, end - 1, body_indent);

    for (int i = 0; i < nr; i++) free_trow(&rows[i]);
    free(rows);
}
