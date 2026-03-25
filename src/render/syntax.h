/* syntax — Basic syntax highlighting for fenced code blocks. */
#ifndef SYNTAX_H
#define SYNTAX_H

#include "render.h"

const SyntaxLang *syntax_find_lang(const char *name, int name_len);
void syntax_highlight_line(const SyntaxLang *lang, const char *line, int len,
                           CharStyle *styles);

#endif
