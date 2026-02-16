#ifndef RENDER_H
#define RENDER_H

#include <ncurses.h>

/* Color pairs */
enum {
    CP_DEFAULT = 0,
    CP_HEADING1,
    CP_HEADING2,
    CP_HEADING3,
    CP_HEADING4,
    CP_DIMMED,
    CP_CODE,
    CP_CODE_BLOCK,
    CP_LINK,
    CP_URL,
    CP_STATUSBAR,
    CP_BLOCKQUOTE,
    CP_LIST_MARKER,
    CP_HRULE,
    CP_SEARCH_HL,
    CP_MSGBAR,
};

/* Block-level element types */
typedef enum {
    BLOCK_PARAGRAPH,
    BLOCK_HEADING,
    BLOCK_LIST_UNORDERED,
    BLOCK_LIST_ORDERED,
    BLOCK_CODE_FENCE,
    BLOCK_CODE_CONTENT,
    BLOCK_BLOCKQUOTE,
    BLOCK_HRULE,
    BLOCK_EMPTY,
} BlockType;

/* Per-character style for rendering */
typedef struct {
    attr_t attr;
    short  cpair;
} CharStyle;

void      render_init_colors(void);
void      render_draw_line(int screen_y, int screen_cols,
                           const char *text, int len, int scroll_x,
                           BlockType btype, int hlevel);
BlockType render_get_block_type(const char *line, int in_code_block);
int       render_is_code_fence(const char *line);
int       render_heading_level(const char *line);

#endif
