/* render — Block/inline markdown rendering to ncurses, preview buffer construction. */
#include "render.h"
#include "render_table.h"
#include "render_olist.h"
#include "render_ulist.h"
#include "render_todo.h"
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
        col += utf8_char_width(text, len, i);
        i += clen;
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

/* The `claimed` array (one byte per source character) tracks which
   characters have been processed by a prior inline pass:
     0 = unclaimed, available for parsing
     1 = delimiter (*, _, `, ~, [, ], (, )) — dimmed in edit mode,
         stripped entirely in preview mode (strip_inline skips these)
     2 = styled content (code span body, link text) — keeps its style,
         later passes may add attributes but won't re-parse as delimiter */

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

        /* Skip * runs that immediately follow '/': these are C comment openers
           and must not be parsed as emphasis delimiters. */
        if (ch == '*' && start > 0 && text[start - 1] == '/') { i += n; continue; }

        int close = find_close_delim(text, len, start + dcount,
                                     ch, dcount, claimed);
        if (close < 0) { i += n; continue; }
        if (close == start + dcount) { i += n; continue; } /* empty span */

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
        if (link_idx_at && next_link_idx && !img_bang) {
            int is_anchor = (bracket_end + 2 < paren_end &&
                             text[bracket_end + 2] == '#');
            int idx = ++(*next_link_idx);
            link_idx_at[bracket_end - 1] = is_anchor ? -idx : idx;
        }

        i = paren_end + 1;
    }
}

/* Orchestrate all inline passes.
   Code spans MUST run first: backtick-enclosed content is protected
   from emphasis/link parsing by marking it as claimed. */
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
 *  Todo item helpers (edit-mode styling -- parse/meta moved to render_todo.c)
 * ================================================================ */

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
 *  Content indent for hanging-indent wrapping
 * ================================================================ */

/* Returns the display-column width of the prefix for list/blockquote lines,
   used for hanging indent in word-wrap mode. */
int render_line_content_indent(const char *text, int len, BlockType btype)
{
    int i = 0;
    switch (btype) {
    case BLOCK_LIST_UNORDERED:
        /* skip leading spaces + marker char + space */
        while (i < len && text[i] == ' ') i++;
        if (i < len && (text[i] == '-' || text[i] == '*' || text[i] == '+')) {
            i++;
            if (i < len && text[i] == ' ') i++;
        }
        return render_byte_to_col(text, len, i);
    case BLOCK_LIST_ORDERED:
        /* skip leading spaces + digits + '.'/')' + space */
        while (i < len && text[i] == ' ') i++;
        while (i < len && isdigit((unsigned char)text[i])) i++;
        if (i < len && (text[i] == '.' || text[i] == ')')) {
            i++;
            if (i < len && text[i] == ' ') i++;
        }
        return render_byte_to_col(text, len, i);
    case BLOCK_BLOCKQUOTE:
        /* skip leading spaces + '>' + space */
        while (i < len && text[i] == ' ') i++;
        if (i < len && text[i] == '>') {
            i++;
            if (i < len && text[i] == ' ') i++;
        }
        return render_byte_to_col(text, len, i);
    default:
        return 0;
    }
}

/* ================================================================
 *  Word-wrap helpers
 * ================================================================ */

/* Returns the end byte (exclusive) for the current wrapped row.
   Sets *next_start to where the next row begins (past the break space).
   Falls back to mid-word break if no space fits in cols_avail. */
static int find_word_break(const char *text, int start, int len,
                            int cols_avail, int *next_start)
{
    int col = 0, last_space = -1, i = start;
    while (i < len) {
        if (text[i] == ' ') last_space = i;
        int clen = utf8_clen((unsigned char)text[i]);
        if (i + clen > len) clen = len - i;
        if (col >= cols_avail) {
            if (last_space > start) { *next_start = last_space + 1; return last_space; }
            *next_start = i; return i;  /* forced break */
        }
        col += utf8_char_width(text, len, i);
        i += clen;
    }
    *next_start = len;
    return len;
}

int render_wrap_height(const char *text, int len, int cols,
                       int content_indent)
{
    if (cols <= 0) return 1;
    int rows = 0, i = 0;
    do {
        int avail = (rows == 0) ? cols : cols - content_indent;
        if (avail <= 0) avail = 1;
        int next;
        find_word_break(text, i, len, avail, &next);
        rows++; i = next;
    } while (i < len);
    return rows;
}

void render_wrap_cursor_pos(const char *text, int len, int cols,
                             int content_indent, int cx,
                             int *out_row, int *out_col)
{
    int row = 0, i = 0;
    do {
        int avail = (row == 0) ? cols : cols - content_indent;
        if (avail <= 0) avail = 1;
        int next, end = find_word_break(text, i, len, avail, &next);
        if (cx <= end || next >= len) {
            *out_row = row;
            *out_col = render_byte_to_col(text, len, cx)
                     - render_byte_to_col(text, len, i);
            if (row > 0) *out_col += content_indent;
            return;
        }
        i = next; row++;
    } while (i < len);
    *out_row = row; *out_col = content_indent;
}

int render_draw_line_wrapped(int screen_y, int screen_cols,
                             const char *text, int len,
                             BlockType btype, int hlevel,
                             int max_rows, int content_indent)
{
    CharStyle *styles = calloc(len + 1, sizeof(CharStyle));
    compute_styles(text, len, styles, btype, hlevel);

    int row = 0;
    int i   = 0;

    do {
        int avail = (row == 0) ? screen_cols : screen_cols - content_indent;
        if (avail <= 0) avail = 1;
        int next, end = find_word_break(text, i, len, avail, &next);
        move(screen_y + row, 0);
        clrtoeol();

        /* draw indent spaces on continuation rows */
        if (row > 0 && content_indent > 0)
            for (int k = 0; k < content_indent; k++) addch(' ');

        /* draw segment [i, end) character by character with styles */
        int j = i;
        while (j < end) {
            attr_t a = COLOR_PAIR(styles[j].cpair) | styles[j].attr;
            int clen = utf8_clen((unsigned char)text[j]);
            if (j + clen > end) clen = end - j;
            attron(a);
            addnstr(text + j, clen);
            attroff(a);
            j += clen;
        }

        i = next;
        row++;
    } while (i < len && row < max_rows);

    free(styles);
    return (row < max_rows) ? row : max_rows;
}

/* ================================================================
 *  Preview mode — inline markdown stripping
 * ================================================================ */

/* Run the same multi-pass inline analysis as edit mode, then build a
   new string that omits delimiter characters (claimed == 1).
   next_link_idx: when non-NULL, link indices (1,2,3...) are appended
   after each link for preview mode; caller must persist across blocks. */
void strip_inline(const char *src, int src_len,
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
            int raw_idx = link_idx_at[i];
            int n = (raw_idx < 0)
                ? snprintf(buf, sizeof(buf), " [%d]", -raw_idx)
                : snprintf(buf, sizeof(buf), " (%d)", raw_idx);
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
        int clen = utf8_clen((unsigned char)s[i]);
        if (i + clen > len) clen = len - i;
        cols += utf8_char_width(s, len, i);
        i += clen;
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

        /* Truncate content to box_w display columns so the right border
           always aligns, even when a line is wider than the box. */
        int write_len = len;
        int content_dw = dw;
        if (dw > box_w) {
            write_len  = 0;
            content_dw = 0;
            while (write_len < len) {
                int clen = utf8_char_bytes(line, len, write_len);
                int w    = utf8_char_width(line, len, write_len);
                if (content_dw + w > box_w) break;
                content_dw += w;
                write_len  += clen;
            }
        }

        int pad = box_w - content_dw;
        /* byte length of this preview line: indent + borders/spaces + content bytes + padding */
        int line_bytes = body_indent + 4 + write_len + pad;

        PreviewLine *pl = pv_add(pb, r, line_bytes);
        int p = pv_fill(pl, 0, body_indent, ' ', 0, CP_DEFAULT);
        pv_set_acs(pl, p, PM_VLINE, 0, CP_DIMMED); p++;
        pl->text[p] = ' '; pl->styles[p].cpair = CP_CODE_BLOCK; p++;
        for (int j = 0; j < write_len; j++) {
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
 *  Public preview API
 * ================================================================ */

void preview_generate(PreviewBuffer *pb, Buffer *buf, int screen_cols)
{
    preview_free(pb);

    int in_code = 0;
    int body_indent = 0;
    int link_idx = 0;
    int row = 0;
    int olist_active = 0, olist_counter = 0;

    while (row < buf->num_lines) {
        char     *line = buffer_line_data(buf, row);
        int       len  = buffer_line_len(buf, row);
        BlockType bt   = render_get_block_type(line, in_code);

        /* Reset ordered-list counter when leaving a run */
        if (bt != BLOCK_LIST_ORDERED)
            olist_active = 0;

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
            gen_table_block(pb, buf, ts, row, screen_cols, body_indent, &link_idx);
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
        case BLOCK_LIST_ORDERED: {
            if (!olist_active) {
                /* First item: parse its number as the starting value */
                int i = 0;
                while (i < len && line[i] == ' ') i++;
                olist_counter = 0;
                while (i < len && isdigit((unsigned char)line[i])) {
                    olist_counter = olist_counter * 10 + (line[i] - '0');
                    i++;
                }
                olist_active = 1;
            } else {
                olist_counter++;
            }
            gen_olist(pb, line, len, row, body_indent, &link_idx,
                      olist_counter);
            break;
        }
        case BLOCK_BLOCKQUOTE:
            gen_blockquote(pb, line, len, row, body_indent, &link_idx); break;
        case BLOCK_CODE_CONTENT:
            /* Shouldn't happen -- code blocks are collected above.
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
            col += utf8_char_width(pl->text, pl->len, i);
            i += clen;
        }
    }
}


/* Same logic as find_word_break but for PreviewLine (never break on ACS markers). */
static int find_preview_word_break(PreviewLine *pl, int start,
                                    int cols_avail, int *next_start)
{
    int col = 0, last_space = -1, i = start;
    while (i < pl->len) {
        if (!pl->styles[i].acs && pl->text[i] == ' ') last_space = i;
        int clen = pl->styles[i].acs ? 1
                 : utf8_clen((unsigned char)pl->text[i]);
        if (i + clen > pl->len) clen = pl->len - i;
        if (col >= cols_avail) {
            if (last_space > start) { *next_start = last_space + 1; return last_space; }
            *next_start = i; return i;
        }
        col += pl->styles[i].acs ? 1 : utf8_char_width(pl->text, pl->len, i);
        i += clen;
    }
    *next_start = pl->len;
    return pl->len;
}

/* Count leading prefix width for hanging-indent in preview mode.
   Recognises three preview prefix patterns:
     [spaces] ACS_BULLET ' '   (unordered list)
     [spaces] ACS_VLINE  ' '   (blockquote)
     [spaces] digits '.'/')' ' '  (ordered list)
   Returns 0 (no indent) for plain paragraphs. */
static int preview_leading_indent(PreviewLine *pl)
{
    int i = 0;
    /* 1. Leading spaces */
    while (i < pl->len && !pl->styles[i].acs && pl->text[i] == ' ')
        i++;
    int prefix_start = i;
    /* 2. ACS marker (bullet or vline) */
    if (i < pl->len && pl->styles[i].acs) {
        i++;
    }
    /* 2b. Or ordered-list marker: digits + '.'/')' */
    else if (i < pl->len && isdigit((unsigned char)pl->text[i])) {
        while (i < pl->len && isdigit((unsigned char)pl->text[i])) i++;
        if (i < pl->len && (pl->text[i] == '.' || pl->text[i] == ')')) i++;
        else return 0;  /* not actually an ordered list marker */
    }
    else {
        return prefix_start;  /* plain paragraph — indent to leading spaces */
    }
    /* 3. One trailing space after the marker */
    if (i < pl->len && pl->text[i] == ' ' && !pl->styles[i].acs)
        i++;
    /* Sanity: must have advanced past prefix_start */
    return (i > prefix_start) ? i : 0;
}

/* Returns 1 if the line is a table row.
   Table borders use HLINE/corners/tees; table content rows have multiple
   PM_VLINE (column separators).  Blockquotes have exactly one PM_VLINE
   (the left bar) and list items have a single PM_BULLET. */
static int preview_line_is_table(PreviewLine *pl)
{
    int vline_count = 0;
    for (int i = 0; i < pl->len; i++) {
        unsigned char acs = pl->styles[i].acs;
        if (!acs) continue;
        if (acs != PM_VLINE && acs != PM_BULLET)
            return 1;  /* HLINE, corner, tee → definitely a table */
        if (acs == PM_VLINE)
            vline_count++;
    }
    return vline_count >= 2;  /* multiple vlines → table content row */
}

int preview_wrap_height(PreviewLine *pl, int cols)
{
    if (cols <= 0) return 1;
    if (preview_line_is_table(pl)) return 1;
    int indent = preview_leading_indent(pl);
    if (indent >= cols / 2) indent = 0;
    int rows = 0, i = 0;
    do {
        int avail = (rows == 0) ? cols : cols - indent;
        if (avail <= 0) avail = 1;
        int next;
        find_preview_word_break(pl, i, avail, &next);
        rows++; i = next;
    } while (i < pl->len);
    return rows;
}

/* Draw one ACS character with its style. */
static void draw_acs_char(unsigned char acs_id, attr_t a)
{
    chtype acs;
    switch (acs_id) {
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
}

int preview_draw_line_wrapped(int screen_y, int screen_cols,
                              PreviewLine *pl, int max_rows)
{
    /* Table lines contain ACS box-drawing chars — never word-wrap them */
    if (preview_line_is_table(pl)) {
        preview_draw_line(screen_y, screen_cols, pl, 0);
        return 1;
    }

    int indent = preview_leading_indent(pl);
    if (indent >= screen_cols / 2) indent = 0;

    int row = 0;
    int i   = 0;

    do {
        int avail = (row == 0) ? screen_cols : screen_cols - indent;
        if (avail <= 0) avail = 1;
        int next, end = find_preview_word_break(pl, i, avail, &next);

        move(screen_y + row, 0);
        clrtoeol();

        if (row > 0 && indent > 0) {
            for (int k = 0; k < indent; k++) {
                if (k < pl->len && pl->styles[k].acs == PM_VLINE) {
                    /* Replicate blockquote vertical bar */
                    attr_t a = COLOR_PAIR(pl->styles[k].cpair) | pl->styles[k].attr;
                    draw_acs_char(PM_VLINE, a);
                } else {
                    addch(' ');
                }
            }
        }

        /* draw segment [i, end) */
        int j = i;
        while (j < end) {
            attr_t a = COLOR_PAIR(pl->styles[j].cpair) | pl->styles[j].attr;
            if (pl->styles[j].acs) {
                draw_acs_char(pl->styles[j].acs, a);
                j++;
            } else {
                int clen = utf8_clen((unsigned char)pl->text[j]);
                if (j + clen > end) clen = end - j;
                attron(a);
                addnstr(pl->text + j, clen);
                attroff(a);
                j += clen;
            }
        }

        i = next;
        row++;
    } while (i < pl->len && row < max_rows);

    return (row < max_rows) ? row : max_rows;
}

int preview_find_line(PreviewBuffer *pb, int buffer_row)
{
    for (int i = 0; i < pb->num_lines; i++)
        if (pb->lines[i].source_row >= buffer_row)
            return i;
    return (pb->num_lines > 0) ? pb->num_lines - 1 : 0;
}

/* ── Search highlighting for preview mode ── */

/* Build a byte-index → screen-column map for a preview line.
   Caller must free the returned array (length pl->len). */
static int *preview_build_col_map(PreviewLine *pl, int *total_cols)
{
    int *col_map = malloc(pl->len * sizeof(int));
    if (!col_map) return NULL;
    int col = 0;
    for (int i = 0; i < pl->len; ) {
        col_map[i] = col;
        if (pl->styles[i].acs) {
            col++;
            i++;
        } else {
            int w = utf8_char_width(pl->text, pl->len, i);
            int clen = utf8_clen((unsigned char)pl->text[i]);
            if (i + clen > pl->len) clen = pl->len - i;
            for (int k = 1; k < clen && i + k < pl->len; k++)
                col_map[i + k] = col;
            col += w;
            i += clen;
        }
    }
    if (total_cols) *total_cols = col;
    return col_map;
}

/* Compute display-column width of a byte range [byte_off, byte_off+blen)
   using a prebuilt col_map.  total_cols is the column after the last byte. */
static int preview_match_width(int *col_map, int total_cols,
                               int byte_off, int blen, int text_len)
{
    int start_col = col_map[byte_off];
    int end_col;
    int end_byte = byte_off + blen;
    if (end_byte < text_len)
        end_col = col_map[end_byte];
    else
        end_col = total_cols;
    return end_col - start_col;
}

/* Redraw a range of bytes from a preview line at (screen_y, screen_col)
   with search-highlight attributes, handling ACS and UTF-8. */
static void preview_redraw_hl(int screen_y, int screen_col, int screen_cols,
                               PreviewLine *pl, int byte_start, int byte_end)
{
    attr_t hl = COLOR_PAIR(CP_SEARCH_HL) | A_BOLD;
    int col = screen_col;
    int i = byte_start;
    while (i < byte_end && col < screen_cols) {
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
            mvaddch(screen_y, col, acs | hl);
            col++;
            i++;
        } else {
            int clen = utf8_clen((unsigned char)pl->text[i]);
            if (i + clen > pl->len) clen = pl->len - i;
            if (i + clen > byte_end) clen = byte_end - i;
            move(screen_y, col);
            attrset(hl);
            addnstr(pl->text + i, clen);
            attrset(A_NORMAL);
            col += utf8_char_width(pl->text, pl->len, i);
            i += clen;
        }
    }
}

/* Case-insensitive substring search (like strstr but ignores case). */
static char *strstr_ci(const char *haystack, const char *needle, int nlen)
{
    if (nlen <= 0) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
    }
    return NULL;
}

/* Apply search highlights on a non-wrapped preview line already drawn at
   screen_y.  Redraws matched characters with highlight attributes. */
void preview_highlight_search(int screen_y, int screen_cols,
                              PreviewLine *pl, int scroll_x,
                              const char *query, int qlen)
{
    if (qlen <= 0 || pl->len == 0) return;

    int total_cols;
    int *col_map = preview_build_col_map(pl, &total_cols);
    if (!col_map) return;

    char *p = pl->text;
    while ((p = strstr_ci(p, query, qlen)) != NULL) {
        int byte_off = (int)(p - pl->text);
        if (byte_off + qlen > pl->len) break;

        int sc = col_map[byte_off] - scroll_x;
        int w  = preview_match_width(col_map, total_cols,
                                     byte_off, qlen, pl->len);
        if (sc < 0) { w += sc; sc = 0; }
        if (w > 0 && sc < screen_cols) {
            if (sc + w > screen_cols) w = screen_cols - sc;
            preview_redraw_hl(screen_y, sc, screen_cols,
                              pl, byte_off, byte_off + qlen);
        }
        p++;
    }

    free(col_map);
}

/* Apply search highlights on a wrapped preview line that was drawn starting
   at screen_y, spanning up to max_rows screen rows.  Uses mvchgat. */
int preview_highlight_search_wrapped(int screen_y, int screen_cols,
                                     PreviewLine *pl, int max_rows,
                                     const char *query, int qlen)
{
    if (qlen <= 0 || pl->len == 0) return 0;
    if (preview_line_is_table(pl)) {
        preview_highlight_search(screen_y, screen_cols, pl, 0, query, qlen);
        return 1;
    }

    int indent = preview_leading_indent(pl);
    if (indent >= screen_cols / 2) indent = 0;

    /* Build byte → (row, col) map */
    int *row_map = calloc(pl->len, sizeof(int));
    int *col_map = calloc(pl->len, sizeof(int));
    if (!row_map || !col_map) { free(row_map); free(col_map); return 0; }

    int row = 0, i = 0;
    do {
        int avail = (row == 0) ? screen_cols : screen_cols - indent;
        if (avail <= 0) avail = 1;
        int next, end;
        end = find_preview_word_break(pl, i, avail, &next);

        int pad = (row > 0) ? indent : 0;
        int col = pad;
        int j = i;
        while (j < end) {
            row_map[j] = row;
            col_map[j] = col;
            if (pl->styles[j].acs) {
                col++;
                j++;
            } else {
                int w = utf8_char_width(pl->text, pl->len, j);
                int clen = utf8_clen((unsigned char)pl->text[j]);
                if (j + clen > pl->len) clen = pl->len - j;
                for (int k = 1; k < clen && j + k < pl->len; k++) {
                    row_map[j + k] = row;
                    col_map[j + k] = col;
                }
                col += w;
                j += clen;
            }
        }

        i = next;
        row++;
    } while (i < pl->len && row < max_rows);

    /* Find and highlight matches by redrawing with highlight attrs */
    char *p = pl->text;
    while ((p = strstr_ci(p, query, qlen)) != NULL) {
        int byte_off = (int)(p - pl->text);
        if (byte_off + qlen > pl->len) break;

        /* Group consecutive match bytes by screen row */
        int b = byte_off;
        while (b < byte_off + qlen && b < pl->len) {
            int sr = row_map[b];
            if (sr >= max_rows) { b++; continue; }

            int run_start = b;
            while (b < byte_off + qlen && b < pl->len && row_map[b] == sr)
                b++;

            int sc = col_map[run_start];
            if (sc >= 0 && sc < screen_cols) {
                preview_redraw_hl(screen_y + sr, sc, screen_cols,
                                  pl, run_start, b);
            }
        }
        p++;
    }

    free(row_map);
    free(col_map);
    return row;
}
