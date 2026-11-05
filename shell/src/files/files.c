#include "shell/files.h"
#include "shell/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void shell_files_init(ShellFileBrowser *fb, const char *root)
{
    char clean[SHELL_PATH_MAX];
    if (!fb)
        return;
    memset(fb, 0, sizeof(*fb));
    if (root && shell_path_sanitize(root, clean, sizeof(clean)) == SHELL_PATH_OK)
        strncpy(fb->cwd, clean, sizeof(fb->cwd) - 1);
    else
        strncpy(fb->cwd, "ms0:/", sizeof(fb->cwd) - 1);
}

void shell_files_free(ShellFileBrowser *fb)
{
    if (!fb)
        return;
    free(fb->items);
    memset(fb, 0, sizeof(*fb));
}

ShellFileStatus shell_files_refresh(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata)
{
    ShellDirEnt *items = NULL;
    size_t count = 0;
    char clean[SHELL_PATH_MAX];
    if (!fb || !list_fn)
        return SHELL_FOP_ERR_IO;
    if (shell_path_sanitize(fb->cwd, clean, sizeof(clean)) != SHELL_PATH_OK)
        return SHELL_FOP_ERR_PATH;
    strncpy(fb->cwd, clean, sizeof(fb->cwd) - 1);
    if (list_fn(fb->cwd, &items, &count, userdata) != 0)
        return SHELL_FOP_ERR_IO;
    free(fb->items);
    fb->items = items;
    fb->count = count;
    fb->capacity = count;
    if (fb->focus >= fb->count)
        fb->focus = fb->count ? fb->count - 1 : 0;
    return SHELL_FOP_OK;
}

ShellFileStatus shell_files_enter(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata)
{
    char next[SHELL_PATH_MAX];
    if (!fb || fb->focus >= fb->count)
        return SHELL_FOP_ERR_IO;
    if (!fb->items[fb->focus].is_dir)
        return SHELL_FOP_OK; /* open file handled by UI */
    if (shell_path_join(fb->cwd, fb->items[fb->focus].name, next, sizeof(next)) != SHELL_PATH_OK)
        return SHELL_FOP_ERR_PATH;
    strncpy(fb->cwd, next, sizeof(fb->cwd) - 1);
    fb->focus = 0;
    return shell_files_refresh(fb, list_fn, userdata);
}

ShellFileStatus shell_files_up(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata)
{
    char parent[SHELL_PATH_MAX];
    char *slash;
    size_t root_len;
    if (!fb)
        return SHELL_FOP_ERR_IO;
    if (shell_path_sanitize(fb->cwd, parent, sizeof(parent)) != SHELL_PATH_OK)
        return SHELL_FOP_ERR_PATH;
    root_len = 5; /* "ms0:/" or "ef0:/" */
    if (strlen(parent) <= root_len)
        return SHELL_FOP_OK; /* already at root */
    slash = strrchr(parent, '/');
    if (!slash)
        return SHELL_FOP_ERR_PATH;
    if (slash <= parent + 4) {
        parent[5] = '\0';
    } else {
        *slash = '\0';
    }
    strncpy(fb->cwd, parent, sizeof(fb->cwd) - 1);
    fb->focus = 0;
    return shell_files_refresh(fb, list_fn, userdata);
}

ShellFileStatus shell_files_copy_safe(const char *src, const char *dst_dir, const char *name,
                                      ShellCopyFn copy_fn, int *cancelled, void *userdata)
{
    char src_clean[SHELL_PATH_MAX];
    char dst_clean[SHELL_PATH_MAX];
    if (!src || !dst_dir || !name || !copy_fn)
        return SHELL_FOP_ERR_IO;
    if (shell_path_sanitize(src, src_clean, sizeof(src_clean)) != SHELL_PATH_OK)
        return SHELL_FOP_ERR_PATH;
    if (shell_path_join(dst_dir, name, dst_clean, sizeof(dst_clean)) != SHELL_PATH_OK)
        return SHELL_FOP_ERR_PATH;
    if (shell_path_is_flash_or_system(dst_clean) || shell_path_is_flash_or_system(src_clean))
        return SHELL_FOP_ERR_FORBIDDEN;
    if (cancelled)
        *cancelled = 0;
    return copy_fn(src_clean, dst_clean, cancelled, userdata);
}
