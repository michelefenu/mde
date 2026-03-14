#ifndef COMMAND_H
#define COMMAND_H

#include "editor.h"

/* Tab-complete a filename in a prompt buffer.
   buf must start with the partial path; buflen is the current length.
   Returns the new length. */
int command_file_tab_complete(char *buf, int buflen, int maxlen);

/* Prompt the user for a filename and open it (handles unsaved-changes check).
   If the editor is in preview mode, exits and re-enters preview around the reload. */
void editor_open_file_prompt(Editor *ed);

/* Prompt the user for a link number and open it (browser or internal anchor). */
void editor_open_link_prompt(Editor *ed);

#endif
