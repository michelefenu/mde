#include "toc.h"
#include "preview_ui.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  TOC buffer construction
 * ================================================================ */

static void toc_generate(PreviewBuffer *pb, Buffer *buf)
{
    preview_free(pb);

    int found = 0;
    int in_code = 0;

    for (int r = 0; r < buf->num_lines; r++) {
        const char *line = buffer_line_data(buf, r);
        int len = buffer_line_len(buf, r);

        if (render_is_code_fence(line)) {
            in_code = !in_code;
            continue;
        }
        if (in_code) continue;

        int level = render_heading_level(line);
        if (level == 0) continue;

        found = 1;

        /* Skip leading spaces and '#' chars and the mandatory space */
        const char *p = line;
        while (*p == ' ') p++;
        while (*p == '#') p++;
        if (*p == ' ') p++;

        int text_len = (int)(line + len - p);
        if (text_len < 0) text_len = 0;

        int indent = (level - 1) * 2;
        int pl_len = indent + text_len;
        if (pl_len < 1) pl_len = 1;

        PreviewLine *pl = pv_add(pb, r, pl_len);
        if (!pl) continue;

        /* Choose heading color pair */
        short cpair;
        switch (level) {
        case 1:  cpair = CP_HEADING1; break;
        case 2:  cpair = CP_HEADING2; break;
        case 3:  cpair = CP_HEADING3; break;
        default: cpair = CP_HEADING4; break;
        }

        attr_t attr = (level <= 2) ? A_BOLD : A_NORMAL;

        /* Fill indent spaces */
        if (indent > 0)
            pv_fill(pl, 0, indent, ' ', A_NORMAL, CP_DEFAULT);

        /* Copy heading text */
        for (int i = 0; i < text_len; i++) {
            CharStyle cs;
            cs.attr  = attr;
            cs.cpair = cpair;
            cs.acs   = 0;
            pv_copy(pl, indent + i, p + i, &cs, 1);
        }
    }

    if (!found) {
        /* No headings: insert a placeholder */
        const char *msg = "(no headings found)";
        int mlen = (int)strlen(msg);
        PreviewLine *pl = pv_add(pb, -1, mlen);
        if (pl) {
            for (int i = 0; i < mlen; i++) {
                CharStyle cs;
                cs.attr  = A_DIM;
                cs.cpair = CP_DIMMED;
                cs.acs   = 0;
                pv_copy(pl, i, msg + i, &cs, 1);
            }
        }
    }
}

/* ================================================================
 *  Scroll clamping
 * ================================================================ */

void toc_clamp_scroll(Editor *ed)
{
    int max_s = ed->toc_buf.num_lines - ed->screen_rows;
    if (max_s < 0) max_s = 0;
    if (ed->toc_scroll_y > max_s) ed->toc_scroll_y = max_s;
    if (ed->toc_scroll_y < 0)     ed->toc_scroll_y = 0;

    /* Keep selected entry visible */
    if (ed->toc_selected < ed->toc_scroll_y)
        ed->toc_scroll_y = ed->toc_selected;
    if (ed->toc_selected >= ed->toc_scroll_y + ed->screen_rows)
        ed->toc_scroll_y = ed->toc_selected - ed->screen_rows + 1;

    /* Re-clamp after selection adjustment */
    if (ed->toc_scroll_y > max_s) ed->toc_scroll_y = max_s;
    if (ed->toc_scroll_y < 0)     ed->toc_scroll_y = 0;
}

/* ================================================================
 *  Show / close
 * ================================================================ */

void editor_show_toc(Editor *ed)
{
    ed->toc_was_preview = ed->preview_mode;

    getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
    ed->screen_rows -= 2;

    toc_generate(&ed->toc_buf, &ed->buf);

    /* Determine the current document position to seed the selection.
       In preview mode use the source_row of the topmost visible preview line. */
    int cur_row = ed->cy;
    if (ed->toc_was_preview && ed->preview_buf.num_lines > 0) {
        int pi = ed->preview_scroll_y;
        if (pi >= ed->preview_buf.num_lines)
            pi = ed->preview_buf.num_lines - 1;
        cur_row = ed->preview_buf.lines[pi].source_row;
    }

    /* Find the TOC entry nearest cur_row */
    ed->toc_selected = 0;
    for (int i = 0; i < ed->toc_buf.num_lines; i++) {
        if (ed->toc_buf.lines[i].source_row <= cur_row)
            ed->toc_selected = i;
        else
            break;
    }
    if (ed->toc_selected >= ed->toc_buf.num_lines)
        ed->toc_selected = ed->toc_buf.num_lines > 0
                           ? ed->toc_buf.num_lines - 1 : 0;

    ed->toc_scroll_y = 0;
    toc_clamp_scroll(ed);

    ed->toc_mode = 1;
    curs_set(0);
    editor_set_status(ed, "TOC — Up/Down navigate, Enter jump, Esc close");
}

void editor_close_toc(Editor *ed)
{
    preview_free(&ed->toc_buf);
    ed->toc_mode = 0;

    if (ed->toc_was_preview) {
        curs_set(0);
        editor_set_status(ed, "Preview mode — Ctrl+P to edit");
    } else {
        curs_set(1);
        editor_set_status(ed, "");
    }
}

/* ================================================================
 *  Key handler
 * ================================================================ */

void editor_toc_process_key(Editor *ed, int c)
{
    int n = ed->toc_buf.num_lines;

    switch (c) {
    case 27:   /* Escape */
        editor_close_toc(ed);
        break;

    case CTRL_KEY('q'):
        ed->quit = 1;
        break;

    case KEY_UP:
        if (ed->toc_selected > 0) {
            ed->toc_selected--;
            toc_clamp_scroll(ed);
        }
        break;

    case KEY_DOWN:
        if (ed->toc_selected < n - 1) {
            ed->toc_selected++;
            toc_clamp_scroll(ed);
        }
        break;

    case '\r':
    case '\n':
    case KEY_ENTER: {
        if (n > 0) {
            int src = ed->toc_buf.lines[ed->toc_selected].source_row;
            if (src >= 0) {
                if (ed->toc_was_preview) {
                    /* Scroll preview to the selected heading */
                    ed->preview_scroll_y =
                        preview_find_line(&ed->preview_buf, src);
                    clamp_preview_scroll(ed);
                } else {
                    /* Jump edit cursor and put heading at top of screen */
                    ed->cy = src;
                    ed->cx = 0;
                    ed->scroll_y = src;   /* editor_scroll will clamp if needed */
                }
            }
        }
        editor_close_toc(ed);
        break;
    }

    case KEY_RESIZE:
        getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
        ed->screen_rows -= 2;
        toc_generate(&ed->toc_buf, &ed->buf);
        if (ed->toc_selected >= ed->toc_buf.num_lines)
            ed->toc_selected = ed->toc_buf.num_lines > 0
                               ? ed->toc_buf.num_lines - 1 : 0;
        toc_clamp_scroll(ed);
        break;

    default:
        break;
    }
}
