/* syntax — Basic syntax highlighting for C, Python, JavaScript, and Shell. */
#include "syntax.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Language definitions
 * ================================================================ */

static const char *c_keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned",
    "void", "volatile", "while",
    "#define", "#elif", "#else", "#endif", "#if", "#ifdef", "#ifndef",
    "#include", "#pragma", "#undef",
    "NULL", "true", "false",
    NULL
};

static const char *python_keywords[] = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield",
    "self", "print", "len", "range", "int", "str", "float", "list",
    "dict", "set", "tuple", "bool", "type", "isinstance", "open",
    NULL
};

static const char *js_keywords[] = {
    "async", "await", "break", "case", "catch", "class", "const",
    "continue", "debugger", "default", "delete", "do", "else", "export",
    "extends", "false", "finally", "for", "from", "function", "if",
    "import", "in", "instanceof", "let", "new", "null", "of", "return",
    "static", "super", "switch", "this", "throw", "true", "try",
    "typeof", "undefined", "var", "void", "while", "with", "yield",
    "console", "require", "module",
    NULL
};

static const char *shell_keywords[] = {
    "case", "do", "done", "elif", "else", "esac", "fi", "for", "function",
    "if", "in", "local", "return", "select", "shift", "then", "until",
    "while",
    "cd", "echo", "eval", "exec", "exit", "export", "let", "read",
    "readonly", "set", "source", "test", "trap", "unset",
    NULL
};

struct SyntaxLang {
    const char  *name;          /* canonical name */
    const char **aliases;       /* NULL-terminated alias list */
    const char **keywords;      /* sorted keyword list, NULL-terminated */
    int          num_keywords;
    const char  *line_comment;  /* e.g. "//", "#" */
    const char  *string_delims; /* e.g. "\"'" */
};

static const char *c_aliases[]      = { "c", "h", "cpp", "c++", "cc", "cxx", NULL };
static const char *python_aliases[] = { "python", "py", "python3", NULL };
static const char *js_aliases[]     = { "javascript", "js", "jsx", "ts",
                                        "typescript", "tsx", NULL };
static const char *shell_aliases[]  = { "sh", "bash", "shell", "zsh", NULL };

/* Compare function for qsort/bsearch on keyword strings */
static int kw_cmp(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/* Count and sort keywords once. Called lazily on first use. */
static int count_keywords(const char **kws)
{
    int n = 0;
    while (kws[n]) n++;
    return n;
}

static SyntaxLang languages[4];
static int lang_init_done = 0;

static void init_languages(void)
{
    if (lang_init_done) return;
    lang_init_done = 1;

    languages[0] = (SyntaxLang){
        "c", c_aliases, c_keywords, count_keywords(c_keywords),
        "//", "\"'"
    };
    languages[1] = (SyntaxLang){
        "python", python_aliases, python_keywords, count_keywords(python_keywords),
        "#", "\"'"
    };
    languages[2] = (SyntaxLang){
        "javascript", js_aliases, js_keywords, count_keywords(js_keywords),
        "//", "\"'`"
    };
    languages[3] = (SyntaxLang){
        "shell", shell_aliases, shell_keywords, count_keywords(shell_keywords),
        "#", "\"'"
    };

    /* Sort each keyword list for binary search */
    for (int i = 0; i < 4; i++)
        qsort(languages[i].keywords, languages[i].num_keywords,
              sizeof(char *), kw_cmp);
}

/* ================================================================
 *  Public: find language by name
 * ================================================================ */

const SyntaxLang *syntax_find_lang(const char *name, int name_len)
{
    if (!name || name_len <= 0) return NULL;
    init_languages();

    for (int i = 0; i < 4; i++) {
        const char **al = languages[i].aliases;
        for (int j = 0; al[j]; j++) {
            int alen = (int)strlen(al[j]);
            if (alen != name_len) continue;
            /* Case-insensitive compare */
            int match = 1;
            for (int k = 0; k < name_len; k++) {
                if (tolower((unsigned char)name[k]) !=
                    tolower((unsigned char)al[j][k])) {
                    match = 0; break;
                }
            }
            if (match) return &languages[i];
        }
    }
    return NULL;
}

/* ================================================================
 *  Public: highlight one line of code
 * ================================================================ */

static int is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

void syntax_highlight_line(const SyntaxLang *lang, const char *line, int len,
                           CharStyle *styles)
{
    if (!lang || !line || len <= 0) return;

    int i = 0;
    while (i < len) {
        /* 1. Line comment */
        if (lang->line_comment) {
            int clen = (int)strlen(lang->line_comment);
            if (i + clen <= len && memcmp(line + i, lang->line_comment, clen) == 0) {
                for (int j = i; j < len; j++) {
                    styles[j].cpair = CP_SYN_COMMENT;
                    styles[j].attr  = A_DIM;
                }
                return;
            }
        }

        /* 2. String literal */
        if (lang->string_delims && strchr(lang->string_delims, line[i])) {
            char delim = line[i];
            styles[i].cpair = CP_SYN_STRING;
            int j = i + 1;
            while (j < len) {
                if (line[j] == '\\' && j + 1 < len) {
                    styles[j].cpair = CP_SYN_STRING;
                    styles[j + 1].cpair = CP_SYN_STRING;
                    j += 2;
                    continue;
                }
                styles[j].cpair = CP_SYN_STRING;
                if (line[j] == delim) { j++; break; }
                j++;
            }
            i = j;
            continue;
        }

        /* 3. Number literal (not preceded by letter/underscore) */
        if (isdigit((unsigned char)line[i]) &&
            (i == 0 || !is_ident_char(line[i - 1]))) {
            int j = i;
            /* Hex: 0x... */
            if (j + 1 < len && line[j] == '0' &&
                (line[j + 1] == 'x' || line[j + 1] == 'X')) {
                j += 2;
                while (j < len && isxdigit((unsigned char)line[j])) j++;
            } else {
                while (j < len && (isdigit((unsigned char)line[j]) ||
                                   line[j] == '.')) j++;
            }
            for (int k = i; k < j; k++)
                styles[k].cpair = CP_SYN_NUMBER;
            i = j;
            continue;
        }

        /* 4. Identifier / keyword (including # for preprocessor) */
        if (is_ident_char(line[i]) || line[i] == '#') {
            int start = i;
            if (line[i] == '#') i++;
            while (i < len && is_ident_char(line[i])) i++;
            int wlen = i - start;

            /* Binary search in sorted keyword list */
            char buf[64];
            if (wlen < (int)sizeof(buf)) {
                memcpy(buf, line + start, wlen);
                buf[wlen] = '\0';
                const char *key = buf;
                const char **found = bsearch(&key, lang->keywords,
                                              lang->num_keywords,
                                              sizeof(char *), kw_cmp);
                if (found) {
                    for (int k = start; k < i; k++) {
                        styles[k].cpair = CP_SYN_KEYWORD;
                        styles[k].attr |= A_BOLD;
                    }
                }
            }
            continue;
        }

        /* Not a token start -- skip */
        i++;
    }
}
