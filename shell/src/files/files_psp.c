#include "shell/files.h"
#include "shell/path.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int shell_files_list_ms(const char *dir, ShellDirEnt **out_items, size_t *out_count, void *userdata)
{
    DIR *d;
    struct dirent *e;
    ShellDirEnt *items = NULL;
    size_t count = 0, cap = 0;
    char clean[SHELL_PATH_MAX];
    (void)userdata;

    if (shell_path_sanitize(dir, clean, sizeof(clean)) != SHELL_PATH_OK)
        return -1;
    d = opendir(clean);
    if (!d)
        return -1;
    while ((e = readdir(d)) != NULL) {
        char full[SHELL_PATH_MAX];
        struct stat st;
        ShellDirEnt *nitems;
        if (e->d_name[0] == '.')
            continue;
        if (count >= cap) {
            cap = cap ? cap * 2 : 32;
            nitems = (ShellDirEnt *)realloc(items, cap * sizeof(ShellDirEnt));
            if (!nitems) {
                free(items);
                closedir(d);
                return -1;
            }
            items = nitems;
        }
        memset(&items[count], 0, sizeof(items[count]));
        strncpy(items[count].name, e->d_name, sizeof(items[count].name) - 1);
        if (shell_path_join(clean, e->d_name, full, sizeof(full)) == SHELL_PATH_OK &&
            stat(full, &st) == 0) {
            items[count].is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
            items[count].size = (uint64_t)st.st_size;
        }
        count++;
    }
    closedir(d);
    *out_items = items;
    *out_count = count;
    return 0;
}

ShellFileStatus shell_files_copy_stdio(const char *src, const char *dst, int *cancelled, void *userdata)
{
    FILE *in, *out;
    char buf[4096];
    size_t n;
    (void)userdata;
    if (cancelled)
        *cancelled = 0;
    in = fopen(src, "rb");
    if (!in)
        return SHELL_FOP_ERR_IO;
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return SHELL_FOP_ERR_IO;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (cancelled && *cancelled) {
            fclose(in);
            fclose(out);
            remove(dst);
            return SHELL_FOP_ERR_CANCELLED;
        }
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            remove(dst); /* no fake success on truncated write */
            return SHELL_FOP_ERR_NOSPACE;
        }
    }
    if (ferror(in)) {
        fclose(in);
        fclose(out);
        remove(dst);
        return SHELL_FOP_ERR_IO;
    }
    fclose(in);
    if (fclose(out) != 0) {
        remove(dst);
        return SHELL_FOP_ERR_IO;
    }
    return SHELL_FOP_OK;
}

ShellFileStatus shell_files_delete_safe(const char *path)
{
    char clean[SHELL_PATH_MAX];
    struct stat st;
    if (shell_path_sanitize(path, clean, sizeof(clean)) != SHELL_PATH_OK)
        return SHELL_FOP_ERR_PATH;
    if (shell_path_is_flash_or_system(clean))
        return SHELL_FOP_ERR_FORBIDDEN;
    if (stat(clean, &st) != 0)
        return SHELL_FOP_ERR_IO;
    if (S_ISDIR(st.st_mode)) {
        if (rmdir(clean) != 0)
            return SHELL_FOP_ERR_IO;
    } else if (remove(clean) != 0) {
        return SHELL_FOP_ERR_IO;
    }
    return SHELL_FOP_OK;
}

ShellFileStatus shell_files_rename_safe(const char *src, const char *new_name)
{
    char src_clean[SHELL_PATH_MAX];
    char parent[SHELL_PATH_MAX];
    char dst[SHELL_PATH_MAX];
    char *slash;
    if (!new_name || strchr(new_name, '/') || strchr(new_name, '\\') || strchr(new_name, ':'))
        return SHELL_FOP_ERR_FORBIDDEN;
    if (shell_path_sanitize(src, src_clean, sizeof(src_clean)) != SHELL_PATH_OK)
        return SHELL_FOP_ERR_PATH;
    strncpy(parent, src_clean, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    slash = strrchr(parent, '/');
    if (!slash)
        return SHELL_FOP_ERR_PATH;
    *slash = '\0';
    if (shell_path_join(parent, new_name, dst, sizeof(dst)) != SHELL_PATH_OK)
        return SHELL_FOP_ERR_PATH;
    if (rename(src_clean, dst) != 0)
        return (errno == EEXIST) ? SHELL_FOP_ERR_EXISTS : SHELL_FOP_ERR_IO;
    return SHELL_FOP_OK;
}

ShellFileStatus shell_files_move_safe(const char *src, const char *dst_dir, const char *name,
                                      int *cancelled)
{
    ShellFileStatus st;
    st = shell_files_copy_safe(src, dst_dir, name, shell_files_copy_stdio, cancelled, NULL);
    if (st != SHELL_FOP_OK)
        return st;
    return shell_files_delete_safe(src);
}

int shell_files_free_space(const char *path, uint64_t *out_free)
{
    char clean[SHELL_PATH_MAX];
    if (!out_free)
        return -1;
    *out_free = 0;
    /* Do not invent free space. Real query varies by CFW; leave unknown. */
    if (shell_path_sanitize(path, clean, sizeof(clean)) != SHELL_PATH_OK)
        return -1;
    (void)clean;
    return -1;
}

int shell_files_ensure_dir(const char *path)
{
    char clean[SHELL_PATH_MAX];
    char tmp[SHELL_PATH_MAX];
    size_t i, n;
    if (shell_path_sanitize(path, clean, sizeof(clean)) != SHELL_PATH_OK)
        return -1;
    if (shell_path_is_flash_or_system(clean))
        return -1;
    n = strlen(clean);
    strncpy(tmp, clean, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    /* skip ms0: or ef0: */
    for (i = 4; i <= n; ++i) {
        if (tmp[i] == '/' || tmp[i] == '\0') {
            char save = tmp[i];
            tmp[i] = '\0';
            if (strlen(tmp) > 5)
                mkdir(tmp, 0777);
            tmp[i] = save;
            if (save == '\0')
                break;
        }
    }
    return 0;
}
