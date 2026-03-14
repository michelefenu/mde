#include "help.h"
#include "help_md.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

int help_max_scroll(Editor *ed)
{
    if (ed->word_wrap) {
        int n = ed->help_buf.num_lines;
        if (n == 0) return 0;
        int total = 0;
        int first = n - 1;
        for (int i = n - 1; i >= 0; i--) {
            total += preview_wrap_height(&ed->help_buf.lines[i],
                                         ed->screen_cols);
            first = i;
            if (total >= ed->screen_rows) break;
        }
        return first;
    }
    int max_s = ed->help_buf.num_lines - ed->screen_rows;
    return max_s > 0 ? max_s : 0;
}

void clamp_help_scroll(Editor *ed)
{
    int max_s = help_max_scroll(ed);
    if (ed->help_scroll_y > max_s) ed->help_scroll_y = max_s;
    if (ed->help_scroll_y < 0)     ed->help_scroll_y = 0;
}

static void load_help_buffer(Editor *ed)
{
    Buffer tmp;
    buffer_init(&tmp);
    /* Free the default empty line */
    for (int i = 0; i < tmp.num_lines; i++)
        free(tmp.lines[i].data);
    tmp.num_lines = 0;

    /* Parse embedded help markdown into lines */
    const char *p = (const char *)docs_help_md;
    const char *end = p + docs_help_md_len;
    while (p < end) {
        const char *nl = memchr(p, '\n', end - p);
        int len = nl ? (int)(nl - p) : (int)(end - p);
        /* Strip trailing \r */
        if (len > 0 && p[len - 1] == '\r') len--;
        buffer_insert_line(&tmp, tmp.num_lines, p, len);
        p += (nl ? (int)(nl - p) + 1 : (int)(end - p));
    }
    if (tmp.num_lines == 0)
        buffer_insert_line(&tmp, 0, "", 0);

    preview_free(&ed->help_buf);
    preview_generate(&ed->help_buf, &tmp, ed->screen_cols);
    buffer_free(&tmp);
}

void editor_show_help(Editor *ed)
{
    ed->help_was_preview = ed->preview_mode;

    getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
    ed->screen_rows -= 2;

    load_help_buffer(ed);

    ed->help_mode = 1;
    ed->help_scroll_y = 0;
    curs_set(0);
    editor_set_status(ed, "Help — press Esc or F1 to close");
}

void editor_close_help(Editor *ed)
{
    preview_free(&ed->help_buf);
    ed->help_mode = 0;

    if (ed->help_was_preview) {
        curs_set(0);
        editor_set_status(ed, "Preview mode — Ctrl+P to edit");
    } else {
        curs_set(1);
        editor_set_status(ed, "");
    }
}

void editor_help_process_key(Editor *ed, int c)
{
    switch (c) {
    case 27:
    case KEY_F(1):
        editor_close_help(ed);
        break;

    case CTRL_KEY('q'):
        ed->quit = 1;
        break;

    case CTRL_KEY('w'):
        ed->word_wrap = !ed->word_wrap;
        clamp_help_scroll(ed);
        editor_set_status(ed, "Word wrap %s", ed->word_wrap ? "ON" : "OFF");
        break;

    case KEY_UP:
        ed->help_scroll_y--;
        clamp_help_scroll(ed);
        break;

    case KEY_DOWN:
        ed->help_scroll_y++;
        clamp_help_scroll(ed);
        break;

    case KEY_PPAGE:
        ed->help_scroll_y -= ed->screen_rows;
        clamp_help_scroll(ed);
        break;

    case KEY_NPAGE:
        ed->help_scroll_y += ed->screen_rows;
        clamp_help_scroll(ed);
        break;

    case KEY_HOME:
        ed->help_scroll_y = 0;
        break;

    case KEY_END:
        ed->help_scroll_y = ed->help_buf.num_lines - ed->screen_rows;
        clamp_help_scroll(ed);
        break;

    case KEY_RESIZE: {
        getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
        ed->screen_rows -= 2;
        load_help_buffer(ed);
        clamp_help_scroll(ed);
        break;
    }

    default:
        break;
    }
}
