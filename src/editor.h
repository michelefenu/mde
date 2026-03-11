#ifndef EDITOR_H
#define EDITOR_H

#include "buffer.h"
#include "render.h"
#include "undo.h"
#include <time.h>

#define MDE_QUIT_TIMES         2
#define MDE_TAB_STOP           4
#define MDE_STATUS_MSG_TIMEOUT 5

#define CTRL_KEY(k) ((k) & 0x1f)

typedef struct {
    Buffer buf;
    int    cx, cy;           /* Cursor position (col, row in buffer) */
    int    scroll_y;         /* Vertical scroll offset */
    int    scroll_x;         /* Horizontal scroll offset */
    int    screen_rows;      /* Rows available for text */
    int    screen_cols;      /* Columns available */
    char  *filename;         /* Current filename (NULL = untitled) */
    int    dirty;            /* Number of edits since last save */
    char   statusmsg[256];
    time_t statusmsg_time;
    int    quit_times;       /* Remaining confirmations before force quit */
    int    quit;             /* Set to 1 to exit main loop */
    /* Search state */
    char   search_query[256];
    int    search_query_len;
    int    search_saved_cx;
    int    search_saved_cy;
    int    search_saved_scroll_y;
    /* Preview mode */
    int           preview_mode;
    PreviewBuffer preview_buf;
    int           preview_scroll_y;
    /* Word wrap */
    int    word_wrap;            /* 0 = off (horizontal scroll), 1 = on */
    /* Undo / redo */
    UndoStack undo;
    UndoStack redo;
    int       undo_seq;          /* Sequence counter — groups entries per keystroke */
    /* Help mode */
    int           help_mode;
    int           help_was_preview; /* 1 if help was entered from preview mode */
    PreviewBuffer help_buf;
    int           help_scroll_y;
    /* TOC mode */
    int           toc_mode;
    int           toc_was_preview; /* 1 if TOC was opened from preview mode */
    int           toc_selected;    /* Index of highlighted TOC entry */
    int           toc_scroll_y;    /* First visible TOC line */
    PreviewBuffer toc_buf;
} Editor;

void editor_init(Editor *ed);
void editor_free(Editor *ed);
void editor_open(Editor *ed, const char *filename);
void editor_run(Editor *ed);

/* Internal API used by sub-modules (search, help, preview_ui) */
typedef void (*PromptCallback)(Editor *ed, const char *input, int key);
char *editor_prompt(Editor *ed, const char *prompt_str, PromptCallback cb);
void  editor_set_status(Editor *ed, const char *fmt, ...);
void  editor_refresh_screen(Editor *ed);

#endif
