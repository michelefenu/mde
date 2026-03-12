#include "command.h"
#include "links.h"
#include "search.h"
#include "help.h"
#include "preview_ui.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define MAX_LINKS 256

static void open_url(Editor *ed, const char *url)
{
    char cmd[600];
#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd), "open '%s' >/dev/null 2>&1", url);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1", url);
#endif
    def_prog_mode();
    endwin();
    int ret = system(cmd);
    reset_prog_mode();
    refresh();
    if (ret != 0)
        editor_set_status(ed, "Failed to open: %s", url);
    else
        editor_set_status(ed, "Opened: %s", url);
}

void editor_command_mode(Editor *ed)
{
    char *input = editor_prompt(ed, ":", NULL);
    if (!input) {
        editor_set_status(ed, "");
        return;
    }

    /* Strip leading/trailing whitespace */
    char *cmd = input;
    while (*cmd && isspace((unsigned char)*cmd)) cmd++;
    char *end = cmd + strlen(cmd) - 1;
    while (end > cmd && isspace((unsigned char)*end)) *end-- = '\0';

    if (strcmp(cmd, "w") == 0) {
        editor_save(ed);
    } else if (strcmp(cmd, "q") == 0) {
        if (ed->dirty) {
            editor_set_status(ed, "Unsaved changes, use :q! to force");
        } else {
            ed->quit = 1;
        }
    } else if (strcmp(cmd, "q!") == 0) {
        ed->quit = 1;
    } else if (strcmp(cmd, "wq") == 0 || strcmp(cmd, "x") == 0) {
        editor_save(ed);
        if (!ed->dirty)
            ed->quit = 1;
    } else if (strncmp(cmd, "e ", 2) == 0) {
        char *filename = cmd + 2;
        while (*filename && isspace((unsigned char)*filename)) filename++;
        if (*filename) {
            if (ed->dirty) {
                editor_set_status(ed,
                    "Unsaved changes, save first or use :q! to quit");
            } else {
                /* Exit preview, reload file, re-enter preview */
                editor_toggle_preview(ed);
                buffer_free(&ed->buf);
                buffer_init(&ed->buf);
                undo_stack_clear(&ed->undo);
                undo_stack_clear(&ed->redo);
                ed->cx = 0;
                ed->cy = 0;
                ed->scroll_y = 0;
                ed->scroll_x = 0;
                ed->dirty = 0;
                ed->search_query[0] = '\0';
                ed->search_query_len = 0;
                editor_open(ed, filename);
                editor_toggle_preview(ed);
            }
        } else {
            editor_set_status(ed, "Usage: :e <filename>");
        }
    } else if (strcmp(cmd, "set wrap") == 0) {
        ed->word_wrap = 1;
        editor_set_status(ed, "Word wrap ON");
    } else if (strcmp(cmd, "set nowrap") == 0) {
        ed->word_wrap = 0;
        editor_set_status(ed, "Word wrap OFF");
    } else if (strcmp(cmd, "help") == 0) {
        editor_show_help(ed);
    } else if (strncmp(cmd, "open ", 5) == 0) {
        int idx = atoi(cmd + 5);
        if (idx <= 0) {
            editor_set_status(ed, "Usage: :open N  (N >= 1)");
        } else {
            LinkInfo links[MAX_LINKS];
            int count = links_collect(&ed->buf, links, MAX_LINKS);
            if (idx > count) {
                editor_set_status(ed, "No link %d (document has %d link%s)",
                                  idx, count, count == 1 ? "" : "s");
            } else {
                const char *url = links[idx - 1].url;
                if (url[0] == '#') {
                    /* Internal anchor — scroll preview */
                    int row = links_find_anchor(&ed->buf, url + 1);
                    if (row < 0) {
                        editor_set_status(ed, "Anchor not found: %s", url);
                    } else {
                        if (!ed->preview_mode) editor_toggle_preview(ed);
                        ed->preview_scroll_y =
                            preview_find_line(&ed->preview_buf, row);
                        clamp_preview_scroll(ed);
                        editor_set_status(ed, "Jumped to: %s", url);
                    }
                } else {
                    open_url(ed, url);
                }
            }
        }
    } else {
        /* Try as line number */
        int line = atoi(cmd);
        if (line > 0) {
            if (line > ed->buf.num_lines) line = ed->buf.num_lines;
            /* Exit preview, jump, re-enter preview */
            editor_toggle_preview(ed);
            ed->cy = line - 1;
            ed->cx = 0;
            editor_toggle_preview(ed);
            editor_set_status(ed, "Jumped to line %d", line);
        } else {
            editor_set_status(ed, "Not a command: %s", cmd);
        }
    }

    free(input);
}
