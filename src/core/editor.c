/* editor — Editor event loop, key dispatch, file I/O, prompt dialogs. */
#include "editor.h"
#include "render.h"
#include "render_table.h"
#include "utf8.h"
#include "search.h"
#include "help.h"
#include "preview_ui.h"
#include "toc.h"
#include "command.h"
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <wchar.h>

/* Sentinel returned by editor_read_key() for non-ASCII characters.
   The actual UTF-8 bytes are in the output buffer. */
#define KEY_UTF8 (-2)

/* ================================================================
 *  Portable key reading  (uses get_wch on wide ncurses, getch otherwise)
 * ================================================================ */

/* Read one key event.
   - ASCII chars and KEY_* constants are returned directly.
   - Non-ASCII Unicode characters: fills utf8_buf, sets *utf8_len,
     and returns KEY_UTF8 as a sentinel. */
static int editor_read_key(char utf8_buf[4], int *utf8_len)
{
    *utf8_len = 0;

#if NCURSES_WIDECHAR
    wint_t wc;
    int ret = get_wch(&wc);
    if (ret == ERR) return ERR;
    if (ret == KEY_CODE_YES) return (int)wc;   /* function key */
    if ((int)wc < 0x80) return (int)wc;        /* ASCII */
    /* Non-ASCII: encode as UTF-8 */
    *utf8_len = wchar_to_utf8((unsigned long)wc, utf8_buf);
    return KEY_UTF8;
#else
    int c = getch();
    if (c == ERR || c < 0x80 || c >= 0x100) return c;
    /* Raw high byte — assemble UTF-8 sequence */
    utf8_buf[0] = (char)c;
    unsigned char uc = (unsigned char)c;
    int expected = 1;
    if      (uc >= 0xF0) expected = 4;
    else if (uc >= 0xE0) expected = 3;
    else if (uc >= 0xC0) expected = 2;
    for (int i = 1; i < expected; i++) {
        int next = getch();
        if (next == ERR || (next & 0xC0) != 0x80) {
            if (next != ERR) ungetch(next);
            *utf8_len = i;
            return KEY_UTF8;
        }
        utf8_buf[i] = (char)next;
    }
    *utf8_len = expected;
    return KEY_UTF8;
#endif
}

/* ================================================================
 *  Status message
 * ================================================================ */

void editor_set_status(Editor *ed, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ed->statusmsg, sizeof(ed->statusmsg), fmt, ap);
    va_end(ap);
    ed->statusmsg_time = time(NULL);
}

/* ================================================================
 *  Prompt  (used for Save-As, Search, Go-to-line)
 * ================================================================ */

char *editor_prompt(Editor *ed, const char *prompt_str,
                    PromptCallback cb, TabCompleteFunc tab_fn)
{
    char buf[256];
    int  buflen = 0;
    buf[0] = '\0';

    for (;;) {
        editor_set_status(ed, "%s%s", prompt_str, buf);
        editor_refresh_screen(ed);

        char utf8[4];
        int  utf8_len;
        int  c = editor_read_key(utf8, &utf8_len);

        if (c == KEY_RESIZE) {
            getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
            ed->screen_rows -= 2;
            continue;
        }

        if (c == 27) {                                    /* Escape */
            editor_set_status(ed, "");
            if (cb) cb(ed, buf, c);
            return NULL;
        }

        if (c == '\r' || c == '\n' || c == KEY_ENTER) {
            if (buflen > 0) {
                editor_set_status(ed, "");
                if (cb) cb(ed, buf, c);
                return strdup(buf);
            }
            continue;
        }

        if (c == KEY_BACKSPACE || c == 127 || c == CTRL_KEY('h')) {
            if (buflen > 0) buf[--buflen] = '\0';
        } else if (c == '\t' && tab_fn) {
            buflen = tab_fn(buf, buflen, (int)sizeof(buf) - 1);
            buf[buflen] = '\0';
        } else if (c == KEY_UTF8 && buflen < (int)sizeof(buf) - 5) {
            for (int i = 0; i < utf8_len && buflen < (int)sizeof(buf) - 1; i++)
                buf[buflen++] = utf8[i];
            buf[buflen] = '\0';
        } else if (c >= 32 && c < 127 && buflen < (int)sizeof(buf) - 1) {
            buf[buflen++] = (char)c;
            buf[buflen]   = '\0';
        }

        if (cb) cb(ed, buf, c);
    }
}

/* ================================================================
 *  Scrolling
 * ================================================================ */

static void editor_scroll(Editor *ed)
{
    if (ed->word_wrap) {
        /* Wrapping replaces horizontal scrolling */
        ed->scroll_x = 0;

        /* Ensure cursor line is above the viewport start */
        if (ed->cy < ed->scroll_y)
            ed->scroll_y = ed->cy;

        /* Compute visual row of the cursor relative to scroll_y.
           Sum wrap heights of lines from scroll_y to cy-1,
           then add the cursor's sub-line within cy. */
        for (;;) {
            int vis_y = 0;
            /* Pre-scan code fence state up to scroll_y */
            int sc_in_code = 0;
            for (int i = 0; i < ed->scroll_y && i < ed->buf.num_lines; i++) {
                if (render_is_code_fence(buffer_line_data(&ed->buf, i)))
                    sc_in_code = !sc_in_code;
            }
            int sc_code = sc_in_code;
            for (int r = ed->scroll_y; r < ed->cy; r++) {
                const char *rd = buffer_line_data(&ed->buf, r);
                int rl = buffer_line_len(&ed->buf, r);
                BlockType rbt = render_get_block_type(rd, sc_code);
                if (rbt == BLOCK_CODE_FENCE) sc_code = !sc_code;
                if (rbt == BLOCK_PARAGRAPH && is_table_line(rd, rl)) {
                    vis_y += 1;
                } else {
                    int ci = render_line_content_indent(rd, rl, rbt);
                    vis_y += render_wrap_height(rd, rl, ed->screen_cols, ci);
                }
            }
            /* Add cursor's sub-line within its own line */
            const char *cl = buffer_line_data(&ed->buf, ed->cy);
            int cl_len = buffer_line_len(&ed->buf, ed->cy);
            BlockType cy_bt = render_get_block_type(cl, sc_code);
            int sub_row, sub_col;
            if (cy_bt == BLOCK_PARAGRAPH && is_table_line(cl, cl_len)) {
                sub_row = 0;
                sub_col = render_byte_to_col(cl, cl_len, ed->cx);
            } else {
                int cy_ci = render_line_content_indent(cl, cl_len, cy_bt);
                render_wrap_cursor_pos(cl, cl_len, ed->screen_cols, cy_ci,
                                       ed->cx, &sub_row, &sub_col);
            }
            vis_y += sub_row;

            if (vis_y >= ed->screen_rows) {
                ed->scroll_y++;
                if (ed->scroll_y >= ed->buf.num_lines) {
                    ed->scroll_y = ed->buf.num_lines > 0 ? ed->buf.num_lines - 1 : 0;
                    break;
                }
            } else {
                break;
            }
        }
        if (ed->scroll_y >= ed->buf.num_lines)
            ed->scroll_y = ed->buf.num_lines > 0 ? ed->buf.num_lines - 1 : 0;
    } else {
        if (ed->cy < ed->scroll_y)
            ed->scroll_y = ed->cy;
        if (ed->cy >= ed->scroll_y + ed->screen_rows)
            ed->scroll_y = ed->cy - ed->screen_rows + 1;

        if (ed->cx < ed->scroll_x)
            ed->scroll_x = ed->cx;
        if (ed->cx >= ed->scroll_x + ed->screen_cols)
            ed->scroll_x = ed->cx - ed->screen_cols + 1;
    }
}

/* ================================================================
 *  Cursor movement
 * ================================================================ */

static void editor_move_cursor(Editor *ed, int key)
{
    switch (key) {
    case KEY_LEFT:
        if (ed->cx > 0) {
            const char *line = buffer_line_data(&ed->buf, ed->cy);
            ed->cx = utf8_prev_char(line, ed->cx);
        } else if (ed->cy > 0) {
            ed->cy--;
            ed->cx = buffer_line_len(&ed->buf, ed->cy);
        }
        break;
    case KEY_RIGHT:
        {
            int ll = buffer_line_len(&ed->buf, ed->cy);
            if (ed->cx < ll) {
                const char *line = buffer_line_data(&ed->buf, ed->cy);
                ed->cx += utf8_char_bytes(line, ll, ed->cx);
            } else if (ed->cy < ed->buf.num_lines - 1) {
                ed->cy++;
                ed->cx = 0;
            }
        }
        break;
    case KEY_SR:    /* Shift+Up: move 10 lines up */
        ed->cy -= 10;
        if (ed->cy < 0) ed->cy = 0;
        break;
    case KEY_SF:    /* Shift+Down: move 10 lines down */
        ed->cy += 10;
        if (ed->cy >= ed->buf.num_lines) ed->cy = ed->buf.num_lines - 1;
        break;
    case KEY_UP:
        if (ed->cy > 0) ed->cy--;
        break;
    case KEY_DOWN:
        if (ed->cy < ed->buf.num_lines - 1) ed->cy++;
        break;
    case KEY_PPAGE:
        ed->cy -= ed->screen_rows;
        if (ed->cy < 0) ed->cy = 0;
        break;
    case KEY_NPAGE:
        ed->cy += ed->screen_rows;
        if (ed->cy >= ed->buf.num_lines) ed->cy = ed->buf.num_lines - 1;
        break;
    case KEY_HOME:
        ed->cx = 0;
        break;
    case KEY_END:
        ed->cx = buffer_line_len(&ed->buf, ed->cy);
        break;
    }

    /* Clamp cx to current line length */
    int ll = buffer_line_len(&ed->buf, ed->cy);
    if (ed->cx > ll) ed->cx = ll;
}

/* ================================================================
 *  Text editing
 * ================================================================ */

static void editor_insert_char(Editor *ed, int c)
{
    char ch = (char)c;
    undo_push(&ed->undo, UNDO_INSERT, ed->cy, ed->cx,
              &ch, 1, ed->cx, ed->cy, ed->undo_seq);
    undo_stack_clear(&ed->redo);
    buffer_insert_char(&ed->buf, ed->cy, ed->cx, c);
    ed->cx++;
    ed->dirty++;
}

static int list_next_prefix(const char *line, char *out, int out_size)
{
    int i = 0;
    while (line[i] == ' ') i++;
    int indent = i;

    /* Unordered: - item, * item, + item */
    if ((line[i] == '-' || line[i] == '*' || line[i] == '+') && line[i + 1] == ' ') {
        int plen = indent + 2;
        if (plen >= out_size) return 0;
        memcpy(out, line, indent);
        out[indent]     = line[i];
        out[indent + 1] = ' ';
        return plen;
    }

    /* Ordered: 1. item, 1) item */
    if (isdigit((unsigned char)line[i])) {
        int j = i, num = 0;
        while (isdigit((unsigned char)line[j])) { num = num * 10 + (line[j] - '0'); j++; }
        char delim = line[j];
        if ((delim == '.' || delim == ')') && line[j + 1] == ' ') {
            char numbuf[24];
            int nlen = snprintf(numbuf, sizeof(numbuf), "%d%c ", num + 1, delim);
            int plen = indent + nlen;
            if (plen >= out_size) return 0;
            memcpy(out, line, indent);
            memcpy(out + indent, numbuf, nlen);
            return plen;
        }
    }

    return 0;
}

void editor_insert_newline(Editor *ed)
{
    const char *cur = buffer_line_data(&ed->buf, ed->cy);
    char prefix[64];
    int plen = list_next_prefix(cur, prefix, sizeof(prefix));

    /* Empty list item: exit list mode by deleting the prefix, no newline */
    if (plen > 0 && buffer_line_len(&ed->buf, ed->cy) <= plen) {
        int cur_plen = buffer_line_len(&ed->buf, ed->cy);
        const char *line = buffer_line_data(&ed->buf, ed->cy);
        undo_push(&ed->undo, UNDO_DELETE, ed->cy, 0,
                  line, cur_plen, ed->cx, ed->cy, ed->undo_seq);
        undo_stack_clear(&ed->redo);
        for (int i = 0; i < cur_plen; i++)
            buffer_delete_forward(&ed->buf, ed->cy, 0);
        ed->cx = 0;
        ed->dirty++;
        return;
    }

    undo_push(&ed->undo, UNDO_SPLIT, ed->cy, ed->cx,
              NULL, 0, ed->cx, ed->cy, ed->undo_seq);
    undo_stack_clear(&ed->redo);
    buffer_insert_newline(&ed->buf, ed->cy, ed->cx);
    ed->cy++;
    ed->cx = 0;
    ed->dirty++;

    /* Insert list prefix on new line (same undo_seq = undoes with the newline) */
    for (int i = 0; i < plen; i++) {
        char ch = prefix[i];
        undo_push(&ed->undo, UNDO_INSERT, ed->cy, ed->cx,
                  &ch, 1, ed->cx, ed->cy, ed->undo_seq);
        buffer_insert_char(&ed->buf, ed->cy, ed->cx, (unsigned char)ch);
        ed->cx++;
    }
}

static void editor_delete_backward(Editor *ed)
{
    if (ed->cx == 0 && ed->cy == 0) return;

    if (ed->cx > 0) {
        const char *line = buffer_line_data(&ed->buf, ed->cy);
        int prev = utf8_prev_char(line, ed->cx);
        int count = ed->cx - prev;
        undo_push(&ed->undo, UNDO_DELETE, ed->cy, prev,
                  line + prev, count, ed->cx, ed->cy, ed->undo_seq);
        undo_stack_clear(&ed->redo);
        for (int i = 0; i < count; i++)
            buffer_delete_forward(&ed->buf, ed->cy, prev);
        ed->cx = prev;
    } else {
        int prev_len = buffer_line_len(&ed->buf, ed->cy - 1);
        undo_push(&ed->undo, UNDO_JOIN, ed->cy - 1, prev_len,
                  NULL, 0, ed->cx, ed->cy, ed->undo_seq);
        undo_stack_clear(&ed->redo);
        buffer_delete_char(&ed->buf, ed->cy, 0);
        ed->cy--;
        ed->cx = prev_len;
    }
    ed->dirty++;
}

static void editor_delete_forward(Editor *ed)
{
    int ll = buffer_line_len(&ed->buf, ed->cy);
    if (ed->cx >= ll && ed->cy >= ed->buf.num_lines - 1) return;

    if (ed->cx < ll) {
        const char *line = buffer_line_data(&ed->buf, ed->cy);
        int count = utf8_char_bytes(line, ll, ed->cx);
        undo_push(&ed->undo, UNDO_DELETE, ed->cy, ed->cx,
                  line + ed->cx, count, ed->cx, ed->cy, ed->undo_seq);
        undo_stack_clear(&ed->redo);
        for (int i = 0; i < count; i++)
            buffer_delete_forward(&ed->buf, ed->cy, ed->cx);
    } else {
        undo_push(&ed->undo, UNDO_JOIN, ed->cy, ed->cx,
                  NULL, 0, ed->cx, ed->cy, ed->undo_seq);
        undo_stack_clear(&ed->redo);
        buffer_delete_forward(&ed->buf, ed->cy, ed->cx);
    }
    ed->dirty++;
}

static void editor_delete_to_eol(Editor *ed)
{
    int ll = buffer_line_len(&ed->buf, ed->cy);
    if (ed->cx >= ll) return;
    const char *line = buffer_line_data(&ed->buf, ed->cy);
    undo_push(&ed->undo, UNDO_DELETE, ed->cy, ed->cx,
              line + ed->cx, ll - ed->cx, ed->cx, ed->cy, ed->undo_seq);
    undo_stack_clear(&ed->redo);
    buffer_truncate_line(&ed->buf, ed->cy, ed->cx);
    ed->dirty++;
}

/* ================================================================
 *  Undo / Redo
 * ================================================================ */

static void undo_apply_reverse(Editor *ed, UndoEntry *e)
{
    switch (e->type) {
    case UNDO_INSERT:
        for (int i = 0; i < e->data_len; i++)
            buffer_delete_forward(&ed->buf, e->row, e->col);
        break;
    case UNDO_DELETE:
        for (int i = 0; i < e->data_len; i++)
            buffer_insert_char(&ed->buf, e->row, e->col + i,
                               (unsigned char)e->data[i]);
        break;
    case UNDO_SPLIT:
        buffer_delete_char(&ed->buf, e->row + 1, 0);
        break;
    case UNDO_JOIN:
        buffer_insert_newline(&ed->buf, e->row, e->col);
        break;
    }
    ed->cx = e->old_cx;
    ed->cy = e->old_cy;
}

static void undo_apply_forward(Editor *ed, UndoEntry *e)
{
    switch (e->type) {
    case UNDO_INSERT:
        for (int i = 0; i < e->data_len; i++)
            buffer_insert_char(&ed->buf, e->row, e->col + i,
                               (unsigned char)e->data[i]);
        ed->cy = e->row;
        ed->cx = e->col + e->data_len;
        break;
    case UNDO_DELETE:
        for (int i = 0; i < e->data_len; i++)
            buffer_delete_forward(&ed->buf, e->row, e->col);
        ed->cy = e->row;
        ed->cx = e->col;
        break;
    case UNDO_SPLIT:
        buffer_insert_newline(&ed->buf, e->row, e->col);
        ed->cy = e->row + 1;
        ed->cx = 0;
        break;
    case UNDO_JOIN:
        buffer_delete_char(&ed->buf, e->row + 1, 0);
        ed->cy = e->row;
        ed->cx = e->col;
        break;
    }
}

void editor_undo(Editor *ed)
{
    if (undo_empty(&ed->undo)) {
        editor_set_status(ed, "Nothing to undo");
        return;
    }

    int seq = undo_top_seq(&ed->undo);
    int count = 0;

    while (!undo_empty(&ed->undo) && undo_top_seq(&ed->undo) == seq) {
        UndoEntry *e = undo_pop(&ed->undo);
        undo_apply_reverse(ed, e);
        undo_push(&ed->redo, e->type, e->row, e->col,
                  e->data, e->data_len, e->old_cx, e->old_cy, e->seq);
        free(e->data);
        e->data = NULL;
        ed->dirty--;
        count++;
    }

    editor_set_status(ed, "Undo (%d action%s)", count, count > 1 ? "s" : "");
}

void editor_redo(Editor *ed)
{
    if (undo_empty(&ed->redo)) {
        editor_set_status(ed, "Nothing to redo");
        return;
    }

    int seq = undo_top_seq(&ed->redo);
    int count = 0;

    while (!undo_empty(&ed->redo) && undo_top_seq(&ed->redo) == seq) {
        UndoEntry *e = undo_pop(&ed->redo);
        undo_apply_forward(ed, e);
        undo_push(&ed->undo, e->type, e->row, e->col,
                  e->data, e->data_len, e->old_cx, e->old_cy, e->seq);
        free(e->data);
        e->data = NULL;
        ed->dirty++;
        count++;
    }

    editor_set_status(ed, "Redo (%d action%s)", count, count > 1 ? "s" : "");
}

/* ================================================================
 *  File I/O
 * ================================================================ */

void editor_open(Editor *ed, const char *filename)
{
    free(ed->filename);
    ed->filename = strdup(filename);

    if (buffer_load(&ed->buf, filename) == 0) {
        ed->dirty = 0;
        editor_set_status(ed, "\"%s\" — %d lines", filename, ed->buf.num_lines);
    } else {
        editor_set_status(ed, "New file: %s", filename);
    }
}

void editor_save(Editor *ed)
{
    if (!ed->filename) {
        char *name = editor_prompt(ed, "Save as: ", NULL, NULL);
        if (!name) {
            editor_set_status(ed, "Save cancelled.");
            return;
        }
        ed->filename = name;
    }

    if (buffer_save(&ed->buf, ed->filename) == 0) {
        ed->dirty = 0;
        editor_set_status(ed, "\"%s\" written — %d lines",
                          ed->filename, ed->buf.num_lines);
    } else {
        editor_set_status(ed, "Save error: %s", strerror(errno));
    }
}


/* ================================================================
 *  Screen drawing
 * ================================================================ */

static void editor_draw_rows(Editor *ed)
{
    /* Compute initial code-block state at scroll_y */
    int in_code = 0;
    for (int i = 0; i < ed->scroll_y && i < ed->buf.num_lines; i++) {
        if (render_is_code_fence(buffer_line_data(&ed->buf, i)))
            in_code = !in_code;
    }

    if (ed->word_wrap) {
        /* ── Wrapped mode: one buffer line may span multiple screen rows ── */
        int y = 0;
        int frow = ed->scroll_y;
        while (y < ed->screen_rows && frow < ed->buf.num_lines) {
            char     *line = buffer_line_data(&ed->buf, frow);
            int       len  = buffer_line_len(&ed->buf, frow);
            BlockType bt   = render_get_block_type(line, in_code);
            int       hl   = (bt == BLOCK_HEADING)
                             ? render_heading_level(line) : 0;

            if (bt == BLOCK_CODE_FENCE) in_code = !in_code;

            int is_tbl = (bt == BLOCK_PARAGRAPH && is_table_line(line, len));
            int used;
            if (is_tbl) {
                render_draw_line(y, ed->screen_cols, line, len, 0, bt, hl);
                used = 1;
            } else {
                int ci = render_line_content_indent(line, len, bt);
                int remaining = ed->screen_rows - y;
                used = render_draw_line_wrapped(y, ed->screen_cols,
                                                    line, len, bt, hl,
                                                    remaining, ci);
            }
            y += used;
            frow++;
        }
        /* Fill remaining rows with ~ */
        while (y < ed->screen_rows) {
            move(y, 0);
            clrtoeol();
            attron(COLOR_PAIR(CP_DIMMED) | A_DIM);
            addch('~');
            attroff(COLOR_PAIR(CP_DIMMED) | A_DIM);
            y++;
        }
    } else {
        /* ── Non-wrapped mode: one buffer line per screen row ── */
        for (int y = 0; y < ed->screen_rows; y++) {
            int frow = ed->scroll_y + y;

            if (frow < ed->buf.num_lines) {
                char     *line = buffer_line_data(&ed->buf, frow);
                int       len  = buffer_line_len(&ed->buf, frow);
                BlockType bt   = render_get_block_type(line, in_code);
                int       hl   = (bt == BLOCK_HEADING)
                                 ? render_heading_level(line) : 0;

                if (bt == BLOCK_CODE_FENCE) in_code = !in_code;

                render_draw_line(y, ed->screen_cols, line, len,
                                 ed->scroll_x, bt, hl);

                /* Overlay search highlights */
                if (ed->search_query_len > 0) {
                    char *p = line;
                    while ((p = strstr(p, ed->search_query)) != NULL) {
                        int mc = (int)(p - line);
                        for (int x = mc; x < mc + ed->search_query_len; x++) {
                            int sx = x - ed->scroll_x;
                            if (sx >= 0 && sx < ed->screen_cols && x < len) {
                                move(y, sx);
                                attron(COLOR_PAIR(CP_SEARCH_HL) | A_BOLD);
                                addch((unsigned char)line[x]);
                                attroff(COLOR_PAIR(CP_SEARCH_HL) | A_BOLD);
                            }
                        }
                        p++;
                    }
                }
            } else {
                /* Past end of file */
                move(y, 0);
                clrtoeol();
                attron(COLOR_PAIR(CP_DIMMED) | A_DIM);
                addch('~');
                attroff(COLOR_PAIR(CP_DIMMED) | A_DIM);
            }
        }
    }
}

static void editor_draw_preview_rows(Editor *ed)
{
    if (ed->word_wrap) {
        int y = 0;
        int prow = ed->preview_scroll_y;
        while (y < ed->screen_rows && prow < ed->preview_buf.num_lines) {
            int remaining = ed->screen_rows - y;
            int used = preview_draw_line_wrapped(y, ed->screen_cols,
                                                 &ed->preview_buf.lines[prow],
                                                 remaining);
            if (ed->search_query_len > 0)
                preview_highlight_search_wrapped(y, ed->screen_cols,
                                                 &ed->preview_buf.lines[prow],
                                                 used,
                                                 ed->search_query,
                                                 ed->search_query_len);
            y += used;
            prow++;
        }
        while (y < ed->screen_rows) {
            move(y, 0);
            clrtoeol();
            y++;
        }
    } else {
        for (int y = 0; y < ed->screen_rows; y++) {
            int prow = ed->preview_scroll_y + y;
            if (prow < ed->preview_buf.num_lines) {
                preview_draw_line(y, ed->screen_cols,
                                  &ed->preview_buf.lines[prow], 0);
                if (ed->search_query_len > 0)
                    preview_highlight_search(y, ed->screen_cols,
                                             &ed->preview_buf.lines[prow], 0,
                                             ed->search_query,
                                             ed->search_query_len);
            } else {
                move(y, 0);
                clrtoeol();
            }
        }
    }
}

static void editor_draw_help_rows(Editor *ed)
{
    if (ed->word_wrap) {
        int y = 0;
        int prow = ed->help_scroll_y;
        while (y < ed->screen_rows && prow < ed->help_buf.num_lines) {
            int remaining = ed->screen_rows - y;
            int used = preview_draw_line_wrapped(y, ed->screen_cols,
                                                 &ed->help_buf.lines[prow],
                                                 remaining);
            y += used;
            prow++;
        }
        while (y < ed->screen_rows) {
            move(y, 0);
            clrtoeol();
            y++;
        }
    } else {
        for (int y = 0; y < ed->screen_rows; y++) {
            int prow = ed->help_scroll_y + y;
            if (prow < ed->help_buf.num_lines) {
                preview_draw_line(y, ed->screen_cols,
                                  &ed->help_buf.lines[prow], 0);
            } else {
                move(y, 0);
                clrtoeol();
            }
        }
    }
}

static void editor_draw_toc_rows(Editor *ed)
{
    for (int y = 0; y < ed->screen_rows; y++) {
        int prow = ed->toc_scroll_y + y;
        if (prow < ed->toc_buf.num_lines) {
            preview_draw_line(y, ed->screen_cols,
                              &ed->toc_buf.lines[prow], 0);
            if (prow == ed->toc_selected)
                mvchgat(y, 0, -1, A_REVERSE, 0, NULL);
        } else {
            move(y, 0);
            clrtoeol();
        }
    }
}

static void editor_draw_statusbar(Editor *ed)
{
    int y = ed->screen_rows;

    /* ── Status bar ── */
    attron(COLOR_PAIR(CP_STATUSBAR));

    char left[256], right[128];
    int llen, rlen;

    if (ed->toc_mode) {
        int n = ed->toc_buf.num_lines;
        llen = snprintf(left, sizeof(left), " [TOC] %d heading%s",
                        n, n == 1 ? "" : "s");
        rlen = snprintf(right, sizeof(right), "%d/%d ",
                        ed->toc_selected + 1, n);
    } else if (ed->help_mode) {
        llen = snprintf(left, sizeof(left), " [HELP]");
        int max_s = help_max_scroll(ed);
        int pct = (max_s > 0) ? (ed->help_scroll_y * 100 / max_s) : 100;
        rlen = snprintf(right, sizeof(right), "%d%% (%d/%d) ",
                        pct, ed->help_scroll_y + 1, ed->help_buf.num_lines);
    } else if (ed->preview_mode) {
        llen = snprintf(left, sizeof(left), " %s [PREVIEW]",
                        ed->filename ? ed->filename : "[New File]");
        int cur = ed->preview_scroll_y + 1;
        int tot = ed->preview_buf.num_lines;
        int max_s = preview_max_scroll(ed);
        int pct = (max_s > 0) ? (ed->preview_scroll_y * 100 / max_s) : 100;
        rlen = snprintf(right, sizeof(right), "%d%% (%d/%d) ",
                        pct, cur, tot);
    } else {
        llen = snprintf(left, sizeof(left), " %s%s",
                        ed->filename ? ed->filename : "[New File]",
                        ed->dirty ? " [+]" : "");
        rlen = snprintf(right, sizeof(right), "Ln %d, Col %d  %d lines%s ",
                        ed->cy + 1, ed->cx + 1, ed->buf.num_lines,
                        ed->word_wrap ? "  wrap" : "");
    }
    if (llen < 0) llen = 0;
    if (rlen < 0) rlen = 0;

    move(y, 0);
    for (int x = 0; x < ed->screen_cols; x++) {
        if (x < llen)
            addch((unsigned char)left[x]);
        else if (x >= ed->screen_cols - rlen)
            addch((unsigned char)right[x - (ed->screen_cols - rlen)]);
        else
            addch(' ');
    }

    attroff(COLOR_PAIR(CP_STATUSBAR));

    /* ── Message / help bar ── */
    move(y + 1, 0);
    clrtoeol();

    if (ed->statusmsg[0] &&
        time(NULL) - ed->statusmsg_time < MDE_STATUS_MSG_TIMEOUT) {
        attron(A_BOLD);
        int ml = (int)strlen(ed->statusmsg);
        if (ml > ed->screen_cols) ml = ed->screen_cols;
        for (int i = 0; i < ml; i++)
            addch((unsigned char)ed->statusmsg[i]);
        attroff(A_BOLD);
    } else {
        const char *help;
        char search_hint[512];
        if (ed->search_query_len > 0 && !ed->toc_mode && !ed->help_mode) {
            if (ed->preview_mode) {
                snprintf(search_hint, sizeof(search_hint),
                         "Search: \"%s\"  |  Ctrl+N next  |  Ctrl+F new search",
                         ed->search_query);
            } else {
                snprintf(search_hint, sizeof(search_hint),
                         "Search: \"%s\"  |  Ctrl+N next  |  Esc Preview",
                         ed->search_query);
            }
            help = search_hint;
        } else if (ed->toc_mode)
            help = "Up/Down Navigate | Enter Jump | Esc Close";
        else if (ed->help_mode)
            help = "Up/Down Scroll | PgUp/PgDn Page | Esc Close";
        else if (ed->preview_mode)
            help = "Ctrl+P Edit | Ctrl+F Search | Ctrl+S Save | "
                   "Ctrl+Q Quit | F1 Help";
        else
            help = "Ctrl+P Preview | Ctrl+S Save | Ctrl+Z Undo | Ctrl+Y Redo | "
                   "Ctrl+F Search | Ctrl+T TOC | F1 Help";
        attron(A_DIM);
        int hl = (int)strlen(help);
        if (hl > ed->screen_cols) hl = ed->screen_cols;
        for (int i = 0; i < hl; i++)
            addch((unsigned char)help[i]);
        attroff(A_DIM);
    }
}

void editor_refresh_screen(Editor *ed)
{
    getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
    ed->screen_rows -= 2;          /* status bar + message line */
    if (ed->screen_rows < 1) ed->screen_rows = 1;

    if (ed->toc_mode) {
        toc_clamp_scroll(ed);
        editor_draw_toc_rows(ed);
    } else if (ed->help_mode) {
        clamp_help_scroll(ed);
        editor_draw_help_rows(ed);
    } else if (ed->preview_mode) {
        clamp_preview_scroll(ed);
        editor_draw_preview_rows(ed);
    } else {
        editor_scroll(ed);
        editor_draw_rows(ed);
    }

    editor_draw_statusbar(ed);

    if (ed->toc_mode || ed->help_mode || ed->preview_mode) {
        /* Hide cursor in TOC / preview / help */
        move(ed->screen_rows, 0);
    } else if (ed->word_wrap) {
        /* Place cursor accounting for wrapped lines */
        /* Pre-scan code fence state up to scroll_y */
        int cur_in_code = 0;
        for (int i = 0; i < ed->scroll_y && i < ed->buf.num_lines; i++) {
            if (render_is_code_fence(buffer_line_data(&ed->buf, i)))
                cur_in_code = !cur_in_code;
        }
        int cur_code = cur_in_code;
        int vis_y = 0;
        for (int r = ed->scroll_y; r < ed->cy; r++) {
            const char *rd = buffer_line_data(&ed->buf, r);
            int rl = buffer_line_len(&ed->buf, r);
            BlockType rbt = render_get_block_type(rd, cur_code);
            if (rbt == BLOCK_CODE_FENCE) cur_code = !cur_code;
            if (rbt == BLOCK_PARAGRAPH && is_table_line(rd, rl)) {
                vis_y += 1;
            } else {
                int ci = render_line_content_indent(rd, rl, rbt);
                vis_y += render_wrap_height(rd, rl, ed->screen_cols, ci);
            }
        }

        const char *line = buffer_line_data(&ed->buf, ed->cy);
        int line_len = buffer_line_len(&ed->buf, ed->cy);
        BlockType cy_bt = render_get_block_type(line, cur_code);
        int wrap_row, wrap_col;
        if (cy_bt == BLOCK_PARAGRAPH && is_table_line(line, line_len)) {
            wrap_row = 0;
            wrap_col = render_byte_to_col(line, line_len, ed->cx);
        } else {
            int cy_ci = render_line_content_indent(line, line_len, cy_bt);
            render_wrap_cursor_pos(line, line_len, ed->screen_cols, cy_ci,
                                   ed->cx, &wrap_row, &wrap_col);
        }
        vis_y += wrap_row;
        move(vis_y, wrap_col);
    } else {
        /* Place the visible cursor (convert byte offsets to columns) */
        const char *line = buffer_line_data(&ed->buf, ed->cy);
        int line_len = buffer_line_len(&ed->buf, ed->cy);
        int vis_y = ed->cy - ed->scroll_y;
        int vis_x = render_byte_to_col(line, line_len, ed->cx)
                  - render_byte_to_col(line, line_len, ed->scroll_x);
        move(vis_y, vis_x);
    }

    refresh();
}

/* ================================================================
 *  Key processing
 * ================================================================ */

static void editor_process_key(Editor *ed)
{
    char utf8[4];
    int  utf8_len;
    int  c = editor_read_key(utf8, &utf8_len);

    /* TOC mode has its own key handler */
    if (ed->toc_mode) {
        editor_toc_process_key(ed, c);
        return;
    }

    /* Help mode has its own key handler */
    if (ed->help_mode) {
        editor_help_process_key(ed, c);
        return;
    }

    /* Preview mode has its own key handler */
    if (ed->preview_mode) {
        editor_preview_process_key(ed, c);
        return;
    }

    ed->undo_seq++;

    if (c != CTRL_KEY('q'))
        ed->quit_times = MDE_QUIT_TIMES;

    switch (c) {

    /* ── Quit ── */
    case CTRL_KEY('q'):
        if (ed->dirty && ed->quit_times > 0) {
            editor_set_status(ed,
                "Unsaved changes! Ctrl+Q %d more time(s) to force quit.",
                ed->quit_times);
            ed->quit_times--;
            return;
        }
        ed->quit = 1;
        break;

    /* ── Save ── */
    case CTRL_KEY('s'):
        editor_save(ed);
        break;

    /* ── Search ── */
    case CTRL_KEY('f'):
        editor_search(ed);
        break;

    case CTRL_KEY('n'):
        editor_search_next(ed);
        break;

    /* ── Go to line ── */
    case CTRL_KEY('g'):
        editor_goto_line(ed);
        break;

    /* ── Preview ── */
    case CTRL_KEY('p'):
        editor_toggle_preview(ed);
        break;

    /* ── Table of Contents ── */
    case CTRL_KEY('t'):
        editor_show_toc(ed);
        break;

    /* ── Word wrap toggle ── */
    case CTRL_KEY('w'):
        ed->word_wrap = !ed->word_wrap;
        if (ed->word_wrap) ed->scroll_x = 0;
        editor_set_status(ed, "Word wrap %s", ed->word_wrap ? "ON" : "OFF");
        break;

    /* ── Undo / Redo ── */
    case CTRL_KEY('z'):
        editor_undo(ed);
        break;
    case CTRL_KEY('y'):
        editor_redo(ed);
        break;

    /* ── Delete to end of line ── */
    case CTRL_KEY('k'):
        editor_delete_to_eol(ed);
        break;

    /* ── Home / End (emacs style) ── */
    case CTRL_KEY('a'):
        ed->cx = 0;
        break;
    case CTRL_KEY('e'):
        ed->cx = buffer_line_len(&ed->buf, ed->cy);
        break;

    /* ── Open link by number ── */
    case CTRL_KEY('l'):
        editor_open_link_prompt(ed);
        break;

    /* ── Open file ── */
    case CTRL_KEY('o'):
        editor_open_file_prompt(ed);
        break;

    /* ── Help ── */
    case KEY_F(1):
        editor_show_help(ed);
        break;

    /* ── Enter ── */
    case '\r':
    case '\n':
    case KEY_ENTER:
        editor_insert_newline(ed);
        break;

    /* ── Backspace ── */
    case KEY_BACKSPACE:
    case 127:
    case CTRL_KEY('h'):
        editor_delete_backward(ed);
        break;

    /* ── Delete ── */
    case KEY_DC:
        editor_delete_forward(ed);
        break;

    /* ── Tab ── */
    case '\t':
        for (int i = 0; i < MDE_TAB_STOP; i++)
            editor_insert_char(ed, ' ');
        break;

    /* ── Navigation ── */
    case KEY_SR: case KEY_SF:
    case KEY_UP: case KEY_DOWN:
    case KEY_LEFT: case KEY_RIGHT:
    case KEY_PPAGE: case KEY_NPAGE:
    case KEY_HOME: case KEY_END:
        editor_move_cursor(ed, c);
        break;

    /* ── Terminal resize ── */
    case KEY_RESIZE:
        break;

    /* ── Escape: switch to preview mode ── */
    case 27:
        editor_toggle_preview(ed);
        break;

    /* ── Non-ASCII character (from editor_read_key) ── */
    case KEY_UTF8:
        for (int i = 0; i < utf8_len; i++)
            editor_insert_char(ed, (unsigned char)utf8[i]);
        break;

    /* ── ASCII printable character ── */
    default:
        if (c >= 32 && c < 127)
            editor_insert_char(ed, c);
        break;
    }
}

/* ================================================================
 *  Init / Free / Run
 * ================================================================ */

void editor_init(Editor *ed)
{
    memset(ed, 0, sizeof(Editor));
    buffer_init(&ed->buf);
    undo_stack_init(&ed->undo);
    undo_stack_init(&ed->redo);
    ed->quit_times = MDE_QUIT_TIMES;
    ed->word_wrap = 1;
}

void editor_free(Editor *ed)
{
    buffer_free(&ed->buf);
    preview_free(&ed->preview_buf);
    preview_free(&ed->help_buf);
    preview_free(&ed->toc_buf);
    undo_stack_free(&ed->undo);
    undo_stack_free(&ed->redo);
    free(ed->filename);
    ed->filename = NULL;
}

void editor_run(Editor *ed)
{
    /* ncurses setup */
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    curs_set(1);

    if (has_colors())
        render_init_colors();

    /* Start in preview mode */
    editor_toggle_preview(ed);
    editor_set_status(ed,
        "mde — Terminal Markdown Editor  |  "
        "Ctrl+Q Quit  |  Ctrl+S Save  |  Ctrl+P Edit  |  F1 Help");

    while (!ed->quit) {
        editor_refresh_screen(ed);
        editor_process_key(ed);
    }

    endwin();
}
