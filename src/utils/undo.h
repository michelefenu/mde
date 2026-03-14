/* undo — Undo/redo stack with sequence-grouped entries. */
#ifndef UNDO_H
#define UNDO_H

typedef enum {
    UNDO_INSERT,    /* characters inserted at (row, col) */
    UNDO_DELETE,    /* characters deleted from (row, col) */
    UNDO_SPLIT,     /* newline inserted — line split at (row, col) */
    UNDO_JOIN,      /* line row+1 merged into row at col */
} UndoType;

typedef struct {
    UndoType type;
    int      row, col;
    char    *data;
    int      data_len;
    int      old_cx, old_cy;
    int      seq;
} UndoEntry;

typedef struct {
    UndoEntry *entries;
    int        count;
    int        cap;
} UndoStack;

void undo_stack_init(UndoStack *s);
void undo_stack_free(UndoStack *s);
void undo_stack_clear(UndoStack *s);
void undo_push(UndoStack *s, UndoType type, int row, int col,
               const char *data, int data_len,
               int old_cx, int old_cy, int seq);
UndoEntry *undo_pop(UndoStack *s);
int  undo_top_seq(UndoStack *s);
int  undo_empty(UndoStack *s);

#endif
