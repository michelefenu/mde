/* render_codeblock — Fenced and indented code block rendering for preview. */
#include "render_codeblock.h"
#include "syntax.h"
#include "utf8.h"
#include "xalloc.h"
#include <stdlib.h>
#include <string.h>

/* Return the number of display columns a UTF-8 string occupies. */
static int utf8_display_width(const char *s, int len)
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

/* Generate a boxed code block from a VLine array.
   fence_idx      = index into vlines of the opening ``` line.
   content_start .. content_end-1 = vline indices of actual code content. */
void gen_code_block_v(PreviewBuffer *pb, const VLine *vlines, int n,
                      int fence_idx, int content_start, int content_end,
                      int screen_cols, int body_indent)
{
    /* Extract language label from opening fence */
    const char *fence = vlines[fence_idx].text;
    int flen = vlines[fence_idx].len;
    int fi = 0;
    while (fi < flen && (fence[fi] == ' ' || fence[fi] == '`' || fence[fi] == '~')) fi++;
    int lang_start = fi;
    while (fi < flen && fence[fi] != ' ' && fence[fi] != '`' && fence[fi] != '~') fi++;
    int lang_len = fi - lang_start;

    const SyntaxLang *lang = syntax_find_lang(fence + lang_start, lang_len);

    /* Find max content display width (columns, not bytes) */
    int max_w = 0;
    for (int r = content_start; r < content_end && r < n; r++) {
        int dw = utf8_display_width(vlines[r].text, vlines[r].len);
        if (dw > max_w) max_w = dw;
    }

    /* box_w is in display columns */
    int box_w = max_w;
    if (lang_len + 2 > box_w) box_w = lang_len + 2;
    if (box_w < 20) box_w = 20;
    int max_box = screen_cols - body_indent - 4;
    if (max_box > 0 && box_w > max_box) box_w = max_box;

    /* Border total is in display columns (all single-byte chars) */
    int border_total = body_indent + box_w + 4;

    /* ── Top border: ┌─ language ──────┐ ── */
    {
        PreviewLine *pl = pv_add(pb, vlines[fence_idx].source_row, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_ULCORNER, 0, CP_DIMMED); p++;
        if (lang_len > 0) {
            pv_set_acs(pl, p, PM_HLINE, 0, CP_DIMMED); p++;
            pl->text[p] = ' '; pl->styles[p].cpair = CP_DIMMED; p++;
            for (int j = 0; j < lang_len && p < border_total - 1; j++) {
                pl->text[p] = fence[lang_start + j];
                pl->styles[p].attr  = A_BOLD;
                pl->styles[p].cpair = CP_CODE_BLOCK;
                p++;
            }
            pl->text[p] = ' '; pl->styles[p].cpair = CP_DIMMED; p++;
        }
        while (p < border_total - 1) {
            pv_set_acs(pl, p, PM_HLINE, 0, CP_DIMMED); p++;
        }
        pv_set_acs(pl, p, PM_URCORNER, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }

    /* ── Content lines: │ code padded │ ──
       Each line may have multi-byte UTF-8 chars, so its byte count can
       exceed its display width.  We copy all bytes, then pad with spaces
       to fill box_w display columns. */
    for (int r = content_start; r < content_end && r < n; r++) {
        const char *line = vlines[r].text;
        int         len  = vlines[r].len;
        int         dw   = utf8_display_width(line, len);

        /* Truncate content to box_w display columns so the right border
           always aligns, even when a line is wider than the box. */
        int write_len = len;
        int content_dw = dw;
        if (dw > box_w) {
            write_len  = 0;
            content_dw = 0;
            while (write_len < len) {
                int clen = utf8_char_bytes(line, len, write_len);
                int w    = utf8_char_width(line, len, write_len);
                if (content_dw + w > box_w) break;
                content_dw += w;
                write_len  += clen;
            }
        }

        int pad = box_w - content_dw;
        /* byte length: indent + borders/spaces + content bytes + padding */
        int line_bytes = body_indent + 4 + write_len + pad;

        PreviewLine *pl = pv_add(pb, vlines[r].source_row, line_bytes);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        pl->text[p] = ' '; pl->styles[p].cpair = CP_CODE_BLOCK; p++;
        int text_start = p;
        for (int j = 0; j < write_len; j++) {
            pl->text[p]         = line[j];
            pl->styles[p].cpair = CP_CODE_BLOCK;
            p++;
        }
        /* Apply syntax highlighting to the text portion */
        if (lang)
            syntax_highlight_line(lang, line, write_len,
                                  pl->styles + text_start);
        p = pv_fill(pl, p, pad, ' ', 0, CP_CODE_BLOCK);
        pl->text[p] = ' '; pl->styles[p].cpair = CP_CODE_BLOCK; p++;
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }

    /* If the code block is empty, add one blank content line */
    if (content_start >= content_end) {
        PreviewLine *pl = pv_add(pb, vlines[fence_idx].source_row, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        p = pv_fill(pl, p, box_w + 2, ' ', 0, CP_CODE_BLOCK);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }

    /* ── Bottom border: └──────────────┘ ── */
    {
        int src = (content_end > content_start && content_end - 1 < n)
                ? vlines[content_end - 1].source_row
                : vlines[fence_idx].source_row;
        PreviewLine *pl = pv_add(pb, src, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_LLCORNER, 0, CP_DIMMED); p++;
        while (p < border_total - 1) {
            pv_set_acs(pl, p, PM_HLINE, 0, CP_DIMMED); p++;
        }
        pv_set_acs(pl, p, PM_LRCORNER, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }
}

/* Generate a boxed indented code block (CommonMark 4.4).
   lines/lens/source_rows are the already-stripped content lines
   (4-space or 1-tab prefix removed by the caller). */
void gen_indented_code_block(PreviewBuffer *pb,
                             const char **lines, const int *lens,
                             const int *source_rows, int n,
                             int screen_cols, int body_indent)
{
    /* Find max content display width */
    int max_w = 0;
    for (int r = 0; r < n; r++) {
        int dw = utf8_display_width(lines[r], lens[r]);
        if (dw > max_w) max_w = dw;
    }

    int box_w = max_w;
    if (box_w < 20) box_w = 20;
    int max_box = screen_cols - body_indent - 4;
    if (max_box > 0 && box_w > max_box) box_w = max_box;

    int border_total = body_indent + box_w + 4;
    int top_src = (n > 0) ? source_rows[0] : 0;

    /* ── Top border: ┌──────────────────┐ ── (no language label) */
    {
        PreviewLine *pl = pv_add(pb, top_src, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_ULCORNER, 0, CP_DIMMED); p++;
        while (p < border_total - 1) {
            pv_set_acs(pl, p, PM_HLINE, 0, CP_DIMMED); p++;
        }
        pv_set_acs(pl, p, PM_URCORNER, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }

    /* ── Content lines ── */
    if (n == 0) {
        /* Empty block: one blank content line */
        PreviewLine *pl = pv_add(pb, top_src, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        p = pv_fill(pl, p, box_w + 2, ' ', 0, CP_CODE_BLOCK);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    } else {
        for (int r = 0; r < n; r++) {
            const char *line = lines[r];
            int         len  = lens[r];
            int         dw   = utf8_display_width(line, len);

            int write_len = len;
            int content_dw = dw;
            if (dw > box_w) {
                write_len  = 0;
                content_dw = 0;
                while (write_len < len) {
                    int clen = utf8_char_bytes(line, len, write_len);
                    int w    = utf8_char_width(line, len, write_len);
                    if (content_dw + w > box_w) break;
                    content_dw += w;
                    write_len  += clen;
                }
            }

            int pad = box_w - content_dw;
            int line_bytes = body_indent + 4 + write_len + pad;

            PreviewLine *pl = pv_add(pb, source_rows[r], line_bytes);
            int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
            pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
            pl->text[p] = ' '; pl->styles[p].cpair = CP_CODE_BLOCK; p++;
            for (int j = 0; j < write_len; j++) {
                pl->text[p]         = line[j];
                pl->styles[p].cpair = CP_CODE_BLOCK;
                p++;
            }
            p = pv_fill(pl, p, pad, ' ', 0, CP_CODE_BLOCK);
            pl->text[p] = ' '; pl->styles[p].cpair = CP_CODE_BLOCK; p++;
            pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
            pl->len = p; pl->text[p] = '\0';
        }
    }

    /* ── Bottom border: └──────────────┘ ── */
    {
        int bot_src = (n > 0) ? source_rows[n - 1] : top_src;
        PreviewLine *pl = pv_add(pb, bot_src, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_LLCORNER, 0, CP_DIMMED); p++;
        while (p < border_total - 1) {
            pv_set_acs(pl, p, PM_HLINE, 0, CP_DIMMED); p++;
        }
        pv_set_acs(pl, p, PM_LRCORNER, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }
}
