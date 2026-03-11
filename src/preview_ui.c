#include "preview_ui.h"
#include "help.h"
#include "toc.h"
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

/* ── Open link in browser ── */

static void editor_open_url(Editor *ed, const char *url)
{
    char cmd[1024];
#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd), "open '%s'", url);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open '%s'", url);
#endif
    def_prog_mode();
    endwin();
    int ret = system(cmd);
    reset_prog_mode();
    refresh();
    if (ret != 0)
        editor_set_status(ed, "Could not open URL");
}

typedef struct { char url[512]; char text[256]; } LinkInfo;

static int editor_collect_links(Buffer *buf, LinkInfo *links, int max_links)
{
    int count = 0;
    for (int r = 0; r < buf->num_lines && count < max_links; r++) {
        const char *line = buffer_line_data(buf, r);
        int len = buffer_line_len(buf, r);
        for (int i = 0; i < len && count < max_links; i++) {
            if (line[i] != '[') continue;
            /* Skip images */
            if (i > 0 && line[i - 1] == '!') continue;

            int bracket_end = -1;
            for (int j = i + 1; j < len; j++) {
                if (line[j] == ']') { bracket_end = j; break; }
            }
            if (bracket_end < 0 || bracket_end + 1 >= len ||
                line[bracket_end + 1] != '(') continue;

            int paren_end = -1;
            for (int j = bracket_end + 2; j < len; j++) {
                if (line[j] == ')') { paren_end = j; break; }
            }
            if (paren_end < 0) continue;

            int tlen = bracket_end - i - 1;
            int ulen = paren_end - bracket_end - 2;
            if (ulen <= 0) { i = paren_end; continue; }

            LinkInfo *li = &links[count];
            if (tlen >= (int)sizeof(li->text)) tlen = (int)sizeof(li->text) - 1;
            memcpy(li->text, line + i + 1, tlen);
            li->text[tlen] = '\0';

            if (ulen >= (int)sizeof(li->url)) ulen = (int)sizeof(li->url) - 1;
            memcpy(li->url, line + bracket_end + 2, ulen);
            li->url[ulen] = '\0';

            count++;
            i = paren_end;
        }
    }
    return count;
}

static void editor_preview_open_link(Editor *ed)
{
    LinkInfo links[64];
    int n = editor_collect_links(&ed->buf, links, 64);

    if (n == 0) {
        editor_set_status(ed, "No links found in document");
        return;
    }

    if (n == 1) {
        editor_set_status(ed, "Opening: %s", links[0].url);
        editor_open_url(ed, links[0].url);
        return;
    }

    /* Build a compact prompt listing links */
    char list[1024];
    int pos = 0;
    for (int i = 0; i < n && pos < (int)sizeof(list) - 40; i++) {
        pos += snprintf(list + pos, sizeof(list) - pos,
                        "%d:%s  ", i + 1, links[i].text);
    }
    editor_set_status(ed, "%s", list);
    editor_refresh_screen(ed);

    char *input = editor_prompt(ed, "Open link #: ", NULL);
    if (!input) {
        editor_set_status(ed, "");
        return;
    }

    int choice = atoi(input);
    free(input);
    if (choice < 1 || choice > n) {
        editor_set_status(ed, "Invalid link number");
        return;
    }

    editor_set_status(ed, "Opening: %s", links[choice - 1].url);
    editor_open_url(ed, links[choice - 1].url);
}

void editor_preview_process_key(Editor *ed, int c)
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

    case KEY_F(1):
        editor_show_help(ed);
        break;

    case CTRL_KEY('t'):
        editor_show_toc(ed);
        break;

    case 'o':
        editor_preview_open_link(ed);
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
