/* command — File browser, URL/link dispatch, local-path file opening. */
#include "command.h"
#include "links.h"
#include "preview_ui.h"
#include "term.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#define MAX_LINKS 512

static int is_local_path(const char *url)
{
    if (url[0] == '#')                      return 0; /* anchor */
    if (strncmp(url, "http://",  7) == 0)   return 0;
    if (strncmp(url, "https://", 8) == 0)   return 0;
    if (strncmp(url, "ftp://",   6) == 0)   return 0;
    if (strncmp(url, "mailto:",  7) == 0)   return 0;
    return 1;
}

static void open_url(Editor *ed, const char *url)
{
    char cmd[600];
#ifdef __APPLE__
    snprintf(cmd, sizeof(cmd), "open '%s' >/dev/null 2>&1", url);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1", url);
#endif
    term_suspend();
    int ret = system(cmd);
    term_resume();
    if (ret != 0)
        editor_set_status(ed, "Failed to open: %s", url);
    else
        editor_set_status(ed, "Opened: %s", url);
}

int command_file_tab_complete(char *buf, int buflen, int maxlen)
{
    const char *partial = buf;

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
        if (strncmp(de->d_name, base, baselen) == 0) {
            snprintf(matches[nmatch], sizeof(matches[nmatch]), "%s", de->d_name);
            nmatch++;
        }
    }
    closedir(dp);

    if (nmatch == 0) return buflen;

    /* Find the longest common prefix among all matches */
    char common[256];
    strncpy(common, matches[0], sizeof(common) - 1);
    common[sizeof(common) - 1] = '\0';
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

    int newlen = snprintf(buf, (size_t)maxlen, "%s", suffix);
    if (newlen < 0 || newlen >= maxlen) return buflen;
    return newlen;
}

void editor_open_file_direct(Editor *ed, const char *path)
{
    if (ed->dirty) {
        editor_set_status(ed, "Unsaved changes — save first (Ctrl+S)");
        return;
    }

    char resolved[PATH_MAX];
    if (path[0] != '/' && ed->filename) {
        /* resolve relative to directory of current file */
        char dir[PATH_MAX];
        strncpy(dir, ed->filename, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        char *slash = strrchr(dir, '/');
        if (slash) {
            slash[1] = '\0';
            snprintf(resolved, sizeof(resolved), "%s%s", dir, path);
        } else {
            strncpy(resolved, path, sizeof(resolved) - 1);
            resolved[sizeof(resolved) - 1] = '\0';
        }
    } else {
        strncpy(resolved, path, sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = '\0';
    }

    int was_preview = ed->preview_mode;
    if (was_preview) editor_toggle_preview(ed);

    buffer_free(&ed->buf);
    buffer_init(&ed->buf);
    undo_stack_clear(&ed->undo);
    undo_stack_clear(&ed->redo);
    ed->cx = 0; ed->cy = 0;
    ed->scroll_y = 0; ed->scroll_x = 0;
    ed->dirty = 0;
    ed->search_query[0] = '\0';
    ed->search_query_len = 0;
    editor_open(ed, resolved);

    if (was_preview) editor_toggle_preview(ed);
}

void editor_open_file_prompt(Editor *ed)
{
    char *filename = editor_prompt(ed, "Open file: ", NULL,
                                   command_file_tab_complete);
    if (!filename) {
        editor_set_status(ed, "");
        return;
    }

    /* Strip leading whitespace */
    char *fn = filename;
    while (*fn && isspace((unsigned char)*fn)) fn++;

    if (!*fn) {
        editor_set_status(ed, "No filename given");
        free(filename);
        return;
    }

    if (ed->dirty) {
        editor_set_status(ed, "Unsaved changes — save first (Ctrl+S)");
        free(filename);
        return;
    }

    /* Exit preview if active, reload file, re-enter preview */
    int was_preview = ed->preview_mode;
    if (was_preview) editor_toggle_preview(ed);

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
    editor_open(ed, fn);

    if (was_preview) editor_toggle_preview(ed);

    free(filename);
}

void editor_open_link_prompt(Editor *ed)
{
    char *input = editor_prompt(ed, "Link #: ", NULL, NULL);
    if (!input) {
        editor_set_status(ed, "");
        return;
    }

    int idx = atoi(input);
    free(input);

    if (idx <= 0) {
        editor_set_status(ed, "Enter a link number (1, 2, ...)");
        return;
    }

    LinkInfo links[MAX_LINKS];
    int count = links_collect(&ed->buf, links, MAX_LINKS);
    if (idx > count) {
        editor_set_status(ed, "No link %d (document has %d link%s)",
                          idx, count, count == 1 ? "" : "s");
        return;
    }

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
    } else if (is_local_path(url)) {
        editor_open_file_direct(ed, url);
    } else {
        open_url(ed, url);
    }
}
