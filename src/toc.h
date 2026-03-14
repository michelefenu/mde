/* toc — Table-of-contents overlay extracted from headings. */
#ifndef TOC_H
#define TOC_H

#include "editor.h"

void toc_clamp_scroll(Editor *ed);
void editor_show_toc(Editor *ed);
void editor_close_toc(Editor *ed);
void editor_toc_process_key(Editor *ed, int c);

#endif
