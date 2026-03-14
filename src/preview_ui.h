/* preview_ui — Preview mode toggle and key handling. */
#ifndef PREVIEW_UI_H
#define PREVIEW_UI_H

#include "editor.h"

int  preview_max_scroll(Editor *ed);
void clamp_preview_scroll(Editor *ed);
void editor_toggle_preview(Editor *ed);
void editor_preview_process_key(Editor *ed, int c);

#endif
