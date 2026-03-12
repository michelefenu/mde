#include "search.h"
#include <string.h>
#include <stdlib.h>

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

void editor_search(Editor *ed)
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

void editor_search_next(Editor *ed)
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

void editor_search_prev(Editor *ed)
{
    if (ed->search_query_len == 0) {
        editor_set_status(ed, "No search query.");
        return;
    }

    for (int off = 1; off <= ed->buf.num_lines; off++) {
        int   row  = (ed->cy - off + ed->buf.num_lines) % ed->buf.num_lines;
        char *line = buffer_line_data(&ed->buf, row);
        int   line_len = buffer_line_len(&ed->buf, row);

        /* Find the last match before the limit */
        int limit = (off == 1 && row == ed->cy) ? ed->cx : line_len;

        char *last_match = NULL;
        char *p = line;
        while ((p = strstr(p, ed->search_query)) != NULL) {
            if ((int)(p - line) < limit)
                last_match = p;
            else
                break;
            p++;
        }

        if (last_match) {
            ed->cy = row;
            ed->cx = (int)(last_match - line);
            editor_set_status(ed, "Match on line %d", row + 1);
            return;
        }
    }
    editor_set_status(ed, "Pattern not found: %s", ed->search_query);
}

void editor_goto_line(Editor *ed)
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
