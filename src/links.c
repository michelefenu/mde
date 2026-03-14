/* links — Parse [text](url) links, GitHub-style slug generation. */
#include "links.h"
#include "render.h"
#include "utf8.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int links_collect(Buffer *buf, LinkInfo *out, int max)
{
    int count = 0;
    int in_code = 0;

    for (int row = 0; row < buf->num_lines && count < max; row++) {
        const char *line = buffer_line_data(buf, row);
        int len = buffer_line_len(buf, row);

        if (render_is_code_fence(line)) {
            in_code = !in_code;
            continue;
        }
        if (in_code) continue;

        for (int i = 0; i < len && count < max; ) {
            /* Skip inline code spans (`...`) to avoid counting links inside them */
            if (line[i] == '`') {
                int bt = 0;
                while (i < len && line[i] == '`') { bt++; i++; }
                /* Find matching closing run of exactly bt backticks */
                for (int j = i; j + bt <= len; j++) {
                    int ok = 1;
                    for (int k = 0; k < bt; k++)
                        if (line[j + k] != '`') { ok = 0; break; }
                    if (!ok) continue;
                    if (j + bt < len && line[j + bt] == '`') continue; /* too many */
                    i = j + bt;
                    break;
                }
                /* If no close found, i is already past opening ticks */
                continue;
            }

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

/* Slugify heading text following GitHub-style spec:
   - ASCII alphanumeric → lowercase
   - Underscore and hyphen → keep (collapse consecutive hyphens)
   - Space → convert to '-' (collapse consecutive)
   - Other ASCII punctuation → remove entirely
   - Multi-byte UTF-8 → copy as-is
   - Strip leading/trailing hyphens */
static void slugify(const char *src, int src_len, char *dst, int dst_size)
{
    int di = 0;
    int prev_dash = 1; /* 1 = suppress leading dashes */

    for (int i = 0; i < src_len && di < dst_size - 1; i++) {
        unsigned char c = (unsigned char)src[i];

        if (c >= 0x80) {
            /* Multi-byte UTF-8: copy all bytes */
            int clen = utf8_clen(c);
            if (clen > src_len - i) clen = src_len - i;
            for (int b = 0; b < clen && di < dst_size - 1; b++)
                dst[di++] = src[i + b];
            i += clen - 1; /* -1 because for-loop increments */
            prev_dash = 0;
        } else if (isalnum(c)) {
            dst[di++] = (char)tolower(c);
            prev_dash = 0;
        } else if (c == '_') {
            dst[di++] = '_';
            prev_dash = 0;
        } else if (c == '-' || c == ' ') {
            if (!prev_dash && di < dst_size - 1) {
                dst[di++] = '-';
                prev_dash = 1;
            }
        }
        /* Other ASCII punctuation: silently removed */
    }
    /* Strip trailing dash */
    if (di > 0 && dst[di - 1] == '-') di--;
    dst[di] = '\0';
}

int links_find_anchor(Buffer *buf, const char *anchor)
{
    char slug[512];
    int in_code = 0;

    /* Duplicate-heading tracking */
    typedef struct { char slug[512]; int count; } SlugEntry;
    SlugEntry *seen = NULL;
    int seen_count = 0;
    int seen_cap = 0;
    int result = -1;

    for (int row = 0; row < buf->num_lines; row++) {
        const char *line = buffer_line_data(buf, row);
        int len = buffer_line_len(buf, row);

        if (render_is_code_fence(line)) {
            in_code = !in_code;
            continue;
        }
        if (in_code) continue;

        int level = render_heading_level(line);
        if (level == 0) continue;

        /* Skip leading spaces, '#' chars, and the mandatory space
           (same pattern as toc.c) */
        const char *p = line;
        while (*p == ' ') p++;
        while (*p == '#') p++;
        if (*p == ' ') p++;

        int text_len = (int)(line + len - p);
        if (text_len < 0) text_len = 0;

        /* Strip inline Markdown to get plain text */
        char *plain_text = NULL;
        CharStyle *plain_styles = NULL;
        int plain_len = 0;
        strip_inline(p, text_len, 0, 0, &plain_text, &plain_styles,
                     &plain_len, NULL);

        slugify(plain_text, plain_len, slug, (int)sizeof(slug));

        free(plain_text);
        free(plain_styles);

        /* Track duplicates: find existing or add new */
        int dup_idx = -1;
        for (int i = 0; i < seen_count; i++) {
            if (strcmp(seen[i].slug, slug) == 0) {
                dup_idx = i;
                break;
            }
        }

        char final_slug[512];
        if (dup_idx >= 0) {
            seen[dup_idx].count++;
            snprintf(final_slug, sizeof(final_slug), "%s-%d",
                     slug, seen[dup_idx].count);
        } else {
            /* Add to seen list */
            if (seen_count >= seen_cap) {
                seen_cap = seen_cap ? seen_cap * 2 : 16;
                seen = realloc(seen, (size_t)seen_cap * sizeof(SlugEntry));
            }
            strncpy(seen[seen_count].slug, slug, sizeof(seen[seen_count].slug) - 1);
            seen[seen_count].slug[sizeof(seen[seen_count].slug) - 1] = '\0';
            seen[seen_count].count = 0;
            seen_count++;
            strncpy(final_slug, slug, sizeof(final_slug) - 1);
            final_slug[sizeof(final_slug) - 1] = '\0';
        }

        if (strcmp(final_slug, anchor) == 0) {
            result = row;
            break;
        }
    }

    free(seen);
    return result;
}
