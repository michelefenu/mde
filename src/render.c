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

/* ================================================================
 *  Preview mode — inline markdown stripping
 * ================================================================ */

/* Run the same multi-pass inline analysis as edit mode, then build a
   new string that omits delimiter characters (claimed == 1). */
static void strip_inline(const char *src, int src_len,
                         attr_t base_attr, short base_cpair,
                         char **out_text, CharStyle **out_styles,
                         int *out_len)
{
    if (src_len <= 0) {
        *out_text   = calloc(1, 1);
        *out_styles = calloc(1, sizeof(CharStyle));
        *out_len    = 0;
        return;
    }

    CharStyle *raw = calloc(src_len + 1, sizeof(CharStyle));
    for (int i = 0; i < src_len; i++) {
        raw[i].attr  = base_attr;
        raw[i].cpair = base_cpair;
    }

    char *claimed = calloc(src_len + 1, 1);
    apply_code_spans(src, src_len, raw, claimed);
    apply_emphasis(src, src_len, raw, claimed, 3);
    apply_emphasis(src, src_len, raw, claimed, 2);
    apply_emphasis(src, src_len, raw, claimed, 1);
    apply_strikethrough(src, src_len, raw, claimed);
    apply_links(src, src_len, raw, claimed);

    char      *text   = malloc(src_len + 1);
    CharStyle *styles = malloc((src_len + 1) * sizeof(CharStyle));
    int        len    = 0;

    for (int i = 0; i < src_len; i++) {
        if (claimed[i] == 1) continue;          /* skip delimiters */
        text[len]   = src[i];
        styles[len] = raw[i];
        if (styles[len].cpair == CP_DIMMED) {   /* safety: no dim leaks */
            styles[len].attr  = base_attr;
            styles[len].cpair = base_cpair;
        }
        len++;
    }
    text[len] = '\0';

    free(raw);
    free(claimed);
    *out_text   = text;
    *out_styles = styles;
    *out_len    = len;
}

/* ================================================================
 *  Preview buffer helpers
 * ================================================================ */

static PreviewLine *pv_add(PreviewBuffer *pb, int source_row, int len)
{
    if (pb->num_lines >= pb->cap_lines) {
        pb->cap_lines = pb->cap_lines ? pb->cap_lines * 2 : 256;
        pb->lines = realloc(pb->lines, sizeof(PreviewLine) * pb->cap_lines);
    }
    PreviewLine *pl = &pb->lines[pb->num_lines++];
    memset(pl, 0, sizeof(*pl));
    pl->source_row = source_row;
    pl->len        = len;
    pl->text       = calloc(len + 1, 1);
    pl->styles     = calloc(len + 1, sizeof(CharStyle));
    return pl;
}

static int pv_fill(PreviewLine *pl, int pos, int n,
                   char ch, attr_t attr, short cpair)
{
    for (int i = 0; i < n; i++) {
        pl->text[pos]         = ch;
        pl->styles[pos].attr  = attr;
        pl->styles[pos].cpair = cpair;
        pos++;
    }
    return pos;
}

/* Copy stripped text + styles into a preview line at offset `pos`. */
static int pv_copy(PreviewLine *pl, int pos,
                   const char *t, const CharStyle *s, int n)
{
    memcpy(pl->text   + pos, t, n);
    memcpy(pl->styles + pos, s, n * sizeof(CharStyle));
    return pos + n;
}

/* Fill N positions with an ACS marker (stored in styles, not text). */
static int pv_fill_acs(PreviewLine *pl, int pos, int n,
                       unsigned char acs_id, attr_t attr, short cpair)
{
    for (int i = 0; i < n; i++) {
        pl->text[pos]         = ' ';
        pl->styles[pos].attr  = attr;
        pl->styles[pos].cpair = cpair;
        pl->styles[pos].acs   = acs_id;
        pos++;
    }
    return pos;
}

/* Set a single position to an ACS marker. */
static void pv_set_acs(PreviewLine *pl, int pos,
                       unsigned char acs_id, attr_t attr, short cpair)
{
    pl->text[pos]         = ' ';
    pl->styles[pos].attr  = attr;
    pl->styles[pos].cpair = cpair;
    pl->styles[pos].acs   = acs_id;
}

/* ================================================================
 *  Preview generators — one per block type
 * ================================================================ */

static void gen_heading(PreviewBuffer *pb, const char *line, int len,
                        int source_row, int screen_cols)
{
    int hlevel = render_heading_level(line);
    int indent = (hlevel <= 1) ? 0 : (hlevel - 1) * 2;
    if (indent > 8) indent = 8;

    short cpair;
    switch (hlevel) {
    case 1:  cpair = CP_HEADING1; break;
    case 2:  cpair = CP_HEADING2; break;
    case 3:  cpair = CP_HEADING3; break;
    default: cpair = CP_HEADING4; break;
    }

    /* Skip past # markers */
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    while (i < len && line[i] == '#') i++;
    if (i < len && line[i] == ' ') i++;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + i, len - i, A_BOLD, cpair, &ct, &cs, &cl);

    int total = indent + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);

    /* H1 gets an underline */
    if (hlevel == 1 && cl > 0) {
        int uw = indent + cl;
        if (uw > screen_cols) uw = screen_cols;
        PreviewLine *ul = pv_add(pb, source_row, uw);
        pv_fill_acs(ul, 0, uw, PM_HLINE, A_BOLD, cpair);
        ul->text[uw] = '\0';
    }
}

static void gen_paragraph(PreviewBuffer *pb, const char *line, int len,
                          int source_row, int body_indent)
{
    char *ct; CharStyle *cs; int cl;
    strip_inline(line, len, 0, CP_DEFAULT, &ct, &cs, &cl);
    int total = body_indent + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
    pv_copy(pl, p, ct, cs, cl);
    pl->text[total] = '\0';
    free(ct); free(cs);
}

static void gen_ulist(PreviewBuffer *pb, const char *line, int len,
                      int source_row, int body_indent)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;
    i++;                                       /* skip marker */
    if (i < len && line[i] == ' ') i++;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + i, len - i, 0, CP_DEFAULT, &ct, &cs, &cl);

    int total = indent + 2 + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);
    pv_set_acs(pl, p, PM_BULLET, A_BOLD, CP_LIST_MARKER);
    p++;
    pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);
}

static void gen_olist(PreviewBuffer *pb, const char *line, int len,
                      int source_row, int body_indent)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;
    int ns = i;
    while (i < len && isdigit((unsigned char)line[i])) i++;
    if (i < len && (line[i] == '.' || line[i] == ')')) i++;
    if (i < len && line[i] == ' ') i++;
    int prefix_len = i - ns;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + i, len - i, 0, CP_DEFAULT, &ct, &cs, &cl);

    int total = indent + prefix_len + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);
    for (int j = 0; j < prefix_len; j++) {
        pl->text[p]         = line[ns + j];
        pl->styles[p].attr  = A_BOLD;
        pl->styles[p].cpair = CP_LIST_MARKER;
        p++;
    }
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);
}

static void gen_blockquote(PreviewBuffer *pb, const char *line, int len,
                           int source_row, int body_indent)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;
    if (i < len && line[i] == '>') i++;
    if (i < len && line[i] == ' ') i++;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + i, len - i, 0, CP_BLOCKQUOTE, &ct, &cs, &cl);

    int total = indent + 2 + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);
    pv_set_acs(pl, p, PM_VLINE, A_BOLD, CP_BLOCKQUOTE);
    p++;
    pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);
}

/* Return the number of display columns a UTF-8 string occupies.
   Assumes all non-ASCII characters are single-column width. */
static int utf8_display_width(const char *s, int len)
{
    int cols = 0;
    for (int i = 0; i < len; ) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80)        { cols++; i += 1; }
        else if (c < 0xC0)   {         i += 1; } /* stray continuation */
        else if (c < 0xE0)   { cols++; i += 2; }
        else if (c < 0xF0)   { cols++; i += 3; }
        else                  { cols++; i += 4; }
    }
    return cols;
}

/* Generate a boxed code block.
   fence_row  = row of the opening ``` line (used for language label).
   content_start .. content_end-1 = rows of actual code content. */
static void gen_code_block(PreviewBuffer *pb, Buffer *buf,
                           int fence_row, int content_start,
                           int content_end, int screen_cols,
                           int body_indent)
{
    /* Extract language label from opening fence */
    char *fence = buffer_line_data(buf, fence_row);
    int flen = buffer_line_len(buf, fence_row);
    int fi = 0;
    while (fi < flen && (fence[fi] == ' ' || fence[fi] == '`' || fence[fi] == '~')) fi++;
    int lang_start = fi;
    while (fi < flen && fence[fi] != ' ' && fence[fi] != '`' && fence[fi] != '~') fi++;
    int lang_len = fi - lang_start;

    /* Find max content display width (columns, not bytes) */
    int max_w = 0;
    for (int r = content_start; r < content_end; r++) {
        int dw = utf8_display_width(buffer_line_data(buf, r),
                                    buffer_line_len(buf, r));
        if (dw > max_w) max_w = dw;
    }

    /* box_w is in display columns */
    int box_w = max_w;
    if (lang_len + 2 > box_w) box_w = lang_len + 2;
    if (box_w < 20) box_w = 20;
    int max_box = screen_cols - body_indent - 4;
    if (max_box > 0 && box_w > max_box) box_w = max_box;

    /* Border total is in display columns (all single-byte chars) */
    int border_total = body_indent + box_w + 4;

    /* ── Top border: ┌─ language ──────┐ ── */
    {
        PreviewLine *pl = pv_add(pb, fence_row, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_ULCORNER, 0, CP_DIMMED); p++;
        if (lang_len > 0) {
            pv_set_acs(pl, p, PM_HLINE, 0, CP_DIMMED); p++;
            pl->text[p] = ' '; pl->styles[p].cpair = CP_DIMMED; p++;
            for (int j = 0; j < lang_len && p < border_total - 1; j++) {
                pl->text[p] = fence[lang_start + j];
                pl->styles[p].attr  = A_BOLD;
                pl->styles[p].cpair = CP_CODE_BLOCK;
                p++;
            }
            pl->text[p] = ' '; pl->styles[p].cpair = CP_DIMMED; p++;
        }
        while (p < border_total - 1) {
            pv_set_acs(pl, p, PM_HLINE, 0, CP_DIMMED); p++;
        }
        pv_set_acs(pl, p, PM_URCORNER, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }

    /* ── Content lines: │ code padded │ ──
       Each line may have multi-byte UTF-8 chars, so its byte count can
       exceed its display width.  We copy all bytes, then pad with spaces
       to fill box_w display columns. */
    for (int r = content_start; r < content_end; r++) {
        char *line = buffer_line_data(buf, r);
        int   len  = buffer_line_len(buf, r);
        int   dw   = utf8_display_width(line, len);
        int   pad  = box_w - dw;
        if (pad < 0) pad = 0;
        /* byte length of this preview line: indent + borders/spaces + content bytes + padding */
        int line_bytes = body_indent + 4 + len + pad;

        PreviewLine *pl = pv_add(pb, r, line_bytes);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        pl->text[p] = ' '; pl->styles[p].cpair = CP_CODE_BLOCK; p++;
        for (int j = 0; j < len; j++) {
            pl->text[p]         = line[j];
            pl->styles[p].cpair = CP_CODE_BLOCK;
            p++;
        }
        p = pv_fill(pl, p, pad, ' ', 0, CP_CODE_BLOCK);
        pl->text[p] = ' '; pl->styles[p].cpair = CP_CODE_BLOCK; p++;
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }

    /* If the code block is empty, add one blank content line */
    if (content_start >= content_end) {
        PreviewLine *pl = pv_add(pb, fence_row, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        p = pv_fill(pl, p, box_w + 2, ' ', 0, CP_CODE_BLOCK);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }

    /* ── Bottom border: └──────────────┘ ── */
    {
        int src = (content_end > content_start) ? content_end - 1 : fence_row;
        PreviewLine *pl = pv_add(pb, src, border_total);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_LLCORNER, 0, CP_DIMMED); p++;
        while (p < border_total - 1) {
            pv_set_acs(pl, p, PM_HLINE, 0, CP_DIMMED); p++;
        }
        pv_set_acs(pl, p, PM_LRCORNER, 0, CP_DIMMED); p++;
        pl->len = p; pl->text[p] = '\0';
    }
}

static void gen_hrule(PreviewBuffer *pb, int source_row, int screen_cols,
                      int body_indent)
{
    int rule_w = (screen_cols > body_indent) ? screen_cols - body_indent : 20;
    int total = body_indent + rule_w;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
    pv_fill_acs(pl, p, rule_w, PM_HLINE, A_DIM, CP_HRULE);
    pl->text[total] = '\0';
}

static void gen_empty(PreviewBuffer *pb, int source_row)
{
    pv_add(pb, source_row, 0);
}

/* ================================================================
 *  Table support
 * ================================================================ */

#define MAX_TBL_COLS 32

typedef struct {
    char *cells[MAX_TBL_COLS];
    int   cell_lens[MAX_TBL_COLS];
    int   num_cells;
    int   is_sep;
} TRow;

static int is_table_line(const char *line, int len)
{
    int pipes = 0;
    for (int i = 0; i < len; i++)
        if (line[i] == '|') pipes++;
    return pipes >= 1;
}

static void parse_table_row(const char *line, int len, TRow *row)
{
    memset(row, 0, sizeof(*row));
    row->is_sep = 1;

    int i = 0;
    while (i < len && line[i] == ' ') i++;
    if (i < len && line[i] == '|') i++;

    while (i < len && row->num_cells < MAX_TBL_COLS) {
        int cs = i;
        while (i < len && line[i] != '|') i++;
        int ce = i;

        while (cs < ce && line[cs] == ' ') cs++;
        while (ce > cs && line[ce - 1] == ' ') ce--;
        int clen = (ce > cs) ? ce - cs : 0;

        row->cells[row->num_cells]     = strndup(line + cs, clen);
        row->cell_lens[row->num_cells] = clen;

        int sep = (clen > 0);
        for (int j = cs; j < ce; j++)
            if (line[j] != '-' && line[j] != ':' && line[j] != ' ')
                { sep = 0; break; }
        if (clen == 0) sep = 0;
        if (!sep) row->is_sep = 0;

        row->num_cells++;
        if (i < len && line[i] == '|') i++;
        else break;
    }
    if (row->num_cells == 0) row->is_sep = 0;
}

static void free_trow(TRow *r)
{
    for (int c = 0; c < r->num_cells; c++) free(r->cells[c]);
}

/* kind: 0 = top  ┌─┬─┐   1 = mid  ├─┼─┤   2 = bot  └─┴─┘ */
static void gen_table_border(PreviewBuffer *pb, int *cw, int ncols,
                             int kind, int source_row, int body_indent)
{
    int total = body_indent + 1;
    for (int c = 0; c < ncols; c++) total += cw[c] + 2 + 1;

    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
    unsigned char lc, mc, rc;
    switch (kind) {
    case 0: lc = PM_ULCORNER; mc = PM_TTEE; rc = PM_URCORNER; break;
    case 1: lc = PM_LTEE;     mc = PM_PLUS; rc = PM_RTEE;     break;
    default:lc = PM_LLCORNER; mc = PM_BTEE; rc = PM_LRCORNER; break;
    }

    pv_set_acs(pl, p, lc, 0, CP_DIMMED); p++;
    for (int c = 0; c < ncols; c++) {
        p = pv_fill_acs(pl, p, cw[c] + 2, PM_HLINE, 0, CP_DIMMED);
        unsigned char sep = (c < ncols - 1) ? mc : rc;
        pv_set_acs(pl, p, sep, 0, CP_DIMMED); p++;
    }
    pl->len = p; pl->text[p] = '\0';
}

static void gen_table_content(PreviewBuffer *pb, TRow *row, int *cw,
                              int ncols, int is_hdr, int source_row,
                              int body_indent)
{
    int total = body_indent + 1;
    for (int c = 0; c < ncols; c++) total += cw[c] + 2 + 1;

    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
    attr_t ca = is_hdr ? A_BOLD : 0;
    short  cc = is_hdr ? CP_HEADING2 : CP_DEFAULT;

    pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
    for (int c = 0; c < ncols; c++) {
        pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
        int cl   = (c < row->num_cells) ? row->cell_lens[c] : 0;
        const char *ct = (c < row->num_cells) ? row->cells[c] : "";
        for (int j = 0; j < cw[c]; j++) {
            pl->text[p]         = (j < cl) ? ct[j] : ' ';
            pl->styles[p].attr  = ca;
            pl->styles[p].cpair = cc;
            p++;
        }
        pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
    }
    pl->len = p; pl->text[p] = '\0';
}

static void gen_table_block(PreviewBuffer *pb, Buffer *buf,
                            int start, int end, int screen_cols,
                            int body_indent)
{
    int nr = end - start;
    TRow *rows = calloc(nr, sizeof(TRow));
    int max_cols = 0;

    for (int i = 0; i < nr; i++) {
        parse_table_row(buffer_line_data(buf, start + i),
                        buffer_line_len(buf, start + i), &rows[i]);
        if (rows[i].num_cells > max_cols)
            max_cols = rows[i].num_cells;
    }

    int cw[MAX_TBL_COLS];
    memset(cw, 0, sizeof(cw));
    for (int i = 0; i < nr; i++) {
        if (rows[i].is_sep) continue;
        for (int c = 0; c < rows[i].num_cells; c++)
            if (rows[i].cell_lens[c] > cw[c])
                cw[c] = rows[i].cell_lens[c];
    }
    for (int c = 0; c < max_cols; c++)
        if (cw[c] < 3) cw[c] = 3;

    (void)screen_cols;

    gen_table_border(pb, cw, max_cols, 0, start, body_indent);

    int found_sep = 0;
    for (int i = 0; i < nr; i++) {
        if (rows[i].is_sep) {
            gen_table_border(pb, cw, max_cols, 1, start + i, body_indent);
            found_sep = 1;
        } else {
            gen_table_content(pb, &rows[i], cw, max_cols,
                              !found_sep, start + i, body_indent);
        }
    }

    gen_table_border(pb, cw, max_cols, 2, end - 1, body_indent);

    for (int i = 0; i < nr; i++) free_trow(&rows[i]);
    free(rows);
}

/* ================================================================
 *  Public preview API
 * ================================================================ */

void preview_generate(PreviewBuffer *pb, Buffer *buf, int screen_cols)
{
    preview_free(pb);

    int in_code = 0;
    int body_indent = 0;
    int row = 0;

    while (row < buf->num_lines) {
        char     *line = buffer_line_data(buf, row);
        int       len  = buffer_line_len(buf, row);
        BlockType bt   = render_get_block_type(line, in_code);

        if (bt == BLOCK_CODE_FENCE) {
            int fence_open = row;
            row++;                          /* skip opening fence */
            int content_start = row;
            int found_close = 0;
            while (row < buf->num_lines) {
                char *cl = buffer_line_data(buf, row);
                if (render_is_code_fence(cl)) {
                    found_close = 1;
                    break;
                }
                row++;
            }
            int content_end = row;          /* exclusive */
            gen_code_block(pb, buf, fence_open, content_start,
                           content_end, screen_cols, body_indent);
            if (found_close) row++;         /* skip closing fence */
            continue;
        }

        /* Detect table blocks (consecutive paragraph lines with |) */
        if (bt == BLOCK_PARAGRAPH && is_table_line(line, len)) {
            int ts = row;
            while (row < buf->num_lines) {
                char *tl  = buffer_line_data(buf, row);
                int   tln = buffer_line_len(buf, row);
                BlockType tbt = render_get_block_type(tl, in_code);
                if (tbt != BLOCK_PARAGRAPH || !is_table_line(tl, tln))
                    break;
                row++;
            }
            gen_table_block(pb, buf, ts, row, screen_cols, body_indent);
            continue;
        }

        switch (bt) {
        case BLOCK_HEADING:
            gen_heading(pb, line, len, row, screen_cols);
            { int hl = render_heading_level(line);
              body_indent = (hl <= 1) ? 0 : (hl - 1) * 2;
              if (body_indent > 8) body_indent = 8; }
            break;
        case BLOCK_HRULE:
            gen_hrule(pb, row, screen_cols, body_indent); break;
        case BLOCK_LIST_UNORDERED:
            gen_ulist(pb, line, len, row, body_indent); break;
        case BLOCK_LIST_ORDERED:
            gen_olist(pb, line, len, row, body_indent); break;
        case BLOCK_BLOCKQUOTE:
            gen_blockquote(pb, line, len, row, body_indent); break;
        case BLOCK_CODE_CONTENT:
            /* Shouldn't happen — code blocks are collected above.
               Fall through to paragraph as a safety net. */
            gen_paragraph(pb, line, len, row, body_indent); break;
        case BLOCK_EMPTY:
            gen_empty(pb, row); break;
        default:
            gen_paragraph(pb, line, len, row, body_indent); break;
        }
        row++;
    }
}

void preview_free(PreviewBuffer *pb)
{
    for (int i = 0; i < pb->num_lines; i++) {
        free(pb->lines[i].text);
        free(pb->lines[i].styles);
    }
    free(pb->lines);
    memset(pb, 0, sizeof(*pb));
}

void preview_draw_line(int screen_y, int screen_cols,
                       PreviewLine *pl, int scroll_x)
{
    move(screen_y, 0);
    clrtoeol();

    int col = 0;
    for (int i = scroll_x; i < pl->len && col < screen_cols; i++, col++) {
        attr_t a = COLOR_PAIR(pl->styles[i].cpair) | pl->styles[i].attr;
        if (pl->styles[i].acs) {
            chtype acs;
            switch (pl->styles[i].acs) {
            case PM_VLINE:    acs = ACS_VLINE;    break;
            case PM_HLINE:    acs = ACS_HLINE;    break;
            case PM_ULCORNER: acs = ACS_ULCORNER; break;
            case PM_URCORNER: acs = ACS_URCORNER; break;
            case PM_LLCORNER: acs = ACS_LLCORNER; break;
            case PM_LRCORNER: acs = ACS_LRCORNER; break;
            case PM_LTEE:     acs = ACS_LTEE;     break;
            case PM_RTEE:     acs = ACS_RTEE;     break;
            case PM_TTEE:     acs = ACS_TTEE;     break;
            case PM_BTEE:     acs = ACS_BTEE;     break;
            case PM_PLUS:     acs = ACS_PLUS;     break;
            case PM_BULLET:   acs = ACS_BULLET;   break;
            default:          acs = '?';           break;
            }
            addch(acs | a);
        } else {
            addch((unsigned char)pl->text[i] | a);
        }
    }
}

int preview_find_line(PreviewBuffer *pb, int buffer_row)
{
    for (int i = 0; i < pb->num_lines; i++)
        if (pb->lines[i].source_row >= buffer_row)
            return i;
    return (pb->num_lines > 0) ? pb->num_lines - 1 : 0;
}
