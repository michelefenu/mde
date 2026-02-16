#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Attribute used for dimmed / syntax marker characters.
   Updated at init time based on terminal colour depth. */
static attr_t g_dim_attr = A_DIM;

/* ================================================================
 *  Colour initialisation
 * ================================================================ */

void render_init_colors(void)
{
    if (!has_colors()) return;
    start_color();
    use_default_colors();

    short dim_fg = COLOR_WHITE;
    if (COLORS >= 256) {
        dim_fg    = 245;      /* medium grey in 256-colour mode */
        g_dim_attr = 0;        /* colour alone is enough */
    }

    init_pair(CP_HEADING1,    COLOR_CYAN,    -1);
    init_pair(CP_HEADING2,    COLOR_GREEN,   -1);
    init_pair(CP_HEADING3,    COLOR_YELLOW,  -1);
    init_pair(CP_HEADING4,    COLOR_MAGENTA, -1);
    init_pair(CP_DIMMED,      dim_fg,        -1);
    init_pair(CP_CODE,        COLOR_YELLOW,  -1);
    init_pair(CP_CODE_BLOCK,  COLOR_GREEN,   -1);
    init_pair(CP_LINK,        COLOR_CYAN,    -1);
    init_pair(CP_URL,         COLOR_BLUE,    -1);
    init_pair(CP_STATUSBAR,   COLOR_BLACK,   COLOR_CYAN);
    init_pair(CP_BLOCKQUOTE,  COLOR_CYAN,    -1);
    init_pair(CP_LIST_MARKER, COLOR_YELLOW,  -1);
    init_pair(CP_HRULE,       COLOR_BLUE,    -1);
    init_pair(CP_SEARCH_HL,   COLOR_BLACK,   COLOR_YELLOW);
    init_pair(CP_MSGBAR,      COLOR_WHITE,   -1);
}

/* ================================================================
 *  Block-level detection helpers
 * ================================================================ */

int render_is_code_fence(const char *line)
{
    int i = 0;
    while (i < 3 && line[i] == ' ') i++;

    char fc = line[i];
    if (fc != '`' && fc != '~') return 0;

    int n = 0;
    while (line[i] == fc) { n++; i++; }
    return n >= 3;
}

int render_heading_level(const char *line)
{
    int i = 0;
    while (i < 3 && line[i] == ' ') i++;
    if (line[i] != '#') return 0;

    int lv = 0;
    while (line[i] == '#') { lv++; i++; }
    if (line[i] != ' ' && line[i] != '\0') return 0;
    return (lv <= 6) ? lv : 0;
}

static int is_horizontal_rule(const char *line)
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

BlockType render_get_block_type(const char *line, int in_code_block)
{
    if (in_code_block)
        return render_is_code_fence(line) ? BLOCK_CODE_FENCE : BLOCK_CODE_CONTENT;

    if (render_is_code_fence(line)) return BLOCK_CODE_FENCE;

    int i = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;

    if (line[i] == '\0')
        return BLOCK_EMPTY;
    if (render_heading_level(line) > 0)
        return BLOCK_HEADING;
    if (is_horizontal_rule(line))
        return BLOCK_HRULE;
    if (line[i] == '>' && (line[i + 1] == ' ' || line[i + 1] == '\0'))
        return BLOCK_BLOCKQUOTE;
    if ((line[i] == '-' || line[i] == '*' || line[i] == '+') && line[i + 1] == ' ')
        return BLOCK_LIST_UNORDERED;
    if (isdigit((unsigned char)line[i])) {
        int j = i;
        while (isdigit((unsigned char)line[j])) j++;
        if ((line[j] == '.' || line[j] == ')') && line[j + 1] == ' ')
            return BLOCK_LIST_ORDERED;
    }
    return BLOCK_PARAGRAPH;
}

/* ================================================================
 *  Inline style computation
 * ================================================================ */

/* Find a closing run of `count` delimiter characters `ch`
 * starting from position `start`, skipping positions flagged
 * in `claimed` (value 1 = delimiter already used). */
static int find_close_delim(const char *text, int len, int start,
                            char ch, int count, const char *claimed)
{
    for (int i = start; i + count <= len; i++) {
        if (claimed[i]) continue;
        if (text[i] != ch) continue;

        int n = 0, j = i;
        while (j < len && text[j] == ch && !claimed[j]) { n++; j++; }
        if (n >= count) return i;
    }
    return -1;
}

/* --- Pass 1: code spans (`...`) --- */
static void apply_code_spans(const char *text, int len,
                             CharStyle *styles, char *claimed)
{
    for (int i = 0; i < len; ) {
        if (text[i] != '`' || claimed[i]) { i++; continue; }

        int start = i;
        int bt = 0;
        while (i < len && text[i] == '`') { bt++; i++; }

        int close = -1;
        for (int j = i; j + bt <= len; j++) {
            int ok = 1;
            for (int k = 0; k < bt; k++)
                if (text[j + k] != '`') { ok = 0; break; }
            if (!ok) continue;
            if (j + bt < len && text[j + bt] == '`') continue; /* too many */
            close = j;
            break;
        }

        if (close >= 0) {
            for (int j = start; j < start + bt; j++) {
                styles[j].attr  = g_dim_attr;
                styles[j].cpair = CP_DIMMED;
                claimed[j] = 1;
            }
            for (int j = start + bt; j < close; j++) {
                styles[j].cpair = CP_CODE;
                claimed[j] = 2;
            }
            for (int j = close; j < close + bt; j++) {
                styles[j].attr  = g_dim_attr;
                styles[j].cpair = CP_DIMMED;
                claimed[j] = 1;
            }
            i = close + bt;
        }
    }
}

/* --- Passes 2-4: emphasis (* / _ based) --- */
static void apply_emphasis(const char *text, int len,
                           CharStyle *styles, char *claimed, int dcount)
{
    for (int i = 0; i < len; ) {
        if (claimed[i]) { i++; continue; }

        char ch = text[i];
        if (ch != '*' && ch != '_') { i++; continue; }

        int start = i;
        int n = 0;
        while (start + n < len && text[start + n] == ch && !claimed[start + n])
            n++;

        if (n < dcount) { i += n; continue; }

        int close = find_close_delim(text, len, start + dcount,
                                     ch, dcount, claimed);
        if (close < 0) { i += n; continue; }

        /* Opening delimiters → dimmed */
        for (int j = start; j < start + dcount; j++) {
            styles[j].attr  = g_dim_attr;
            styles[j].cpair = CP_DIMMED;
            claimed[j] = 1;
        }

        /* Content → emphasis attributes (additive) */
        attr_t emph = 0;
        if (dcount >= 2) emph |= A_BOLD;
        if (dcount == 1 || dcount == 3) emph |= A_ITALIC;
        for (int j = start + dcount; j < close; j++) {
            if (claimed[j] != 1)
                styles[j].attr |= emph;
        }

        /* Closing delimiters → dimmed */
        for (int j = close; j < close + dcount && j < len; j++) {
            styles[j].attr  = g_dim_attr;
            styles[j].cpair = CP_DIMMED;
            claimed[j] = 1;
        }
        i = close + dcount;
    }
}

/* --- Pass 5: strikethrough (~~) --- */
static void apply_strikethrough(const char *text, int len,
                                CharStyle *styles, char *claimed)
{
    for (int i = 0; i + 1 < len; ) {
        if (claimed[i] || text[i] != '~' || text[i + 1] != '~') { i++; continue; }

        int start = i;
        i += 2;

        int close = -1;
        for (int j = i; j + 1 < len; j++) {
            if (!claimed[j] && text[j] == '~' && text[j + 1] == '~') {
                close = j;
                break;
            }
        }
        if (close < 0) continue;

        styles[start].attr     = g_dim_attr; styles[start].cpair     = CP_DIMMED; claimed[start]     = 1;
        styles[start + 1].attr = g_dim_attr; styles[start + 1].cpair = CP_DIMMED; claimed[start + 1] = 1;

        for (int j = start + 2; j < close; j++) {
            if (claimed[j] != 1)
                styles[j].attr |= A_DIM;
        }

        styles[close].attr     = g_dim_attr; styles[close].cpair     = CP_DIMMED; claimed[close]     = 1;
        styles[close + 1].attr = g_dim_attr; styles[close + 1].cpair = CP_DIMMED; claimed[close + 1] = 1;
        i = close + 2;
    }
}

/* --- Pass 6: links [text](url) and images ![alt](url) --- */
static void apply_links(const char *text, int len,
                        CharStyle *styles, char *claimed)
{
    for (int i = 0; i < len; ) {
        if (claimed[i] || text[i] != '[') { i++; continue; }

        int img_bang = (i > 0 && text[i - 1] == '!' && !claimed[i - 1]);

        int bracket_end = -1;
        for (int j = i + 1; j < len; j++) {
            if (text[j] == ']') { bracket_end = j; break; }
        }
        if (bracket_end < 0 || bracket_end + 1 >= len ||
            text[bracket_end + 1] != '(') { i++; continue; }

        int paren_end = -1;
        for (int j = bracket_end + 2; j < len; j++) {
            if (text[j] == ')') { paren_end = j; break; }
        }
        if (paren_end < 0) { i++; continue; }

        /* ! before [ */
        if (img_bang) {
            styles[i - 1].attr = g_dim_attr;
            styles[i - 1].cpair = CP_DIMMED;
            claimed[i - 1] = 1;
        }

        /* [ and ] */
        styles[i].attr           = g_dim_attr; styles[i].cpair           = CP_DIMMED; claimed[i]           = 1;
        styles[bracket_end].attr = g_dim_attr; styles[bracket_end].cpair = CP_DIMMED; claimed[bracket_end] = 1;

        /* Link text → coloured + underlined */
        for (int j = i + 1; j < bracket_end; j++) {
            if (claimed[j] != 1) {
                styles[j].cpair = CP_LINK;
                styles[j].attr |= A_UNDERLINE;
                claimed[j] = 2;
            }
        }

        /* (url) → dimmed */
        for (int j = bracket_end + 1; j <= paren_end; j++) {
            styles[j].attr  = g_dim_attr;
            styles[j].cpair = CP_URL;
            claimed[j] = 1;
        }

        i = paren_end + 1;
    }
}

/* Orchestrate all inline passes */
static void apply_inline_styles(const char *text, int len, CharStyle *styles)
{
    char *claimed = calloc(len + 1, 1);

    apply_code_spans(text, len, styles, claimed);
    apply_emphasis(text, len, styles, claimed, 3); /* *** bold+italic */
    apply_emphasis(text, len, styles, claimed, 2); /* **  bold        */
    apply_emphasis(text, len, styles, claimed, 1); /* *   italic      */
    apply_strikethrough(text, len, styles, claimed);
    apply_links(text, len, styles, claimed);

    free(claimed);
}

/* ================================================================
 *  Block-level marker styling
 * ================================================================ */

static void mark_block_markers(const char *line, int len,
                               CharStyle *styles, BlockType btype)
{
    int i = 0;

    switch (btype) {
    case BLOCK_HEADING:
        while (i < len && line[i] == ' ') i++;
        while (i < len && line[i] == '#') {
            styles[i].attr  = g_dim_attr;
            styles[i].cpair = CP_DIMMED;
            i++;
        }
        if (i < len && line[i] == ' ') {
            styles[i].attr  = g_dim_attr;
            styles[i].cpair = CP_DIMMED;
        }
        break;

    case BLOCK_BLOCKQUOTE:
        while (i < len && line[i] == ' ') i++;
        if (i < len && line[i] == '>') {
            styles[i].cpair = CP_BLOCKQUOTE;
            styles[i].attr  = A_BOLD;
        }
        break;

    case BLOCK_LIST_UNORDERED:
        while (i < len && line[i] == ' ') i++;
        if (i < len) {
            styles[i].cpair = CP_LIST_MARKER;
            styles[i].attr  = A_BOLD;
        }
        break;

    case BLOCK_LIST_ORDERED:
        while (i < len && line[i] == ' ') i++;
        while (i < len && isdigit((unsigned char)line[i])) {
            styles[i].cpair = CP_LIST_MARKER;
            styles[i].attr  = A_BOLD;
            i++;
        }
        if (i < len && (line[i] == '.' || line[i] == ')')) {
            styles[i].cpair = CP_LIST_MARKER;
            styles[i].attr  = A_BOLD;
        }
        break;

    default:
        break;
    }
}

/* ================================================================
 *  Full style computation for a line
 * ================================================================ */

static void compute_styles(const char *line, int len, CharStyle *styles,
                           BlockType btype, int hlevel)
{
    /* Base style from block type */
    attr_t base_attr  = 0;
    short  base_cpair = CP_DEFAULT;

    switch (btype) {
    case BLOCK_HEADING:
        base_attr = A_BOLD;
        switch (hlevel) {
        case 1: base_cpair = CP_HEADING1; break;
        case 2: base_cpair = CP_HEADING2; break;
        case 3: base_cpair = CP_HEADING3; break;
        default: base_cpair = CP_HEADING4; break;
        }
        break;

    case BLOCK_CODE_FENCE:
        for (int i = 0; i < len; i++) {
            styles[i].attr  = g_dim_attr;
            styles[i].cpair = CP_CODE_BLOCK;
        }
        return;

    case BLOCK_CODE_CONTENT:
        for (int i = 0; i < len; i++)
            styles[i].cpair = CP_CODE_BLOCK;
        return;

    case BLOCK_HRULE:
        for (int i = 0; i < len; i++) {
            styles[i].attr  = A_DIM;
            styles[i].cpair = CP_HRULE;
        }
        return;

    case BLOCK_BLOCKQUOTE:
        base_cpair = CP_BLOCKQUOTE;
        break;

    default:
        break;
    }

    for (int i = 0; i < len; i++) {
        styles[i].attr  = base_attr;
        styles[i].cpair = base_cpair;
    }

    /* Inline formatting */
    apply_inline_styles(line, len, styles);

    /* Override block-level markers (# > - etc.) */
    mark_block_markers(line, len, styles, btype);
}

/* ================================================================
 *  Public: render one line to the ncurses screen
 * ================================================================ */

void render_draw_line(int screen_y, int screen_cols,
                      const char *text, int len, int scroll_x,
                      BlockType btype, int hlevel)
{
    CharStyle *styles = calloc(len + 1, sizeof(CharStyle));
    compute_styles(text, len, styles, btype, hlevel);

    move(screen_y, 0);
    clrtoeol();

    int col = 0;
    for (int i = scroll_x; i < len && col < screen_cols; i++, col++) {
        attr_t a = COLOR_PAIR(styles[i].cpair) | styles[i].attr;
        attron(a);
        addch((unsigned char)text[i]);
        attroff(a);
    }

    free(styles);
}
