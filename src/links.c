#include "links.h"
#include "render.h"
#include <ctype.h>
#include <string.h>

int links_collect(Buffer *buf, LinkInfo *out, int max)
{
    int count = 0;

    for (int row = 0; row < buf->num_lines && count < max; row++) {
        const char *line = buffer_line_data(buf, row);
        int len = buffer_line_len(buf, row);

        for (int i = 0; i < len && count < max; ) {
            if (line[i] != '[') { i++; continue; }

            /* Skip images: '!' before '[' */
            if (i > 0 && line[i - 1] == '!') { i++; continue; }

            /* Find closing ']' */
            int bracket_end = -1;
            for (int j = i + 1; j < len; j++) {
                if (line[j] == ']') { bracket_end = j; break; }
            }
            if (bracket_end < 0 || bracket_end + 1 >= len ||
                line[bracket_end + 1] != '(') { i++; continue; }

            /* Find closing ')' */
            int paren_end = -1;
            for (int j = bracket_end + 2; j < len; j++) {
                if (line[j] == ')') { paren_end = j; break; }
            }
            if (paren_end < 0) { i++; continue; }

            /* Extract text */
            int text_len = bracket_end - i - 1;
            if (text_len < 0) text_len = 0;
            if (text_len >= (int)sizeof(out[count].text))
                text_len = (int)sizeof(out[count].text) - 1;
            memcpy(out[count].text, line + i + 1, text_len);
            out[count].text[text_len] = '\0';

            /* Extract url */
            int url_len = paren_end - bracket_end - 2;
            if (url_len < 0) url_len = 0;
            if (url_len >= (int)sizeof(out[count].url))
                url_len = (int)sizeof(out[count].url) - 1;
            memcpy(out[count].url, line + bracket_end + 2, url_len);
            out[count].url[url_len] = '\0';

            count++;
            i = paren_end + 1;
        }
    }

    return count;
}

/* Slugify heading text: lowercase, non-alphanumeric → '-', collapse runs */
static void slugify(const char *src, char *dst, int dst_size)
{
    int di = 0;
    int prev_dash = 1; /* start with 1 to strip leading dashes */

    for (int i = 0; src[i] && di < dst_size - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c)) {
            dst[di++] = (char)tolower(c);
            prev_dash = 0;
        } else {
            if (!prev_dash && di < dst_size - 1) {
                dst[di++] = '-';
                prev_dash = 1;
            }
        }
    }
    /* Strip trailing dash */
    if (di > 0 && dst[di - 1] == '-') di--;
    dst[di] = '\0';
}

int links_find_anchor(Buffer *buf, const char *anchor)
{
    char slug[512];

    for (int row = 0; row < buf->num_lines; row++) {
        const char *line = buffer_line_data(buf, row);
        int level = render_heading_level(line);
        if (level == 0) continue;

        /* Skip leading '#' chars and space */
        const char *heading_text = line;
        while (*heading_text == '#') heading_text++;
        while (*heading_text == ' ') heading_text++;

        slugify(heading_text, slug, sizeof(slug));
        if (strcmp(slug, anchor) == 0)
            return row;
    }

    return -1;
}
