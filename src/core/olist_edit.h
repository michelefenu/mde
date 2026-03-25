/* olist_edit — Ordered list editing helpers (parse and renumber). */
#ifndef OLIST_EDIT_H
#define OLIST_EDIT_H

#include "buffer.h"
#include "undo.h"

/* Parse an ordered-list prefix from a line.
   Returns 1 if the line is an ordered list item, filling:
     *indent     — leading space count
     *num        — item number
     *delim      — delimiter character ('.' or ')')
     *prefix_end — byte offset past "N<delim> "
   Returns 0 if the line is not an ordered list item. */
int parse_olist_prefix(const char *line, int len,
                       int *indent, int *num, char *delim, int *prefix_end);

/* Renumber ordered list items starting at start_row.
   Walks forward while lines match expected_indent and expected_delim.
   Replaces numbers that differ from expected (start_num, start_num+1, ...).
   All undo entries use the given seq for atomic undo.
   Returns the number of undo entries pushed. */
int olist_renumber(Buffer *buf, UndoStack *undo, int start_row,
                   int start_num, int expected_indent,
                   char expected_delim, int seq);

#endif
