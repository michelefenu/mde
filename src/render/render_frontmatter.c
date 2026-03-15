/* render_frontmatter — YAML front-matter detection and styled rendering. */
#include "render_frontmatter.h"
#include "buffer.h"
#include <string.h>

int render_is_frontmatter_fence(const char *line)
{
    if (!line) return 0;
    char c = line[0];
    if (c != '-' && c != '.') return 0;

    int i = 0;
    while (line[i] == c) i++;
    if (i != 3) return 0;

    while (line[i] == ' ') i++;
    if (line[i] != '\0') return 0;

    return (c == '-') ? 1 : 2;
}

int render_frontmatter_extent(Buffer *buf)
{
    if (!buf || buf->num_lines < 2) return -1;

    const char *first = buffer_line_data(buf, 0);
    if (render_is_frontmatter_fence(first) != 1) return -1;

    for (int r = 1; r < buf->num_lines; r++) {
        if (render_is_frontmatter_fence(buffer_line_data(buf, r)))
            return r;
    }
    return -1;  /* unclosed front-matter: not recognised */
}

void gen_frontmatter_line(PreviewBuffer *pb, const char *line, int len,
                          int source_row)
{
    if (len < 0) len = 0;
    PreviewLine *pl = pv_add(pb, source_row, len);
    if (!pl) return;

    for (int i = 0; i < len; i++) {
        pl->text[i]         = line[i];
        pl->styles[i].attr  = A_DIM;
        pl->styles[i].cpair = CP_FRONTMATTER;
        pl->styles[i].acs   = 0;
    }
    pl->text[len] = '\0';
}
