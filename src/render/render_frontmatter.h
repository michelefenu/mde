/* render_frontmatter — YAML front-matter detection and styled rendering. */
#ifndef RENDER_FRONTMATTER_H
#define RENDER_FRONTMATTER_H

#include "render.h"
#include "buffer.h"

/* Returns 1 if line is a --- fence, 2 if a ... fence, 0 otherwise.
   Trailing spaces are permitted; any other trailing characters are not. */
int  render_is_frontmatter_fence(const char *line);

/* Scan buf from row 0 for a valid YAML front-matter block.
   Returns the row index of the closing delimiter (--- or ...), or -1 when
   no valid front-matter is present (first line is not ---, or unclosed). */
int  render_frontmatter_extent(Buffer *buf);

/* Preview: emit one front-matter line (fence or content) styled with
   CP_FRONTMATTER / A_DIM into pb. */
void gen_frontmatter_line(PreviewBuffer *pb, const char *line, int len,
                          int source_row);

#endif
