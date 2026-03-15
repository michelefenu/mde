/* term — POSIX terminal I/O: drop-in replacement for ncurses. */
#ifndef TERM_H
#define TERM_H

/* ================================================================
 *  Attribute bits
 * ================================================================ */

typedef unsigned long attr_t;

#define A_NORMAL        0UL
#define A_BOLD          (1UL << 0)
#define A_DIM           (1UL << 1)
#define A_ITALIC        (1UL << 2)
#define A_UNDERLINE     (1UL << 3)
#define A_REVERSE       (1UL << 4)
#define A_STRIKETHROUGH (1UL << 5)

/* Color pair encoding: pair index stored in bits 8-23 */
#define COLOR_PAIR(n)   ((attr_t)(n) << 8)
#define PAIR_NUMBER(a)  (((a) >> 8) & 0xFFFFUL)

/* ================================================================
 *  Basic color constants
 * ================================================================ */

#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7

/* Always advertise 256-color support */
#define COLORS 256

/* ================================================================
 *  Key constants (> 0xFF to avoid collision with byte values)
 * ================================================================ */

#define KEY_UP        0x100
#define KEY_DOWN      0x101
#define KEY_LEFT      0x102
#define KEY_RIGHT     0x103
#define KEY_HOME      0x104
#define KEY_END       0x105
#define KEY_PPAGE     0x106
#define KEY_NPAGE     0x107
#define KEY_SF        0x108
#define KEY_SR        0x109
#define KEY_BACKSPACE 0x10A
#define KEY_DC        0x10B
#define KEY_ENTER     0x10C
#define KEY_F(n)      (0x110 + (n))
#define KEY_RESIZE    0x120
#define KEY_CODE_YES  0x121

/* Sentinel for multi-byte UTF-8 input (returned by term_read_key) */
#define KEY_UTF8      (-2)

#define ERR  (-1)
#define TRUE  1
#define FALSE 0

/* ================================================================
 *  WINDOW stub (stdscr is only used as an argument to keypad/getmaxyx)
 * ================================================================ */

typedef int WINDOW;
extern WINDOW *stdscr;

/* getmaxyx uses ioctl instead of ncurses internals */
#define getmaxyx(win, rows, cols) term_get_size(&(rows), &(cols))

/* ================================================================
 *  Lifecycle
 * ================================================================ */

WINDOW *initscr(void);
void    endwin(void);

/* No-op stubs — behaviour baked into initscr/endwin */
void raw(void);
void noecho(void);
void keypad(WINDOW *win, int bf);
void set_escdelay(int ms);

/* ================================================================
 *  Drawing
 * ================================================================ */

void move(int y, int x);
void addch(int c);
void addnstr(const char *s, int n);
void clrtoeol(void);
void mvaddch(int y, int x, int c);
void mvchgat(int y, int x, int n, attr_t attr, short cpair, void *opts);
void attron(attr_t a);
void attroff(attr_t a);
void attrset(attr_t a);
void refresh(void);
int  curs_set(int visibility);

/* ================================================================
 *  Colors
 * ================================================================ */

int  has_colors(void);
void start_color(void);
void use_default_colors(void);
int  init_pair(short pair, short fg, short bg);

/* ================================================================
 *  Dimensions
 * ================================================================ */

void term_get_size(int *rows, int *cols);

/* ================================================================
 *  Input
 * ================================================================ */

/* Read one key event.
   Returns KEY_* constant, an ASCII byte, or KEY_UTF8 (-2) for
   non-ASCII Unicode.  On KEY_UTF8, utf8_buf[0..utf8_len-1] hold the
   raw UTF-8 bytes.  Returns ERR on error, KEY_RESIZE on SIGWINCH. */
int term_read_key(char utf8_buf[4], int *utf8_len);

/* ================================================================
 *  External command support
 * ================================================================ */

void term_suspend(void);  /* leave alt screen, restore normal termios */
void term_resume(void);   /* re-enter raw mode + alt screen */

#endif /* TERM_H */
