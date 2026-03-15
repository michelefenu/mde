/* render_heading — ATX and setext heading rendering for preview mode. */
#ifndef RENDER_HEADING_H
#define RENDER_HEADING_H

#include "render.h"

/* Extract the visible content of an ATX heading (CommonMark 4.2 compliant).
   Skips leading spaces/# markers, strips trailing spaces and optional closing
   # sequence.  Sets *out_start and *out_len to the byte range within `line`. */
void render_heading_content(const char *line, int len,
                            int *out_start, int *out_len);

/* Generate a preview line for an ATX heading (# ... ).
   Moved from render.c; calls render_heading_content internally. */
void gen_heading(PreviewBuffer *pb, const char *line, int len,
                 int source_row, int screen_cols, int *link_idx);

/* Generate a preview line for a setext heading.
   `hlevel` is 1 (===) or 2 (---).  Strips only leading/trailing spaces from
   `line`; H1 gets a PM_HLINE underline row. */
void gen_setext_heading(PreviewBuffer *pb, const char *line, int len,
                        int source_row, int hlevel, int screen_cols,
                        int *link_idx);

#endif
