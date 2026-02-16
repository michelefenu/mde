#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>

typedef struct {
    char *data;    /* Null-terminated string */
    int   len;     /* Length (excluding null) */
    int   cap;     /* Allocated capacity */
} Line;

typedef struct {
    Line *lines;
    int   num_lines;
    int   cap_lines;
} Buffer;

void   buffer_init(Buffer *buf);
void   buffer_free(Buffer *buf);
void   buffer_insert_line(Buffer *buf, int at, const char *text, int len);
void   buffer_delete_line(Buffer *buf, int at);
void   buffer_insert_char(Buffer *buf, int row, int col, int ch);
int    buffer_delete_char(Buffer *buf, int row, int col);
void   buffer_delete_forward(Buffer *buf, int row, int col);
void   buffer_insert_newline(Buffer *buf, int row, int col);
void   buffer_truncate_line(Buffer *buf, int row, int at);
int    buffer_line_len(Buffer *buf, int row);
char  *buffer_line_data(Buffer *buf, int row);
int    buffer_load(Buffer *buf, const char *filename);
int    buffer_save(Buffer *buf, const char *filename);

#endif
