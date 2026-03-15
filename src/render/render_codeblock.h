/* render_codeblock — Fenced and indented code block rendering for preview. */
#ifndef RENDER_CODEBLOCK_H
#define RENDER_CODEBLOCK_H

#include "render.h"

void gen_code_block_v(PreviewBuffer *pb, const VLine *vlines, int n,
                      int fence_idx, int content_start, int content_end,
                      int screen_cols, int body_indent);

void gen_indented_code_block(PreviewBuffer *pb,
                             const char **lines, const int *lens,
                             const int *source_rows, int n,
                             int screen_cols, int body_indent);

#endif
