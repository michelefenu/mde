#ifndef HELP_H
#define HELP_H

#include "editor.h"

int  help_max_scroll(Editor *ed);
void clamp_help_scroll(Editor *ed);
void editor_show_help(Editor *ed);
void editor_close_help(Editor *ed);
void editor_help_process_key(Editor *ed, int c);

#endif
