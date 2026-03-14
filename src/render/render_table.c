/* render_table — Parse markdown tables, compute column widths, draw ACS borders. */
#include "render_table.h"
#include "utf8.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int cell_display_width(const char *s, int len)
{
    int cols = 0;
    for (int i = 0; i < len; ) {
        int clen = utf8_clen((unsigned char)s[i]);
        if (i + clen > len) clen = len - i;
        cols += utf8_char_width(s, len, i);
        i += clen;
    }
    return cols;
}

#define MAX_TBL_COLS 32

typedef struct {
    char      *cells[MAX_TBL_COLS];       /* raw cell text (owned) */
    int        cell_lens[MAX_TBL_COLS];
    char      *stripped[MAX_TBL_COLS];    /* stripped text (owned) — NULL for sep rows */
    CharStyle *cell_styles[MAX_TBL_COLS]; /* per-char styles for stripped text */
    int        strip_lens[MAX_TBL_COLS];  /* byte length of stripped text */
    int        strip_dws[MAX_TBL_COLS];   /* display-column width of stripped text */
    int        num_cells;
    int        is_sep;
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
    for (int c = 0; c < r->num_cells; c++) {
        free(r->cells[c]);
        free(r->stripped[c]);       /* NULL-safe */
        free(r->cell_styles[c]);    /* NULL-safe */
    }
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
                              int ncols, int source_row, int body_indent)
{
    int total = body_indent + 1;
    for (int c = 0; c < ncols; c++) {
        int sl = (c < row->num_cells) ? row->strip_lens[c] : 0;
        int dw = (c < row->num_cells) ? row->strip_dws[c]  : 0;
        /* bytes written = sl (stripped bytes) + (cw[c]-dw) spaces + 2 pads + 1 vline */
        total += sl + (cw[c] - dw) + 2 + 1;
    }

    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);

    pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
    for (int c = 0; c < ncols; c++) {
        pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
        int sl = (c < row->num_cells) ? row->strip_lens[c]  : 0;
        int dw = (c < row->num_cells) ? row->strip_dws[c]   : 0;
        const char      *st = (c < row->num_cells) ? row->stripped[c]    : NULL;
        const CharStyle *ss = (c < row->num_cells) ? row->cell_styles[c] : NULL;
        if (sl > 0 && st && ss)
            p = pv_copy(pl, p, st, ss, sl);
        p = pv_fill(pl, p, cw[c] - dw, ' ', 0, CP_DEFAULT);
        pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
    }
    pl->len = p; pl->text[p] = '\0';
}

void gen_table_block(PreviewBuffer *pb, Buffer *buf,
                     int start, int end, int screen_cols,
                     int body_indent, int *link_idx)
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

    /* Find first separator to distinguish header rows from body rows */
    int first_sep = -1;
    for (int i = 0; i < nr && first_sep < 0; i++)
        if (rows[i].is_sep) first_sep = i;

    /* Pre-strip inline markdown in each non-separator cell */
    for (int i = 0; i < nr; i++) {
        if (rows[i].is_sep) continue;
        int   is_hdr = (first_sep > 0 && i < first_sep);
        attr_t  ba   = is_hdr ? A_BOLD : 0;
        short   bc   = is_hdr ? CP_HEADING2 : CP_DEFAULT;
        for (int c = 0; c < rows[i].num_cells; c++) {
            strip_inline(rows[i].cells[c], rows[i].cell_lens[c],
                         ba, bc,
                         &rows[i].stripped[c],
                         &rows[i].cell_styles[c],
                         &rows[i].strip_lens[c], link_idx);
            rows[i].strip_dws[c] =
                cell_display_width(rows[i].stripped[c], rows[i].strip_lens[c]);
        }
    }

    int cw[MAX_TBL_COLS];
    memset(cw, 0, sizeof(cw));
    for (int i = 0; i < nr; i++) {
        if (rows[i].is_sep) continue;
        for (int c = 0; c < rows[i].num_cells; c++) {
            if (rows[i].strip_dws[c] > cw[c])
                cw[c] = rows[i].strip_dws[c];
        }
    }
    for (int c = 0; c < max_cols; c++)
        if (cw[c] < 3) cw[c] = 3;

    (void)screen_cols;

    gen_table_border(pb, cw, max_cols, 0, start, body_indent);

    for (int i = 0; i < nr; i++) {
        if (rows[i].is_sep)
            gen_table_border(pb, cw, max_cols, 1, start + i, body_indent);
        else
            gen_table_content(pb, &rows[i], cw, max_cols,
                              start + i, body_indent);
    }

    gen_table_border(pb, cw, max_cols, 2, end - 1, body_indent);

    for (int i = 0; i < nr; i++) free_trow(&rows[i]);
    free(rows);
}
