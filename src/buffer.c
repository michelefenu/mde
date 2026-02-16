#include "buffer.h"
#include <stdio.h>
#include <string.h>

#define INITIAL_LINE_CAP  128
#define INITIAL_LINES_CAP  64

/* ── Line helpers ── */

static void line_init(Line *line, const char *data, int len)
{
    line->cap = len + 1;
    if (line->cap < INITIAL_LINE_CAP)
        line->cap = INITIAL_LINE_CAP;
    line->data = malloc(line->cap);
    if (data && len > 0)
        memcpy(line->data, data, len);
    line->data[len] = '\0';
    line->len = len;
}

static void line_free(Line *line)
{
    free(line->data);
    line->data = NULL;
    line->len  = 0;
    line->cap  = 0;
}

static void line_ensure_cap(Line *line, int needed)
{
    if (needed + 1 > line->cap) {
        line->cap = (needed + 1) * 2;
        line->data = realloc(line->data, line->cap);
    }
}

/* ── Buffer operations ── */

void buffer_init(Buffer *buf)
{
    buf->cap_lines = INITIAL_LINES_CAP;
    buf->lines     = malloc(sizeof(Line) * buf->cap_lines);
    buf->num_lines = 0;
    buffer_insert_line(buf, 0, "", 0);
}

void buffer_free(Buffer *buf)
{
    for (int i = 0; i < buf->num_lines; i++)
        line_free(&buf->lines[i]);
    free(buf->lines);
    buf->lines     = NULL;
    buf->num_lines = 0;
    buf->cap_lines = 0;
}

void buffer_insert_line(Buffer *buf, int at, const char *text, int len)
{
    if (at < 0) at = 0;
    if (at > buf->num_lines) at = buf->num_lines;

    if (buf->num_lines >= buf->cap_lines) {
        buf->cap_lines *= 2;
        buf->lines = realloc(buf->lines, sizeof(Line) * buf->cap_lines);
    }

    memmove(&buf->lines[at + 1], &buf->lines[at],
            sizeof(Line) * (buf->num_lines - at));
    line_init(&buf->lines[at], text, len);
    buf->num_lines++;
}

void buffer_delete_line(Buffer *buf, int at)
{
    if (at < 0 || at >= buf->num_lines) return;

    line_free(&buf->lines[at]);
    memmove(&buf->lines[at], &buf->lines[at + 1],
            sizeof(Line) * (buf->num_lines - at - 1));
    buf->num_lines--;
}

void buffer_insert_char(Buffer *buf, int row, int col, int ch)
{
    if (row < 0 || row >= buf->num_lines) return;
    Line *line = &buf->lines[row];
    if (col < 0)         col = 0;
    if (col > line->len) col = line->len;

    line_ensure_cap(line, line->len + 1);
    memmove(&line->data[col + 1], &line->data[col], line->len - col + 1);
    line->data[col] = (char)ch;
    line->len++;
}

int buffer_delete_char(Buffer *buf, int row, int col)
{
    if (row < 0 || row >= buf->num_lines) return 0;

    if (col > 0) {
        Line *line = &buf->lines[row];
        if (col > line->len) col = line->len;
        memmove(&line->data[col - 1], &line->data[col], line->len - col + 1);
        line->len--;
        return 0;
    } else if (row > 0) {
        Line *prev = &buf->lines[row - 1];
        Line *curr = &buf->lines[row];

        line_ensure_cap(prev, prev->len + curr->len);
        memcpy(&prev->data[prev->len], curr->data, curr->len + 1);
        prev->len += curr->len;

        buffer_delete_line(buf, row);
        return 1;
    }
    return 0;
}

void buffer_delete_forward(Buffer *buf, int row, int col)
{
    if (row < 0 || row >= buf->num_lines) return;
    Line *line = &buf->lines[row];

    if (col < line->len) {
        memmove(&line->data[col], &line->data[col + 1], line->len - col);
        line->len--;
    } else if (row + 1 < buf->num_lines) {
        Line *next = &buf->lines[row + 1];
        line_ensure_cap(line, line->len + next->len);
        memcpy(&line->data[line->len], next->data, next->len + 1);
        line->len += next->len;
        buffer_delete_line(buf, row + 1);
    }
}

void buffer_insert_newline(Buffer *buf, int row, int col)
{
    if (row < 0 || row >= buf->num_lines) return;
    Line *line = &buf->lines[row];
    if (col < 0)         col = 0;
    if (col > line->len) col = line->len;

    /* Copy tail before potential realloc */
    int   tail_len = line->len - col;
    char *tail     = malloc(tail_len + 1);
    memcpy(tail, &line->data[col], tail_len);
    tail[tail_len] = '\0';

    buffer_insert_line(buf, row + 1, tail, tail_len);
    free(tail);

    /* Truncate current line (safe after insert — index is still valid) */
    buf->lines[row].data[col] = '\0';
    buf->lines[row].len       = col;
}

void buffer_truncate_line(Buffer *buf, int row, int at)
{
    if (row < 0 || row >= buf->num_lines) return;
    Line *line = &buf->lines[row];
    if (at < 0)         at = 0;
    if (at < line->len) {
        line->data[at] = '\0';
        line->len = at;
    }
}

int buffer_line_len(Buffer *buf, int row)
{
    if (row < 0 || row >= buf->num_lines) return 0;
    return buf->lines[row].len;
}

char *buffer_line_data(Buffer *buf, int row)
{
    if (row < 0 || row >= buf->num_lines) return "";
    return buf->lines[row].data;
}

int buffer_load(Buffer *buf, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;

    /* Clear existing content */
    for (int i = 0; i < buf->num_lines; i++)
        line_free(&buf->lines[i]);
    buf->num_lines = 0;

    char tmp[8192];
    while (fgets(tmp, sizeof(tmp), fp)) {
        int len = (int)strlen(tmp);
        while (len > 0 && (tmp[len - 1] == '\n' || tmp[len - 1] == '\r'))
            len--;
        buffer_insert_line(buf, buf->num_lines, tmp, len);
    }
    fclose(fp);

    if (buf->num_lines == 0)
        buffer_insert_line(buf, 0, "", 0);

    return 0;
}

int buffer_save(Buffer *buf, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;

    for (int i = 0; i < buf->num_lines; i++)
        fprintf(fp, "%s\n", buf->lines[i].data);

    fclose(fp);
    return 0;
}
