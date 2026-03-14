/* utf8 — UTF-8 encoding/decoding and display-width helpers. */
#ifndef UTF8_H
#define UTF8_H

/* Byte length of a UTF-8 character from its lead byte. */
int utf8_clen(unsigned char lead);

/* Byte length of the UTF-8 character at text[pos]. */
int utf8_char_bytes(const char *text, int len, int pos);

/* Byte offset of the start of the UTF-8 character before byte_pos. */
int utf8_prev_char(const char *text, int byte_pos);

/* Encode a Unicode codepoint as UTF-8.  Returns byte count (1-4). */
int wchar_to_utf8(unsigned long wc, char out[4]);

/* Display columns occupied by the UTF-8 character at text[pos..pos+n).
   Decodes the codepoint and delegates to POSIX wcwidth().
   Returns 1 as a safe fallback for non-printable or unrecognised chars. */
int utf8_char_width(const char *text, int len, int pos);

#endif
