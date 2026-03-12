#include "command.h"
#include "search.h"
#include "help.h"
#include "preview_ui.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
