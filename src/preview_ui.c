#include "preview_ui.h"
#include "help.h"
#include "toc.h"
#include "search.h"
#include "command.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int preview_max_scroll(Editor *ed)
{
    if (ed->word_wrap) {
        int n = ed->preview_buf.num_lines;
        if (n == 0) return 0;
        int total = 0;
        int first = n - 1;
        for (int i = n - 1; i >= 0; i--) {
            total += preview_wrap_height(&ed->preview_buf.lines[i],
                                         ed->screen_cols);
            first = i;
            if (total >= ed->screen_rows) break;
        }
        return first;
    }
    int max_s = ed->preview_buf.num_lines - ed->screen_rows;
    return max_s > 0 ? max_s : 0;
}

void clamp_preview_scroll(Editor *ed)
{
    int max_s = preview_max_scroll(ed);
    if (ed->preview_scroll_y > max_s) ed->preview_scroll_y = max_s;
    if (ed->preview_scroll_y < 0)     ed->preview_scroll_y = 0;
}

void editor_toggle_preview(Editor *ed)
{
    ed->search_query[0]  = '\0';
    ed->search_query_len = 0;

    if (ed->preview_mode) {
        /* Leave preview → edit */
        preview_free(&ed->preview_buf);
        ed->preview_mode = 0;
        curs_set(1);
        editor_set_status(ed, "-- EDIT MODE --");
    } else {
        /* Enter preview */
        getmaxyx(stdscr, ed->screen_rows, ed->screen_cols);
        ed->screen_rows -= 2;
        preview_generate(&ed->preview_buf, &ed->buf, ed->screen_cols);
        ed->preview_mode = 1;
        ed->preview_scroll_y = preview_find_line(&ed->preview_buf,
                                                 ed->scroll_y);
        curs_set(0);
        editor_set_status(ed, "-- NORMAL --");
    }
}

void editor_preview_process_key(Editor *ed, int c)
{
    if (c != CTRL_KEY('q'))
        ed->quit_times = MDE_QUIT_TIMES;

    switch (c) {

    /* ── Toggle edit mode ── */
    case CTRL_KEY('p'):
        editor_toggle_preview(ed);
        break;

    /* ── Search ── */
    case '/':
        editor_search(ed);
        ed->preview_scroll_y = preview_find_line(&ed->preview_buf, ed->cy);
        clamp_preview_scroll(ed);
        break;

    case 'n':
        if (ed->search_query_len == 0) {
            editor_set_status(ed, "No search query");
            break;
        }
        editor_search_next(ed);
        ed->preview_scroll_y = preview_find_line(&ed->preview_buf, ed->cy);
        clamp_preview_scroll(ed);
        break;

    case 'N':
        if (ed->search_query_len == 0) {
            editor_set_status(ed, "No search query");
            break;
        }
        editor_search_prev(ed);
        ed->preview_scroll_y = preview_find_line(&ed->preview_buf, ed->cy);
        clamp_preview_scroll(ed);
        break;

    /* ── Command mode ── */
    case ':':
        editor_command_mode(ed);
        break;

    /* ── Undo / Redo ── */
    case 'u':
        editor_toggle_preview(ed);
        editor_undo(ed);
        editor_toggle_preview(ed);
        break;

    case CTRL_KEY('r'):
        editor_toggle_preview(ed);
        editor_redo(ed);
        editor_toggle_preview(ed);
        break;

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

    /* ── Word wrap ── */
    case CTRL_KEY('w'):
        ed->word_wrap = !ed->word_wrap;
        clamp_preview_scroll(ed);
        editor_set_status(ed, "Word wrap %s", ed->word_wrap ? "ON" : "OFF");
        break;

    /* ── Help ── */
    case KEY_F(1):
        editor_show_help(ed);
        break;

    /* ── Table of Contents ── */
    case CTRL_KEY('t'):
        editor_show_toc(ed);
        break;

    /* ── Scrolling ── */
    case KEY_SR:    /* Shift+Up */
        ed->preview_scroll_y -= 10;
        clamp_preview_scroll(ed);
        break;

    case KEY_SF:    /* Shift+Down */
        ed->preview_scroll_y += 10;
        clamp_preview_scroll(ed);
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
