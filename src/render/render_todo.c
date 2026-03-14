/* render_todo — Todo/checkbox rendering for preview and edit modes. */
#include "render_todo.h"
#include "render.h"
#include "utf8.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Returns 1 if the line is a GFM-style todo item: - [ ] or - [x]
   Sets *is_done, *cb_start (byte offset of '['), *text_start (offset
   of text content after "- [x] "). */
int parse_todo_item(const char *line, int len,
                    int *is_done, int *cb_start, int *text_start)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    if (i >= len || (line[i] != '-' && line[i] != '*' && line[i] != '+'))
        return 0;
    i++;
    if (i >= len || line[i] != ' ') return 0;
    i++;
    if (i + 2 >= len || line[i] != '[') return 0;
    if (line[i+1] != ' ' && line[i+1] != 'x' && line[i+1] != 'X') return 0;
    if (line[i+2] != ']') return 0;

    *is_done   = (line[i+1] == 'x' || line[i+1] == 'X');
    *cb_start  = i;
    i += 3;
    if (i < len && line[i] == ' ') i++;
    *text_start = i;
    return 1;
}

/* Color metadata tokens (~dur #tag @name yyyy-mm-dd) in a char array.
   Works on both raw source bytes (edit mode) and stripped preview bytes. */
void apply_todo_meta_styles(const char *text, int len, CharStyle *styles)
{
    for (int j = 0; j < len; j++) {
        char c = text[j];
        if (c == '~' || c == '#' || c == '@') {
            int start = j;
            j++;
            while (j < len && (isalnum((unsigned char)text[j]) ||
                                text[j] == '-' || text[j] == '_'))
                j++;
            if (j > start + 1) {
                for (int k = start; k < j; k++) {
                    styles[k].attr  = A_BOLD;
                    styles[k].cpair = CP_TODO_META;
                }
            }
            j--;
        } else if (isdigit((unsigned char)c) && j + 10 <= len) {
            /* yyyy-mm-dd */
            if (isdigit((unsigned char)text[j+1]) &&
                isdigit((unsigned char)text[j+2]) &&
                isdigit((unsigned char)text[j+3]) &&
                text[j+4] == '-' &&
                isdigit((unsigned char)text[j+5]) &&
                isdigit((unsigned char)text[j+6]) &&
                text[j+7] == '-' &&
                isdigit((unsigned char)text[j+8]) &&
                isdigit((unsigned char)text[j+9])) {
                int at_word_boundary = (j == 0 || text[j-1] == ' ');
                int after_ok = (j + 10 >= len || text[j+10] == ' ');
                if (at_word_boundary && after_ok) {
                    for (int k = j; k < j + 10; k++) {
                        styles[k].attr  = A_BOLD;
                        styles[k].cpair = CP_TODO_META;
                    }
                    j += 9;
                }
            }
        }
    }
}

void gen_todo(PreviewBuffer *pb, const char *line, int len,
              int source_row, int body_indent, int *link_idx,
              int is_done, int text_start)
{
    /* Count leading spaces to compute indent level */
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;

    /* Unicode ballot boxes */
    static const char box_open[] = "\xe2\x98\x90"; /* U+2610 */
    static const char box_done[] = "\xe2\x98\x91"; /* U+2611 */
    const char *box   = is_done ? box_done : box_open;
    int         boxsz = 3; /* UTF-8 byte length */

    short  cb_cpair = is_done ? CP_TODO_DONE : CP_TODO_OPEN;
    attr_t cb_attr  = is_done ? A_DIM : A_BOLD;
    attr_t base_attr  = is_done ? A_DIM : 0;
    short  base_cpair = is_done ? CP_TODO_DONE : CP_DEFAULT;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + text_start, len - text_start,
                 base_attr, base_cpair, &ct, &cs, &cl, link_idx);

    /* Color metadata tokens in the stripped content */
    apply_todo_meta_styles(ct, cl, cs);

    /* byte total: indent spaces + checkbox(3 bytes) + space + content */
    int total = indent + boxsz + 1 + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);

    for (int j = 0; j < boxsz; j++) {
        pl->text[p]         = box[j];
        pl->styles[p].attr  = cb_attr;
        pl->styles[p].cpair = cb_cpair;
        pl->styles[p].acs   = 0;
        p++;
    }

    pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);
}
