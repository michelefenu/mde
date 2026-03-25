/* olist_edit — Ordered list parse + renumber for edit mode. */
#include "olist_edit.h"
#include <ctype.h>
#include <stdio.h>

int parse_olist_prefix(const char *line, int len,
                       int *indent, int *num, char *delim, int *prefix_end)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    *indent = i;

    if (i >= len || !isdigit((unsigned char)line[i])) return 0;

    int n = 0;
    int j = i;
    while (j < len && isdigit((unsigned char)line[j])) {
        n = n * 10 + (line[j] - '0');
        j++;
    }

    if (j >= len) return 0;
    char d = line[j];
    if (d != '.' && d != ')') return 0;
    if (j + 1 >= len || line[j + 1] != ' ') return 0;

    *num = n;
    *delim = d;
    *prefix_end = j + 2;
    return 1;
}

int olist_renumber(Buffer *buf, UndoStack *undo, int start_row,
                   int start_num, int expected_indent,
                   char expected_delim, int seq)
{
    int num = start_num;
    int undo_count = 0;

    for (int r = start_row; r < buf->num_lines; r++) {
        const char *line = buffer_line_data(buf, r);
        int len = buffer_line_len(buf, r);

        int indent, cur_num, prefix_end;
        char delim;
        if (!parse_olist_prefix(line, len, &indent, &cur_num, &delim, &prefix_end))
            break;
        if (indent != expected_indent || delim != expected_delim)
            break;

        if (cur_num != num) {
            int old_plen = prefix_end - indent;

            /* Record and perform delete of old number portion */
            undo_push(undo, UNDO_DELETE, r, indent,
                      line + indent, old_plen, 0, r, seq);
            for (int i = 0; i < old_plen; i++)
                buffer_delete_forward(buf, r, indent);
            undo_count++;

            /* Build, record, and insert new number portion */
            char new_numstr[24];
            int new_nlen = snprintf(new_numstr, sizeof(new_numstr),
                                    "%d%c ", num, expected_delim);
            undo_push(undo, UNDO_INSERT, r, indent,
                      new_numstr, new_nlen, 0, r, seq);
            for (int i = 0; i < new_nlen; i++)
                buffer_insert_char(buf, r, indent + i,
                                   (unsigned char)new_numstr[i]);
            undo_count++;
        }

        num++;
    }

    return undo_count;
}
