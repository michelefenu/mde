#include "editor.h"
#include "render.h"
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <wchar.h>

#define CTRL_KEY(k) ((k) & 0x1f)

/* Sentinel returned by editor_read_key() for non-ASCII characters.
   The actual UTF-8 bytes are in the output buffer. */
#define KEY_UTF8 (-2)

/* ================================================================
 *  UTF-8 helpers
 * ================================================================ */

/* Encode a Unicode codepoint as UTF-8.  Returns byte count (1-4). */
static int wchar_to_utf8(unsigned long wc, char out[4])
{
    if (wc < 0x80) {
        out[0] = (char)wc;
        return 1;
    }
    if (wc < 0x800) {
        out[0] = (char)(0xC0 | (wc >> 6));
        out[1] = (char)(0x80 | (wc & 0x3F));
        return 2;
    }
    if (wc < 0x10000) {
        out[0] = (char)(0xE0 | (wc >> 12));
        out[1] = (char)(0x80 | ((wc >> 6) & 0x3F));
        out[2] = (char)(0x80 | (wc & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (wc >> 18));
    out[1] = (char)(0x80 | ((wc >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((wc >> 6) & 0x3F));
    out[3] = (char)(0x80 | (wc & 0x3F));
    return 4;
}

/* Byte length of the UTF-8 character at text[pos]. */
static int utf8_char_bytes(const char *text, int len, int pos)
{
    if (pos >= len) return 0;
    unsigned char c = (unsigned char)text[pos];
    int n = 1;
    if      (c >= 0xF0) n = 4;
    else if (c >= 0xE0) n = 3;
    else if (c >= 0xC0) n = 2;
    if (pos + n > len) n = len - pos;
    return n;
}

/* Byte offset of the start of the UTF-8 character before byte_pos.
   Walks backwards over continuation bytes (10xxxxxx). */
static int utf8_prev_char(const char *text, int byte_pos)
{
    if (byte_pos <= 0) return 0;
    byte_pos--;
    while (byte_pos > 0 && ((unsigned char)text[byte_pos] & 0xC0) == 0x80)
        byte_pos--;
    return byte_pos;
}

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

static void editor_set_status(Editor *ed, const char *fmt, ...)
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

typedef void (*PromptCallback)(Editor *ed, const char *input, int key);

static void editor_refresh_screen(Editor *ed);   /* forward decl */

static char *editor_prompt(Editor *ed, const char *prompt_str,
                           PromptCallback cb)
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
            for (int r = ed->scroll_y; r < ed->cy; r++) {
                vis_y += render_wrap_height(
                    buffer_line_data(&ed->buf, r),
                    buffer_line_len(&ed->buf, r),
                    ed->screen_cols);
            }
            /* Add cursor's sub-line within its own line */
            const char *cl = buffer_line_data(&ed->buf, ed->cy);
            int cl_len = buffer_line_len(&ed->buf, ed->cy);
            int cx_col = render_byte_to_col(cl, cl_len, ed->cx);
            int sub = (ed->screen_cols > 0) ? cx_col / ed->screen_cols : 0;
            vis_y += sub;

            if (vis_y >= ed->screen_rows) {
                ed->scroll_y++;
            } else {
                break;
            }
        }
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
    buffer_insert_char(&ed->buf, ed->cy, ed->cx, c);
    ed->cx++;
    ed->dirty++;
}

static void editor_insert_newline(Editor *ed)
{
    buffer_insert_newline(&ed->buf, ed->cy, ed->cx);
    ed->cy++;
    ed->cx = 0;
    ed->dirty++;
}

static void editor_delete_backward(Editor *ed)
{
    if (ed->cx == 0 && ed->cy == 0) return;

    if (ed->cx > 0) {
        const char *line = buffer_line_data(&ed->buf, ed->cy);
        int prev = utf8_prev_char(line, ed->cx);
        int count = ed->cx - prev;
        for (int i = 0; i < count; i++)
            buffer_delete_forward(&ed->buf, ed->cy, prev);
        ed->cx = prev;
    } else {
        int prev_len = buffer_line_len(&ed->buf, ed->cy - 1);
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
        for (int i = 0; i < count; i++)
            buffer_delete_forward(&ed->buf, ed->cy, ed->cx);
    } else {
        buffer_delete_forward(&ed->buf, ed->cy, ed->cx);
    }
    ed->dirty++;
}

static void editor_delete_to_eol(Editor *ed)
{
    int ll = buffer_line_len(&ed->buf, ed->cy);
    if (ed->cx >= ll) return;
    buffer_truncate_line(&ed->buf, ed->cy, ed->cx);
    ed->dirty++;
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

static void editor_save(Editor *ed)
{
    if (!ed->filename) {
        char *name = editor_prompt(ed, "Save as: ", NULL);
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
 *  Search
 * ================================================================ */

static void search_callback(Editor *ed, const char *query, int key)
{
    int qlen = (int)strlen(query);

    if (key == 27) {   /* Escape → restore position */
        ed->cx       = ed->search_saved_cx;
        ed->cy       = ed->search_saved_cy;
        ed->scroll_y = ed->search_saved_scroll_y;
        ed->search_query[0]  = '\0';
        ed->search_query_len = 0;
        return;
    }

    if (qlen == 0) {
        ed->search_query[0]  = '\0';
        ed->search_query_len = 0;
        return;
    }

    /* Copy for highlight */
    strncpy(ed->search_query, query, sizeof(ed->search_query) - 1);
    ed->search_query[sizeof(ed->search_query) - 1] = '\0';
    ed->search_query_len = qlen;

    /* Forward search from saved position */
    for (int off = 0; off < ed->buf.num_lines; off++) {
        int row   = (ed->search_saved_cy + off) % ed->buf.num_lines;
        char *line = buffer_line_data(&ed->buf, row);
        int   start = (off == 0) ? ed->search_saved_cx : 0;
        char *match = strstr(line + start, query);
        if (match) {
            ed->cy = row;
            ed->cx = (int)(match - line);
            return;
        }
    }
}

static void editor_search(Editor *ed)
{
    ed->search_saved_cx       = ed->cx;
    ed->search_saved_cy       = ed->cy;
    ed->search_saved_scroll_y = ed->scroll_y;

    char *q = editor_prompt(ed, "Search (Esc = cancel): ", search_callback);
    if (q) {
        strncpy(ed->search_query, q, sizeof(ed->search_query) - 1);
        ed->search_query[sizeof(ed->search_query) - 1] = '\0';
        ed->search_query_len = (int)strlen(q);
        free(q);
    }
}

static void editor_search_next(Editor *ed)
{
    if (ed->search_query_len == 0) {
        editor_set_status(ed, "No search query.");
        return;
    }

    for (int off = 1; off <= ed->buf.num_lines; off++) {
        int   row  = (ed->cy + off) % ed->buf.num_lines;
        char *line = buffer_line_data(&ed->buf, row);
        int   sc   = (off == 1 && row == ed->cy) ? ed->cx + 1 : 0;
        if (sc > buffer_line_len(&ed->buf, row)) continue;

        char *match = strstr(line + sc, ed->search_query);
        if (match) {
            ed->cy = row;
            ed->cx = (int)(match - line);
            editor_set_status(ed, "Match on line %d", row + 1);
            return;
        }
    }
    editor_set_status(ed, "Pattern not found: %s", ed->search_query);
}

/* ================================================================
 *  Go to line
 * ================================================================ */

static void editor_goto_line(Editor *ed)
{
    char *input = editor_prompt(ed, "Go to line: ", NULL);
    if (!input) return;

    int line = atoi(input);
    free(input);

    if (line < 1) line = 1;
    if (line > ed->buf.num_lines) line = ed->buf.num_lines;

    ed->cy = line - 1;
    ed->cx = 0;
    editor_set_status(ed, "Jumped to line %d", line);
}

/* ================================================================
 *  Preview mode
 * ================================================================ */

static void editor_toggle_preview(Editor *ed)
{
    if (ed->preview_mode) {
        /* Leave preview → edit */
        preview_free(&ed->preview_buf);
        ed->preview_mode = 0;
        curs_set(1);
        editor_set_status(ed, "Edit mode");
    } else {
        /* Enter preview */
        getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
        ed->screen_rows -= 2;
        preview_generate(&ed->preview_buf, &ed->buf, ed->screen_cols);
        ed->preview_mode = 1;
        ed->preview_scroll_y = preview_find_line(&ed->preview_buf,
                                                 ed->scroll_y);
        curs_set(0);
        editor_set_status(ed, "Preview mode — Ctrl+P or q to return");
    }
}

static void clamp_preview_scroll(Editor *ed)
{
    int max_s;
    if (ed->word_wrap) {
        /* With wrapping, each line may span multiple rows,
           so allow scrolling further toward the end */
        max_s = ed->preview_buf.num_lines - 1;
    } else {
        max_s = ed->preview_buf.num_lines - ed->screen_rows;
    }
    if (max_s < 0) max_s = 0;
    if (ed->preview_scroll_y > max_s) ed->preview_scroll_y = max_s;
    if (ed->preview_scroll_y < 0)     ed->preview_scroll_y = 0;
}

static void editor_preview_process_key(Editor *ed, int c)
{
    switch (c) {
    case CTRL_KEY('p'):
    case 'q':
    case 27:                              /* Escape */
        editor_toggle_preview(ed);
        break;

    case CTRL_KEY('q'):
        ed->quit = 1;
        break;

    case CTRL_KEY('w'):
        ed->word_wrap = !ed->word_wrap;
        clamp_preview_scroll(ed);
        editor_set_status(ed, "Word wrap %s", ed->word_wrap ? "ON" : "OFF");
        break;

    case KEY_UP:
    case 'k':
        ed->preview_scroll_y--;
        clamp_preview_scroll(ed);
        break;

    case KEY_DOWN:
    case 'j':
    case '\r':
    case '\n':
    case KEY_ENTER:
        ed->preview_scroll_y++;
        clamp_preview_scroll(ed);
        break;

    case KEY_PPAGE:
        ed->preview_scroll_y -= ed->screen_rows;
        clamp_preview_scroll(ed);
        break;

    case KEY_NPAGE:
    case ' ':
        ed->preview_scroll_y += ed->screen_rows;
        clamp_preview_scroll(ed);
        break;

    case KEY_HOME:
    case 'g':
        ed->preview_scroll_y = 0;
        break;

    case KEY_END:
    case 'G':
        ed->preview_scroll_y = ed->preview_buf.num_lines - ed->screen_rows;
        clamp_preview_scroll(ed);
        break;

    case KEY_RESIZE: {
        getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
        ed->screen_rows -= 2;
        int src = (ed->preview_scroll_y < ed->preview_buf.num_lines)
                  ? ed->preview_buf.lines[ed->preview_scroll_y].source_row
                  : 0;
        preview_free(&ed->preview_buf);
        preview_generate(&ed->preview_buf, &ed->buf, ed->screen_cols);
        ed->preview_scroll_y = preview_find_line(&ed->preview_buf, src);
        clamp_preview_scroll(ed);
        break;
    }

    default:
        break;
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

            int remaining = ed->screen_rows - y;
            int used = render_draw_line_wrapped(y, ed->screen_cols,
                                                line, len, bt, hl,
                                                remaining);
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
            } else {
                move(y, 0);
                clrtoeol();
            }
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

    if (ed->preview_mode) {
        llen = snprintf(left, sizeof(left), " %s [PREVIEW]",
                        ed->filename ? ed->filename : "[New File]");
        int cur = ed->preview_scroll_y + 1;
        int tot = ed->preview_buf.num_lines;
        int pct = tot > 0 ? cur * 100 / tot : 100;
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
        time(NULL) - ed->statusmsg_time < TMDE_STATUS_MSG_TIMEOUT) {
        attron(A_BOLD);
        int ml = (int)strlen(ed->statusmsg);
        if (ml > ed->screen_cols) ml = ed->screen_cols;
        for (int i = 0; i < ml; i++)
            addch((unsigned char)ed->statusmsg[i]);
        attroff(A_BOLD);
    } else {
        const char *help;
        if (ed->preview_mode)
            help = "j/k Scroll | Space PgDn | g/G Top/Bot | "
                   "Ctrl+W Wrap | q/Esc/Ctrl+P Edit | Ctrl+Q Quit";
        else
            help = "Ctrl+S Save | Ctrl+Q Quit | Ctrl+F Search | "
                   "Ctrl+N Next | Ctrl+G Goto | Ctrl+P Preview | Ctrl+W Wrap";
        attron(A_DIM);
        int hl = (int)strlen(help);
        if (hl > ed->screen_cols) hl = ed->screen_cols;
        for (int i = 0; i < hl; i++)
            addch((unsigned char)help[i]);
        attroff(A_DIM);
    }
}

static void editor_refresh_screen(Editor *ed)
{
    getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
    ed->screen_rows -= 2;          /* status bar + message line */
    if (ed->screen_rows < 1) ed->screen_rows = 1;

    if (ed->preview_mode) {
        clamp_preview_scroll(ed);
        editor_draw_preview_rows(ed);
    } else {
        editor_scroll(ed);
        editor_draw_rows(ed);
    }

    editor_draw_statusbar(ed);

    if (ed->preview_mode) {
        /* Hide cursor in preview */
        move(ed->screen_rows, 0);
    } else if (ed->word_wrap) {
        /* Place cursor accounting for wrapped lines */
        const char *line = buffer_line_data(&ed->buf, ed->cy);
        int line_len = buffer_line_len(&ed->buf, ed->cy);
        int cx_col = render_byte_to_col(line, line_len, ed->cx);
        int wrap_row = (ed->screen_cols > 0) ? cx_col / ed->screen_cols : 0;
        int wrap_col = (ed->screen_cols > 0) ? cx_col % ed->screen_cols : 0;

        int vis_y = 0;
        for (int r = ed->scroll_y; r < ed->cy; r++)
            vis_y += render_wrap_height(
                buffer_line_data(&ed->buf, r),
                buffer_line_len(&ed->buf, r),
                ed->screen_cols);
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

    /* Preview mode has its own key handler */
    if (ed->preview_mode) {
        editor_preview_process_key(ed, c);
        return;
    }

    if (c != CTRL_KEY('q'))
        ed->quit_times = TMDE_QUIT_TIMES;

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

    /* ── Word wrap toggle ── */
    case CTRL_KEY('w'):
        ed->word_wrap = !ed->word_wrap;
        if (ed->word_wrap) ed->scroll_x = 0;
        editor_set_status(ed, "Word wrap %s", ed->word_wrap ? "ON" : "OFF");
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

    /* ── Refresh ── */
    case CTRL_KEY('l'):
        clear();
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
        for (int i = 0; i < TMDE_TAB_STOP; i++)
            editor_insert_char(ed, ' ');
        break;

    /* ── Navigation ── */
    case KEY_UP: case KEY_DOWN:
    case KEY_LEFT: case KEY_RIGHT:
    case KEY_PPAGE: case KEY_NPAGE:
    case KEY_HOME: case KEY_END:
        editor_move_cursor(ed, c);
        break;

    /* ── Terminal resize ── */
    case KEY_RESIZE:
        break;

    /* ── Escape: clear search highlight ── */
    case 27:
        ed->search_query[0]  = '\0';
        ed->search_query_len = 0;
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
    ed->quit_times = TMDE_QUIT_TIMES;
}

void editor_free(Editor *ed)
{
    buffer_free(&ed->buf);
    preview_free(&ed->preview_buf);
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

    editor_set_status(ed,
        "tmde — Terminal Markdown Editor  |  "
        "Ctrl+Q Quit  |  Ctrl+S Save  |  Ctrl+P Preview");

    while (!ed->quit) {
        editor_refresh_screen(ed);
        editor_process_key(ed);
    }

    endwin();
}
