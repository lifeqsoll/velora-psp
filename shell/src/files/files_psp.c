#include "shell/files.h"
#include "shell/path.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
