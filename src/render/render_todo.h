/* render_todo — Todo/checkbox rendering for preview and edit modes. */
#ifndef RENDER_TODO_H
#define RENDER_TODO_H

#include "render.h"

int  parse_todo_item(const char *line, int len,
                     int *is_done, int *cb_start, int *text_start);
void apply_todo_meta_styles(const char *text, int len, CharStyle *styles);
void gen_todo(PreviewBuffer *pb, const char *line, int len,
              int source_row, int body_indent, int *link_idx,
              int is_done, int text_start);

#endif
