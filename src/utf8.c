#include "utf8.h"

int utf8_clen(unsigned char c)
{
    if (c < 0x80)  return 1;
    if (c < 0xC0)  return 1;   /* stray continuation byte */
    if (c < 0xE0)  return 2;
    if (c < 0xF0)  return 3;
    return 4;
}

int utf8_char_bytes(const char *text, int len, int pos)
{
    if (pos >= len) return 0;
    unsigned char c = (unsigned char)text[pos];
    int n = 1;
    if      (c >= 0xF0) n = 4;
    else if (c >= 0xE0) n = 3;
    else if (c >= 0xC0) n = 2;
    if (pos + n > len) n = len - pos;
    return n;
}

int utf8_prev_char(const char *text, int byte_pos)
{
    if (byte_pos <= 0) return 0;
    byte_pos--;
    while (byte_pos > 0 && ((unsigned char)text[byte_pos] & 0xC0) == 0x80)
        byte_pos--;
    return byte_pos;
}

int wchar_to_utf8(unsigned long wc, char out[4])
{
    if (wc < 0x80) {
        out[0] = (char)wc;
        return 1;
    }
    if (wc < 0x800) {
        out[0] = (char)(0xC0 | (wc >> 6));
        out[1] = (char)(0x80 | (wc & 0x3F));
        return 2;
    }
    if (wc < 0x10000) {
        out[0] = (char)(0xE0 | (wc >> 12));
        out[1] = (char)(0x80 | ((wc >> 6) & 0x3F));
        out[2] = (char)(0x80 | (wc & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (wc >> 18));
    out[1] = (char)(0x80 | ((wc >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((wc >> 6) & 0x3F));
    out[3] = (char)(0x80 | (wc & 0x3F));
    return 4;
}
