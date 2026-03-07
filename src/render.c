#include "render.h"
#include "render_table.h"
#include "utf8.h"
#include "xalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Convert a byte offset to a display column offset.
   Both text and byte_pos are in raw bytes. */
int render_byte_to_col(const char *text, int len, int byte_pos)
{
    int col = 0;
    int i = 0;
    while (i < byte_pos && i < len) {
        int clen = utf8_clen((unsigned char)text[i]);
        if (i + clen > len) clen = len - i;
        i += clen;
        col++;
    }
    return col;
}

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
    init_pair(CP_TODO_OPEN,   COLOR_GREEN,   -1);
    init_pair(CP_TODO_DONE,   dim_fg,        -1);
    init_pair(CP_TODO_META,   COLOR_MAGENTA, -1);
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
                        CharStyle *styles, char *claimed,
                        int *link_idx_at, int *next_link_idx)
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

        /* Record link index for preview (skip images) */
        if (link_idx_at && next_link_idx && !img_bang)
            link_idx_at[bracket_end - 1] = ++(*next_link_idx);

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
    apply_links(text, len, styles, claimed, NULL, NULL);

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
 *  Todo item helpers
 * ================================================================ */

/* Returns 1 if the line is a GFM-style todo item: - [ ] or - [x]
   Sets *is_done, *cb_start (byte offset of '['), *text_start (offset
   of text content after "- [x] "). */
static int parse_todo_item(const char *line, int len,
                           int *is_done, int *cb_start, int *text_start)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    if (i >= len || (line[i] != '-' && line[i] != '*' && line[i] != '+'))
        return 0;
    i++;
    if (i >= len || line[i] != ' ') return 0;
    i++;
    if (i + 2 >= len || line[i] != '[') return 0;
    if (line[i+1] != ' ' && line[i+1] != 'x' && line[i+1] != 'X') return 0;
    if (line[i+2] != ']') return 0;

    *is_done   = (line[i+1] == 'x' || line[i+1] == 'X');
    *cb_start  = i;
    i += 3;
    if (i < len && line[i] == ' ') i++;
    *text_start = i;
    return 1;
}

/* Color metadata tokens (~dur #tag @name yyyy-mm-dd) in a char array.
   Works on both raw source bytes (edit mode) and stripped preview bytes. */
static void apply_todo_meta_styles(const char *text, int len, CharStyle *styles)
{
    for (int j = 0; j < len; j++) {
        char c = text[j];
        if (c == '~' || c == '#' || c == '@') {
            int start = j;
            j++;
            while (j < len && (isalnum((unsigned char)text[j]) ||
                                text[j] == '-' || text[j] == '_'))
                j++;
            if (j > start + 1) {
                for (int k = start; k < j; k++) {
                    styles[k].attr  = A_BOLD;
                    styles[k].cpair = CP_TODO_META;
                }
            }
            j--;
        } else if (isdigit((unsigned char)c) && j + 10 <= len) {
            /* yyyy-mm-dd */
            if (isdigit((unsigned char)text[j+1]) &&
                isdigit((unsigned char)text[j+2]) &&
                isdigit((unsigned char)text[j+3]) &&
                text[j+4] == '-' &&
                isdigit((unsigned char)text[j+5]) &&
                isdigit((unsigned char)text[j+6]) &&
                text[j+7] == '-' &&
                isdigit((unsigned char)text[j+8]) &&
                isdigit((unsigned char)text[j+9])) {
                int at_word_boundary = (j == 0 || text[j-1] == ' ');
                int after_ok = (j + 10 >= len || text[j+10] == ' ');
                if (at_word_boundary && after_ok) {
                    for (int k = j; k < j + 10; k++) {
                        styles[k].attr  = A_BOLD;
                        styles[k].cpair = CP_TODO_META;
                    }
                    j += 9;
                }
            }
        }
    }
}

/* Edit-mode: color the checkbox + metadata on a todo list line. */
static void apply_todo_styles(const char *line, int len, CharStyle *styles)
{
    int is_done, cb_start, text_start;
    if (!parse_todo_item(line, len, &is_done, &cb_start, &text_start))
        return;

    /* Style the [ ] / [x] checkbox */
    short cb_cpair = is_done ? CP_TODO_DONE : CP_TODO_OPEN;
    attr_t cb_attr = is_done ? A_DIM : A_BOLD;
    for (int j = cb_start; j < cb_start + 3 && j < len; j++) {
        styles[j].attr  = cb_attr;
        styles[j].cpair = cb_cpair;
    }

    /* For completed tasks, dim the whole content area */
    if (is_done) {
        for (int j = text_start; j < len; j++) {
            styles[j].attr  = A_DIM;
            styles[j].cpair = CP_TODO_DONE;
        }
    }

    /* Highlight metadata tokens in content area */
    apply_todo_meta_styles(line + text_start, len - text_start,
                           styles + text_start);
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

    /* Todo-specific styling (checkbox + metadata tokens) */
    if (btype == BLOCK_LIST_UNORDERED)
        apply_todo_styles(line, len, styles);
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
    int i = scroll_x;
    while (i < len && col < screen_cols) {
        attr_t a = COLOR_PAIR(styles[i].cpair) | styles[i].attr;
        int clen = utf8_clen((unsigned char)text[i]);
        if (i + clen > len) clen = len - i;

        attron(a);
        addnstr(text + i, clen);
        attroff(a);

        i += clen;
        col++;
    }

    free(styles);
}

/* ================================================================
 *  Word-wrap helpers
 * ================================================================ */

int render_wrap_height(const char *text, int len, int cols)
{
    if (cols <= 0) return 1;
    int dw = render_byte_to_col(text, len, len);
    if (dw == 0) return 1;
    return (dw + cols - 1) / cols;
}

int render_draw_line_wrapped(int screen_y, int screen_cols,
                             const char *text, int len,
                             BlockType btype, int hlevel,
                             int max_rows)
{
    CharStyle *styles = calloc(len + 1, sizeof(CharStyle));
    compute_styles(text, len, styles, btype, hlevel);

    int row = 0;      /* current wrapped sub-row */
    int col = 0;      /* current column within sub-row */
    int i   = 0;      /* byte offset into text */

    move(screen_y, 0);
    clrtoeol();

    while (i < len && row < max_rows) {
        attr_t a = COLOR_PAIR(styles[i].cpair) | styles[i].attr;
        int clen = utf8_clen((unsigned char)text[i]);
        if (i + clen > len) clen = len - i;

        /* If this character would exceed the screen width, wrap */
        if (col >= screen_cols) {
            row++;
            col = 0;
            if (row >= max_rows) break;
            move(screen_y + row, 0);
            clrtoeol();
        }

        attron(a);
        addnstr(text + i, clen);
        attroff(a);

        i += clen;
        col++;
    }

    /* If nothing was drawn (empty line), we still used 1 row */
    free(styles);
    return (row < max_rows) ? row + 1 : max_rows;
}

/* ================================================================
 *  Preview mode — inline markdown stripping
 * ================================================================ */

/* Run the same multi-pass inline analysis as edit mode, then build a
   new string that omits delimiter characters (claimed == 1).
   next_link_idx: when non-NULL, link indices (1,2,3...) are appended
   after each link for preview mode; caller must persist across blocks. */
static void strip_inline(const char *src, int src_len,
                         attr_t base_attr, short base_cpair,
                         char **out_text, CharStyle **out_styles,
                         int *out_len, int *next_link_idx)
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
    int  *link_idx_at = calloc(src_len + 1, sizeof(int));
    apply_code_spans(src, src_len, raw, claimed);
    apply_emphasis(src, src_len, raw, claimed, 3);
    apply_emphasis(src, src_len, raw, claimed, 2);
    apply_emphasis(src, src_len, raw, claimed, 1);
    apply_strikethrough(src, src_len, raw, claimed);
    apply_links(src, src_len, raw, claimed, link_idx_at, next_link_idx);

    int max_len = src_len + 256;
    char      *text   = xmalloc(max_len + 1);
    CharStyle *styles = xmalloc((max_len + 1) * sizeof(CharStyle));
    int        len    = 0;

    for (int i = 0; i < src_len; i++) {
        if (claimed[i] == 1) continue;          /* skip delimiters */
        if (len >= max_len) break;
        text[len]   = src[i];
        styles[len] = raw[i];
        if (styles[len].cpair == CP_DIMMED) {   /* safety: no dim leaks */
            styles[len].attr  = base_attr;
            styles[len].cpair = base_cpair;
        }
        len++;
        /* Append link index (N) after link text in preview */
        if (link_idx_at[i] != 0 && len + 16 < max_len) {
            char buf[16];
            int n = snprintf(buf, sizeof(buf), " (%d)", link_idx_at[i]);
            for (int k = 0; k < n; k++) {
                text[len] = buf[k];
                styles[len].attr  = base_attr;
                styles[len].cpair = CP_DIMMED;
                len++;
            }
        }
    }
    text[len] = '\0';

    free(raw);
    free(claimed);
    free(link_idx_at);
    *out_text   = text;
    *out_styles = styles;
    *out_len    = len;
}

/* ================================================================
 *  Preview buffer helpers
 * ================================================================ */

PreviewLine *pv_add(PreviewBuffer *pb, int source_row, int len)
{
    if (pb->num_lines >= pb->cap_lines) {
        pb->cap_lines = pb->cap_lines ? pb->cap_lines * 2 : 256;
        pb->lines = xrealloc(pb->lines, sizeof(PreviewLine) * pb->cap_lines);
    }
    PreviewLine *pl = &pb->lines[pb->num_lines++];
    memset(pl, 0, sizeof(*pl));
    pl->source_row = source_row;
    pl->len        = len;
    pl->text       = calloc(len + 1, 1);
    pl->styles     = calloc(len + 1, sizeof(CharStyle));
    return pl;
}

int pv_fill(PreviewLine *pl, int pos, int n,
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
int pv_copy(PreviewLine *pl, int pos,
            const char *t, const CharStyle *s, int n)
{
    memcpy(pl->text   + pos, t, n);
    memcpy(pl->styles + pos, s, n * sizeof(CharStyle));
    return pos + n;
}

/* Fill N positions with an ACS marker (stored in styles, not text). */
int pv_fill_acs(PreviewLine *pl, int pos, int n,
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
void pv_set_acs(PreviewLine *pl, int pos,
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
                        int source_row, int screen_cols, int *link_idx)
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
    strip_inline(line + i, len - i, A_BOLD, cpair, &ct, &cs, &cl, link_idx);

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
                          int source_row, int body_indent, int *link_idx)
{
    char *ct; CharStyle *cs; int cl;
    strip_inline(line, len, 0, CP_DEFAULT, &ct, &cs, &cl, link_idx);
    int total = body_indent + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
    pv_copy(pl, p, ct, cs, cl);
    pl->text[total] = '\0';
    free(ct); free(cs);
}

static void gen_ulist(PreviewBuffer *pb, const char *line, int len,
                      int source_row, int body_indent, int *link_idx)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;
    i++;                                       /* skip marker */
    if (i < len && line[i] == ' ') i++;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + i, len - i, 0, CP_DEFAULT, &ct, &cs, &cl, link_idx);

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
                      int source_row, int body_indent, int *link_idx)
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
    strip_inline(line + i, len - i, 0, CP_DEFAULT, &ct, &cs, &cl, link_idx);

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
                           int source_row, int body_indent, int *link_idx)
{
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;
    if (i < len && line[i] == '>') i++;
    if (i < len && line[i] == ' ') i++;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + i, len - i, 0, CP_BLOCKQUOTE, &ct, &cs, &cl, link_idx);

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

static void gen_todo(PreviewBuffer *pb, const char *line, int len,
                     int source_row, int body_indent, int *link_idx,
                     int is_done, int text_start)
{
    /* Count leading spaces to compute indent level */
    int i = 0;
    while (i < len && line[i] == ' ') i++;
    int indent = body_indent + i;

    /* Unicode ballot boxes: ☐ (U+2610) = open, ☑ (U+2611) = done */
    static const char box_open[] = "\xe2\x98\x90"; /* ☐ */
    static const char box_done[] = "\xe2\x98\x91"; /* ☑ */
    const char *box   = is_done ? box_done : box_open;
    int         boxsz = 3; /* UTF-8 byte length */

    short  cb_cpair = is_done ? CP_TODO_DONE : CP_TODO_OPEN;
    attr_t cb_attr  = is_done ? A_DIM : A_BOLD;
    attr_t base_attr  = is_done ? A_DIM : 0;
    short  base_cpair = is_done ? CP_TODO_DONE : CP_DEFAULT;

    char *ct; CharStyle *cs; int cl;
    strip_inline(line + text_start, len - text_start,
                 base_attr, base_cpair, &ct, &cs, &cl, link_idx);

    /* Color metadata tokens in the stripped content */
    apply_todo_meta_styles(ct, cl, cs);

    /* byte total: indent spaces + checkbox(3 bytes) + space + content */
    int total = indent + boxsz + 1 + cl;
    PreviewLine *pl = pv_add(pb, source_row, total);
    int p = pv_fill(pl, 0, indent, ' ', 0, CP_DEFAULT);

    /* Store checkbox glyph bytes; styles[p+1] and [p+2] are intermediate
       continuation bytes — draw routine skips them via utf8_clen. */
    for (int j = 0; j < boxsz; j++) {
        pl->text[p]         = box[j];
        pl->styles[p].attr  = cb_attr;
        pl->styles[p].cpair = cb_cpair;
        pl->styles[p].acs   = 0;
        p++;
    }

    pl->text[p] = ' '; pl->styles[p].cpair = CP_DEFAULT; p++;
    p = pv_copy(pl, p, ct, cs, cl);
    pl->len = p; pl->text[p] = '\0';
    free(ct); free(cs);
}

/* ================================================================
 *  Public preview API
 * ================================================================ */

void preview_generate(PreviewBuffer *pb, Buffer *buf, int screen_cols)
{
    preview_free(pb);

    int in_code = 0;
    int body_indent = 0;
    int link_idx = 0;
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
            gen_heading(pb, line, len, row, screen_cols, &link_idx);
            { int hl = render_heading_level(line);
              body_indent = (hl <= 1) ? 0 : (hl - 1) * 2;
              if (body_indent > 8) body_indent = 8; }
            break;
        case BLOCK_HRULE:
            gen_hrule(pb, row, screen_cols, body_indent); break;
        case BLOCK_LIST_UNORDERED: {
            int is_done, cb_start, text_start;
            if (parse_todo_item(line, len, &is_done, &cb_start, &text_start))
                gen_todo(pb, line, len, row, body_indent, &link_idx,
                         is_done, text_start);
            else
                gen_ulist(pb, line, len, row, body_indent, &link_idx);
            break;
        }
        case BLOCK_LIST_ORDERED:
            gen_olist(pb, line, len, row, body_indent, &link_idx); break;
        case BLOCK_BLOCKQUOTE:
            gen_blockquote(pb, line, len, row, body_indent, &link_idx); break;
        case BLOCK_CODE_CONTENT:
            /* Shouldn't happen — code blocks are collected above.
               Fall through to paragraph as a safety net. */
            gen_paragraph(pb, line, len, row, body_indent, &link_idx); break;
        case BLOCK_EMPTY:
            gen_empty(pb, row); break;
        default:
            gen_paragraph(pb, line, len, row, body_indent, &link_idx); break;
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
    int i = scroll_x;
    while (i < pl->len && col < screen_cols) {
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
            i++;
        } else {
            int clen = utf8_clen((unsigned char)pl->text[i]);
            if (i + clen > pl->len) clen = pl->len - i;
            attron(a);
            addnstr(pl->text + i, clen);
            attroff(a);
            i += clen;
        }
        col++;
    }
}

/* Count display columns (characters) in a PreviewLine */
static int preview_display_width(PreviewLine *pl)
{
    int col = 0;
    int i = 0;
    while (i < pl->len) {
        if (pl->styles[i].acs) {
            i++;
        } else {
            int clen = utf8_clen((unsigned char)pl->text[i]);
            if (i + clen > pl->len) clen = pl->len - i;
            i += clen;
        }
        col++;
    }
    return col;
}

int preview_wrap_height(PreviewLine *pl, int cols)
{
    if (cols <= 0) return 1;
    int dw = preview_display_width(pl);
    if (dw == 0) return 1;
    return (dw + cols - 1) / cols;
}

int preview_draw_line_wrapped(int screen_y, int screen_cols,
                              PreviewLine *pl, int max_rows)
{
    int row = 0;
    int col = 0;
    int i   = 0;

    move(screen_y, 0);
    clrtoeol();

    while (i < pl->len && row < max_rows) {
        if (col >= screen_cols) {
            row++;
            col = 0;
            if (row >= max_rows) break;
            move(screen_y + row, 0);
            clrtoeol();
        }

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
            i++;
        } else {
            int clen = utf8_clen((unsigned char)pl->text[i]);
            if (i + clen > pl->len) clen = pl->len - i;
            attron(a);
            addnstr(pl->text + i, clen);
            attroff(a);
            i += clen;
        }
        col++;
    }

    return (row < max_rows) ? row + 1 : max_rows;
}

int preview_find_line(PreviewBuffer *pb, int buffer_row)
{
    for (int i = 0; i < pb->num_lines; i++)
        if (pb->lines[i].source_row >= buffer_row)
            return i;
    return (pb->num_lines > 0) ? pb->num_lines - 1 : 0;
}
