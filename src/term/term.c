/* term — POSIX terminal I/O implementation (drop-in replacement for ncurses). */
#include "term.h"
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* ================================================================
 *  Internal state
 * ================================================================ */

WINDOW *stdscr = NULL;

static struct termios g_orig_termios;
static int g_rows = 24, g_cols = 80;
static int g_cursor_visible = 1;
static int g_escdelay_ms = 25;
static volatile int g_resize_pending = 0;

typedef struct { short fg; short bg; } ColorPair;
static ColorPair g_pairs[256];

/* Current attribute state.  g_cur_attr is what the caller has requested;
   g_emitted_attr is what has actually been sent to the terminal so far. */
static attr_t g_cur_attr      = A_NORMAL;
static attr_t g_emitted_attr  = (attr_t)-1;  /* invalid sentinel forces first emit */

/* Output buffer — all drawing is accumulated here; refresh() flushes it. */
#define OUTBUF_SIZE (256 * 1024)
static char g_outbuf[OUTBUF_SIZE];
static int  g_outpos = 0;

/* Bit masks for the two fields packed into attr_t */
#define PAIR_MASK  0xFFFF00UL   /* bits 8-23: color pair index */
#define ATTR_MASK  0x0000FFUL   /* bits  0-7: text attributes  */

/* ================================================================
 *  Output helpers
 * ================================================================ */

static void out_flush(void)
{
    if (g_outpos > 0) {
        (void)write(STDOUT_FILENO, g_outbuf, (size_t)g_outpos);
        g_outpos = 0;
    }
}

static void out_raw(const char *s, int n)
{
    if (g_outpos + n > OUTBUF_SIZE)
        out_flush();
    if (n > OUTBUF_SIZE) {
        (void)write(STDOUT_FILENO, s, (size_t)n);
        return;
    }
    memcpy(g_outbuf + g_outpos, s, (size_t)n);
    g_outpos += n;
}

static void out_str(const char *s)
{
    out_raw(s, (int)strlen(s));
}

/* ================================================================
 *  SGR emission
 * ================================================================ */

static void emit_sgr(attr_t a)
{
    if (a == g_emitted_attr)
        return;
    g_emitted_attr = a;

    char buf[96];
    int pos = 0;

    /* Always start with a full reset */
    buf[pos++] = '\033'; buf[pos++] = '['; buf[pos++] = '0'; buf[pos++] = 'm';

    if (a & A_BOLD)          { buf[pos++]='\033'; buf[pos++]='['; buf[pos++]='1'; buf[pos++]='m'; }
    if (a & A_DIM)           { buf[pos++]='\033'; buf[pos++]='['; buf[pos++]='2'; buf[pos++]='m'; }
    if (a & A_ITALIC)        { buf[pos++]='\033'; buf[pos++]='['; buf[pos++]='3'; buf[pos++]='m'; }
    if (a & A_UNDERLINE)     { buf[pos++]='\033'; buf[pos++]='['; buf[pos++]='4'; buf[pos++]='m'; }
    if (a & A_REVERSE)       { buf[pos++]='\033'; buf[pos++]='['; buf[pos++]='7'; buf[pos++]='m'; }
    if (a & A_STRIKETHROUGH) { buf[pos++]='\033'; buf[pos++]='['; buf[pos++]='9'; buf[pos++]='m'; }

    short pair = (short)PAIR_NUMBER(a);
    if (pair > 0 && pair < 256) {
        short fg = g_pairs[pair].fg;
        short bg = g_pairs[pair].bg;

        if (fg >= 0 && fg < 8) {
            char tmp[8];
            int n = snprintf(tmp, sizeof(tmp), "\033[%dm", 30 + fg);
            memcpy(buf + pos, tmp, (size_t)n); pos += n;
        } else if (fg >= 8) {
            char tmp[16];
            int n = snprintf(tmp, sizeof(tmp), "\033[38;5;%dm", fg);
            memcpy(buf + pos, tmp, (size_t)n); pos += n;
        }

        if (bg == -1) {
            const char *s = "\033[49m";
            memcpy(buf + pos, s, 5); pos += 5;
        } else if (bg >= 0 && bg < 8) {
            char tmp[8];
            int n = snprintf(tmp, sizeof(tmp), "\033[%dm", 40 + bg);
            memcpy(buf + pos, tmp, (size_t)n); pos += n;
        } else if (bg >= 8) {
            char tmp[16];
            int n = snprintf(tmp, sizeof(tmp), "\033[48;5;%dm", bg);
            memcpy(buf + pos, tmp, (size_t)n); pos += n;
        }
    }

    out_raw(buf, pos);
}

/* ================================================================
 *  SIGWINCH handler
 * ================================================================ */

static void sigwinch_handler(int sig)
{
    (void)sig;
    g_resize_pending = 1;
}

/* ================================================================
 *  Raw termios helpers (shared between initscr and term_resume)
 * ================================================================ */

static void apply_raw_termios(void)
{
    struct termios raw = g_orig_termios;
    raw.c_iflag &= ~(unsigned)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(unsigned)(OPOST);
    raw.c_cflag |=  (unsigned)(CS8);
    raw.c_lflag &= ~(unsigned)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

WINDOW *initscr(void)
{
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    apply_raw_termios();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigwinch_handler;
    sigaction(SIGWINCH, &sa, NULL);

    term_get_size(&g_rows, &g_cols);

    /* Enter alternate screen and show cursor */
    out_str("\033[?1049h\033[?25h");
    out_flush();

    stdscr = (WINDOW *)1;  /* non-null sentinel */
    return stdscr;
}

void endwin(void)
{
    /* Reset attrs, show cursor, leave alternate screen */
    out_str("\033[0m\033[?25h\033[?1049l");
    out_flush();
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
    g_emitted_attr = (attr_t)-1;
    stdscr = NULL;
}

void raw(void)    { /* done in initscr */ }
void noecho(void) { /* done in initscr */ }
void keypad(WINDOW *win, int bf) { (void)win; (void)bf; }
void set_escdelay(int ms) { g_escdelay_ms = ms; }

/* ================================================================
 *  Colors
 * ================================================================ */

int has_colors(void) { return 1; }

void start_color(void)
{
    memset(g_pairs, 0, sizeof(g_pairs));
    /* pair 0 = default */
    g_pairs[0].fg = -1;
    g_pairs[0].bg = -1;
}

void use_default_colors(void)
{
    g_pairs[0].fg = -1;
    g_pairs[0].bg = -1;
}

int init_pair(short pair, short fg, short bg)
{
    if (pair <= 0 || pair >= 256) return ERR;
    g_pairs[pair].fg = fg;
    g_pairs[pair].bg = bg;
    return 0;
}

/* ================================================================
 *  Dimensions
 * ================================================================ */

void term_get_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
        ws.ws_row > 0 && ws.ws_col > 0) {
        g_rows = ws.ws_row;
        g_cols = ws.ws_col;
    }
    if (rows) *rows = g_rows;
    if (cols) *cols = g_cols;
}

/* ================================================================
 *  Drawing
 * ================================================================ */

void move(int y, int x)
{
    char buf[24];
    int n = snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
    out_raw(buf, n);
}

void addch(int c)
{
    if (c >= 0 && c < 256) {
        unsigned char b = (unsigned char)c;
        out_raw((const char *)&b, 1);
    }
}

void addnstr(const char *s, int n)
{
    if (s && n > 0)
        out_raw(s, n);
}

void clrtoeol(void)
{
    /* Reset terminal to normal, clear to end of line, then restore attr */
    g_emitted_attr = A_NORMAL;
    out_str("\033[0m\033[K");
    if (g_cur_attr != A_NORMAL)
        emit_sgr(g_cur_attr);
}

void mvaddch(int y, int x, int c)
{
    move(y, x);
    addch(c);
}

void mvchgat(int y, int x, int n, attr_t attr, short cpair, void *opts)
{
    /* No-op: TOC highlight handled via overlay in preview_draw_line */
    (void)y; (void)x; (void)n; (void)attr; (void)cpair; (void)opts;
}

void attron(attr_t a)
{
    /* Replace color pair bits; OR in attribute bits */
    if (a & PAIR_MASK)
        g_cur_attr = (g_cur_attr & ~PAIR_MASK) | (a & PAIR_MASK);
    g_cur_attr |= (a & ATTR_MASK);
    emit_sgr(g_cur_attr);
}

void attroff(attr_t a)
{
    if (a & PAIR_MASK)
        g_cur_attr &= ~PAIR_MASK;
    g_cur_attr &= ~(a & ATTR_MASK);
    emit_sgr(g_cur_attr);
}

void attrset(attr_t a)
{
    g_cur_attr = a;
    emit_sgr(g_cur_attr);
}

void refresh(void)
{
    /* Emit cursor visibility state then flush the draw buffer */
    if (g_cursor_visible)
        out_str("\033[?25h");
    else
        out_str("\033[?25l");
    out_flush();
}

int curs_set(int visibility)
{
    int prev = g_cursor_visible;
    g_cursor_visible = (visibility > 0) ? 1 : 0;
    /* Emit immediately (outside of a refresh cycle) */
    if (g_cursor_visible)
        out_str("\033[?25h");
    else
        out_str("\033[?25l");
    out_flush();
    return prev;
}

/* ================================================================
 *  Input
 * ================================================================ */

/* Parse an escape sequence that follows an ESC byte.
   buf[0..len-1] are the bytes read after the ESC.
   Returns a KEY_* constant, or ERR if unrecognised. */
static int parse_escape_seq(const char *buf, int len)
{
    if (len == 0)
        return 27;  /* lone ESC */

    /* SS3: \033O<char> */
    if (buf[0] == 'O' && len >= 2) {
        switch (buf[1]) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
        case 'M': return KEY_ENTER;
        case 'P': return KEY_F(1);
        case 'Q': return KEY_F(2);
        case 'R': return KEY_F(3);
        case 'S': return KEY_F(4);
        }
    }

    /* CSI: \033[<...> */
    if (buf[0] == '[' && len >= 2) {
        /* Simple one-char CSI */
        if (len == 2) {
            switch (buf[1]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
            case 'a': return KEY_SR;   /* Shift+Up (rxvt) */
            case 'b': return KEY_SF;   /* Shift+Down (rxvt) */
            }
        }

        /* Tilde sequences: \033[N~ */
        if (buf[len - 1] == '~') {
            int num = 0;
            for (int i = 1; i < len - 1; i++) {
                if (buf[i] >= '0' && buf[i] <= '9')
                    num = num * 10 + (buf[i] - '0');
            }
            switch (num) {
            case 1:  return KEY_HOME;
            case 3:  return KEY_DC;
            case 4:  return KEY_END;
            case 5:  return KEY_PPAGE;
            case 6:  return KEY_NPAGE;
            case 11: return KEY_F(1);
            case 12: return KEY_F(2);
            case 13: return KEY_F(3);
            case 14: return KEY_F(4);
            }
        }

        /* Modifier + cursor: \033[1;2A (Shift+Up), \033[1;2B (Shift+Down) */
        if (len >= 5 && buf[1] == '1' && buf[2] == ';' && buf[3] == '2') {
            switch (buf[4]) {
            case 'A': return KEY_SR;
            case 'B': return KEY_SF;
            }
        }
    }

    return ERR;
}

int term_read_key(char utf8_buf[4], int *utf8_len)
{
    *utf8_len = 0;
    unsigned char b;
    int n;

retry:
    n = (int)read(STDIN_FILENO, &b, 1);
    if (n < 0) {
        if (errno == EINTR) {
            if (g_resize_pending) {
                g_resize_pending = 0;
                term_get_size(&g_rows, &g_cols);
                return KEY_RESIZE;
            }
            goto retry;
        }
        return ERR;
    }
    if (n == 0)
        return ERR;

    /* Escape — try to read a full sequence */
    if (b == 0x1B) {
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        if (poll(&pfd, 1, g_escdelay_ms) <= 0)
            return 27;  /* lone ESC */

        char seq[16];
        int slen = 0;
        if (read(STDIN_FILENO, &seq[slen], 1) <= 0)
            return 27;
        slen++;

        /* CSI (\033[) and SS3 (\033O) may have more bytes */
        if (seq[0] == '[' || seq[0] == 'O') {
            for (;;) {
                if (slen >= (int)(sizeof(seq) - 1)) break;
                if (poll(&pfd, 1, g_escdelay_ms) <= 0) break;
                if (read(STDIN_FILENO, &seq[slen], 1) <= 0) break;
                char last = seq[slen];
                slen++;
                /* Alphabetic or tilde terminates the sequence */
                if ((last >= 'A' && last <= 'Z') ||
                    (last >= 'a' && last <= 'z') ||
                    last == '~') break;
            }
        }

        int key = parse_escape_seq(seq, slen);
        return (key == ERR) ? 27 : key;
    }

    /* DEL / Backspace */
    if (b == 127)
        return KEY_BACKSPACE;

    /* UTF-8 multi-byte sequence */
    if (b >= 0xC0) {
        utf8_buf[0] = (char)b;
        int expected = (b >= 0xF0) ? 4 : (b >= 0xE0) ? 3 : 2;
        for (int i = 1; i < expected; i++) {
            if (read(STDIN_FILENO, &utf8_buf[i], 1) <= 0) {
                *utf8_len = i;
                return KEY_UTF8;
            }
        }
        *utf8_len = expected;
        return KEY_UTF8;
    }

    return (int)b;
}

/* ================================================================
 *  External command support
 * ================================================================ */

void term_suspend(void)
{
    out_str("\033[0m\033[?25h\033[?1049l");
    out_flush();
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
}

void term_resume(void)
{
    apply_raw_termios();
    g_emitted_attr = (attr_t)-1;  /* invalidate — terminal state unknown */
    out_str("\033[?1049h");
    out_flush();
}
