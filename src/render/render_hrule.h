/* render_hrule — CommonMark thematic break and setext underline detection. */
#ifndef RENDER_HRULE_H
#define RENDER_HRULE_H

/* Returns 1 if `line` is a CommonMark thematic break (---, ***, ___,
   with optional leading spaces up to 3 and spaces between characters).
   Returns 0 otherwise. */
int render_is_thematic_break(const char *line);

/* Returns 1 if `line` is a setext H1 underline (===...),
   returns 2 if it is a setext H2 underline (---...),
   returns 0 otherwise.
   Only a solid run of the character plus optional trailing spaces qualifies;
   spaces between characters (e.g. "- - -") do NOT match. */
int render_is_setext_underline(const char *line);

#endif
