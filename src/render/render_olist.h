/* render_olist — Ordered list rendering for preview mode. */
#ifndef RENDER_OLIST_H
#define RENDER_OLIST_H

#include "render.h"

void gen_olist(PreviewBuffer *pb, const char *line, int len,
               int source_row, int body_indent, int *link_idx,
               int display_num);

#endif
