/* xalloc — Exit-on-failure wrappers for malloc/realloc. */
#ifndef XALLOC_H
#define XALLOC_H

#include <stdlib.h>
#include <stdio.h>

static inline void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) { perror("mde: out of memory"); exit(1); }
    return p;
}

static inline void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n);
    if (!p) { perror("mde: out of memory"); exit(1); }
    return p;
}

#endif
