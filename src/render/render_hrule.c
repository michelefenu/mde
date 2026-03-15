/* render_hrule — CommonMark thematic break and setext underline detection. */
#include "render_hrule.h"
#include <string.h>

int render_is_thematic_break(const char *line)
{
    int i = 0;
    while (i < 3 && line[i] == ' ') i++;

    char c = line[i];
    if (c != '-' && c != '*' && c != '_') return 0;

    int n = 0;
    while (line[i]) {
        if (line[i] == c) n++;
        else if (line[i] != ' ') return 0;
        i++;
    }
    return n >= 3;
}

int render_is_setext_underline(const char *line)
{
    if (!line || line[0] == '\0') return 0;

    char c = line[0];
    if (c != '=' && c != '-') return 0;

    int i = 0;
    int n = 0;
    while (line[i] == c) { n++; i++; }
    if (n < 1) return 0;

    /* Only trailing spaces allowed after the run */
    while (line[i] == ' ') i++;
    if (line[i] != '\0') return 0;

    return (c == '=') ? 1 : 2;
}
