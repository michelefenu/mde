#ifndef RENDER_H
#define RENDER_H

#include <ncurses.h>
#include "buffer.h"

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
    attr_t        attr;
    short         cpair;
    unsigned char acs;    /* 0 = normal char, >0 = ACS marker id (PM_*) */
} CharStyle;

void      render_init_colors(void);
void      render_draw_line(int screen_y, int screen_cols,
                           const char *text, int len, int scroll_x,
                           BlockType btype, int hlevel);
BlockType render_get_block_type(const char *line, int in_code_block);
int       render_is_code_fence(const char *line);
int       render_heading_level(const char *line);
int       render_byte_to_col(const char *text, int len, int byte_pos);
int       render_wrap_height(const char *text, int len, int cols);
int       render_draw_line_wrapped(int screen_y, int screen_cols,
                                   const char *text, int len,
                                   BlockType btype, int hlevel,
                                   int max_rows);

/* ── Preview mode ── */

/* ACS marker IDs stored in CharStyle.acs.
   Converted to real ACS characters at draw time. */
#define PM_VLINE     1
#define PM_HLINE     2
#define PM_ULCORNER  3
#define PM_URCORNER  4
#define PM_LLCORNER  5
#define PM_LRCORNER  6
#define PM_LTEE      7
#define PM_RTEE      8
#define PM_TTEE      9
#define PM_BTEE     10
#define PM_PLUS     11
#define PM_BULLET   12

typedef struct {
    char      *text;       /* may contain PM_* marker bytes */
    CharStyle *styles;
    int        len;
    int        source_row; /* originating buffer line (-1 = virtual) */
} PreviewLine;

typedef struct {
    PreviewLine *lines;
    int          num_lines;
    int          cap_lines;
} PreviewBuffer;

void preview_generate(PreviewBuffer *pb, Buffer *buf, int screen_cols);
void preview_free(PreviewBuffer *pb);
void preview_draw_line(int screen_y, int screen_cols,
                       PreviewLine *pl, int scroll_x);
int  preview_wrap_height(PreviewLine *pl, int cols);
int  preview_draw_line_wrapped(int screen_y, int screen_cols,
                               PreviewLine *pl, int max_rows);
int  preview_find_line(PreviewBuffer *pb, int buffer_row);

#endif
