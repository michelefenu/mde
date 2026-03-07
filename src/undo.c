#include "undo.h"
#include "xalloc.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 128

void undo_stack_init(UndoStack *s)
{
    s->cap     = INITIAL_CAP;
    s->entries = xmalloc(sizeof(UndoEntry) * s->cap);
    s->count   = 0;
}

void undo_stack_free(UndoStack *s)
{
    for (int i = 0; i < s->count; i++)
        free(s->entries[i].data);
    free(s->entries);
    s->entries = NULL;
    s->count   = 0;
    s->cap     = 0;
}

void undo_stack_clear(UndoStack *s)
{
    for (int i = 0; i < s->count; i++)
        free(s->entries[i].data);
    s->count = 0;
}

void undo_push(UndoStack *s, UndoType type, int row, int col,
               const char *data, int data_len,
               int old_cx, int old_cy, int seq)
{
    if (s->count >= s->cap) {
        s->cap *= 2;
        s->entries = xrealloc(s->entries, sizeof(UndoEntry) * s->cap);
    }

    UndoEntry *e = &s->entries[s->count++];
    e->type     = type;
    e->row      = row;
    e->col      = col;
    e->data_len = data_len;
    e->old_cx   = old_cx;
    e->old_cy   = old_cy;
    e->seq      = seq;

    if (data && data_len > 0) {
        e->data = xmalloc(data_len);
        memcpy(e->data, data, data_len);
    } else {
        e->data     = NULL;
        e->data_len = 0;
    }
}

UndoEntry *undo_pop(UndoStack *s)
{
    if (s->count == 0) return NULL;
    return &s->entries[--s->count];
}

int undo_top_seq(UndoStack *s)
{
    if (s->count == 0) return -1;
    return s->entries[s->count - 1].seq;
}

int undo_empty(UndoStack *s)
{
    return s->count == 0;
}
