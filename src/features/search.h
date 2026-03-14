/* search — Text search and go-to-line navigation. */
#ifndef SEARCH_H
#define SEARCH_H

#include "editor.h"

void editor_search(Editor *ed);
void editor_search_next(Editor *ed);
void editor_search_prev(Editor *ed);
void editor_goto_line(Editor *ed);

#endif
