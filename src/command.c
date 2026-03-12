#include "command.h"
#include "links.h"
#include "search.h"
#include "help.h"
#include "preview_ui.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINKS 256

static void open_url(Editor *ed, const char *url)
{
    char cmd[600];
#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd), "open '%s' >/dev/null 2>&1", url);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1", url);
#endif
    def_prog_mode();
    endwin();
    int ret = system(cmd);
    reset_prog_mode();
    refresh();
    if (ret != 0)
        editor_set_status(ed, "Failed to open: %s", url);
    else
        editor_set_status(ed, "Opened: %s", url);
}

static int command_tab_complete(char *buf, int buflen, int maxlen)
{
    /* Only complete for "e <partial>" */
    if (buflen < 2 || buf[0] != 'e' || buf[1] != ' ')
        return buflen;

    const char *partial = buf + 2;

    /* Split partial into directory and basename */
    char dir[512]  = ".";
    char base[256] = "";
    const char *slash = strrchr(partial, '/');
    if (slash) {
        size_t dirlen = (size_t)(slash - partial);
        if (dirlen == 0) {
            strcpy(dir, "/");
        } else {
            strncpy(dir, partial, dirlen);
            dir[dirlen] = '\0';
        }
        strncpy(base, slash + 1, sizeof(base) - 1);
    } else {
        strncpy(base, partial, sizeof(base) - 1);
    }

    /* Collect matching directory entries */
    DIR *dp = opendir(dir);
    if (!dp) return buflen;

    char matches[64][256];
    int  nmatch   = 0;
    size_t baselen = strlen(base);
    struct dirent *de;
    while ((de = readdir(dp)) != NULL && nmatch < 64) {
        /* Skip hidden files unless the user explicitly typed a leading dot */
        if (de->d_name[0] == '.' && (baselen == 0 || base[0] != '.'))
            continue;
        if (strncmp(de->d_name, base, baselen) == 0)
            strncpy(matches[nmatch++], de->d_name, 255);
    }
    closedir(dp);

    if (nmatch == 0) return buflen;

    /* Find the longest common prefix among all matches */
    char common[256];
    strncpy(common, matches[0], 255);
    for (int i = 1; i < nmatch; i++) {
        int j = 0;
        while (common[j] && matches[i][j] && common[j] == matches[i][j])
            j++;
        common[j] = '\0';
    }

    /* Reconstruct the path suffix (dir/ + common) */
    char suffix[512];
    if (slash) {
        if (strcmp(dir, "/") == 0)
            snprintf(suffix, sizeof(suffix), "/%s", common);
        else
            snprintf(suffix, sizeof(suffix), "%s/%s", dir, common);
    } else {
        strncpy(suffix, common, sizeof(suffix) - 1);
        suffix[sizeof(suffix) - 1] = '\0';
    }

    /* Append a trailing '/' if single match and it is a directory */
    if (nmatch == 1) {
        char fullpath[768];
        if (slash)
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, common);
        else
            strncpy(fullpath, common, sizeof(fullpath) - 1);
        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            size_t slen = strlen(suffix);
            if (slen < sizeof(suffix) - 1) {
                suffix[slen]     = '/';
                suffix[slen + 1] = '\0';
            }
        }
    }

    int newlen = snprintf(buf, (size_t)maxlen, "e %s", suffix);
    if (newlen < 0 || newlen >= maxlen) return buflen;
    return newlen;
}

void editor_command_mode(Editor *ed)
{
    char *input = editor_prompt(ed, ":", NULL, command_tab_complete);
    if (!input) {
        editor_set_status(ed, "");
        return;
    }

    /* Strip leading/trailing whitespace */
    char *cmd = input;
    while (*cmd && isspace((unsigned char)*cmd)) cmd++;
    char *end = cmd + strlen(cmd) - 1;
    while (end > cmd && isspace((unsigned char)*end)) *end-- = '\0';

    if (strcmp(cmd, "w") == 0) {
        editor_save(ed);
    } else if (strcmp(cmd, "q") == 0) {
        if (ed->dirty) {
            editor_set_status(ed, "Unsaved changes, use :q! to force");
        } else {
            ed->quit = 1;
        }
    } else if (strcmp(cmd, "q!") == 0) {
        ed->quit = 1;
    } else if (strcmp(cmd, "wq") == 0 || strcmp(cmd, "x") == 0) {
        editor_save(ed);
        if (!ed->dirty)
            ed->quit = 1;
    } else if (strncmp(cmd, "e ", 2) == 0) {
        char *filename = cmd + 2;
        while (*filename && isspace((unsigned char)*filename)) filename++;
        if (*filename) {
            if (ed->dirty) {
                editor_set_status(ed,
                    "Unsaved changes, save first or use :q! to quit");
            } else {
                /* Exit preview, reload file, re-enter preview */
                editor_toggle_preview(ed);
                buffer_free(&ed->buf);
                buffer_init(&ed->buf);
                undo_stack_clear(&ed->undo);
                undo_stack_clear(&ed->redo);
                ed->cx = 0;
                ed->cy = 0;
                ed->scroll_y = 0;
                ed->scroll_x = 0;
                ed->dirty = 0;
                ed->search_query[0] = '\0';
                ed->search_query_len = 0;
                editor_open(ed, filename);
                editor_toggle_preview(ed);
            }
        } else {
            editor_set_status(ed, "Usage: :e <filename>");
        }
    } else if (strcmp(cmd, "set wrap") == 0) {
        ed->word_wrap = 1;
        editor_set_status(ed, "Word wrap ON");
    } else if (strcmp(cmd, "set nowrap") == 0) {
        ed->word_wrap = 0;
        editor_set_status(ed, "Word wrap OFF");
    } else if (strcmp(cmd, "help") == 0) {
        editor_show_help(ed);
    } else if (strncmp(cmd, "open ", 5) == 0) {
        int idx = atoi(cmd + 5);
        if (idx <= 0) {
            editor_set_status(ed, "Usage: :open N  (N >= 1)");
        } else {
            LinkInfo links[MAX_LINKS];
            int count = links_collect(&ed->buf, links, MAX_LINKS);
            if (idx > count) {
                editor_set_status(ed, "No link %d (document has %d link%s)",
                                  idx, count, count == 1 ? "" : "s");
            } else {
                const char *url = links[idx - 1].url;
                if (url[0] == '#') {
                    /* Internal anchor — scroll preview */
                    int row = links_find_anchor(&ed->buf, url + 1);
                    if (row < 0) {
                        editor_set_status(ed, "Anchor not found: %s", url);
                    } else {
                        if (!ed->preview_mode) editor_toggle_preview(ed);
                        ed->preview_scroll_y =
                            preview_find_line(&ed->preview_buf, row);
                        clamp_preview_scroll(ed);
                        editor_set_status(ed, "Jumped to: %s", url);
                    }
                } else {
                    open_url(ed, url);
                }
            }
        }
    } else {
        /* Try as line number */
        int line = atoi(cmd);
        if (line > 0) {
            if (line > ed->buf.num_lines) line = ed->buf.num_lines;
            /* Exit preview, jump, re-enter preview */
            editor_toggle_preview(ed);
            ed->cy = line - 1;
            ed->cx = 0;
            editor_toggle_preview(ed);
            editor_set_status(ed, "Jumped to line %d", line);
        } else {
            editor_set_status(ed, "Not a command: %s", cmd);
        }
    }

    free(input);
}
