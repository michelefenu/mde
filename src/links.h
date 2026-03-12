#ifndef LINKS_H
#define LINKS_H

#include "buffer.h"

typedef struct {
    char url[512];
    char text[256];
} LinkInfo;

/* Scan buffer for [text](url) patterns (skips images).
   Stores results into out[0..N-1] (1-indexed by link number).
   Returns count of links found. max is the capacity of out. */
int links_collect(Buffer *buf, LinkInfo *out, int max);

/* Find the buffer row of a heading whose slugified text matches anchor.
   anchor should NOT include the leading '#'.
   Returns row index (0-based) or -1 if not found. */
int links_find_anchor(Buffer *buf, const char *anchor);

#endif
