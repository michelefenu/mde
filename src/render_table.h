#ifndef RENDER_TABLE_H
#define RENDER_TABLE_H

#include "render.h"
#include "buffer.h"

int  is_table_line(const char *line, int len);
void gen_table_block(PreviewBuffer *pb, Buffer *buf,
                     int start, int end, int screen_cols,
                     int body_indent, int *link_idx);

#endif
