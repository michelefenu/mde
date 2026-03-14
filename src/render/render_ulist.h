/* render_ulist — Unordered list rendering for preview mode. */
#ifndef RENDER_ULIST_H
#define RENDER_ULIST_H

#include "render.h"

void gen_ulist(PreviewBuffer *pb, const char *line, int len,
               int source_row, int body_indent, int *link_idx);

#endif
