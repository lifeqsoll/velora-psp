#include "shell/library.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void shell_library_init(ShellLibrary *lib)
{
    if (!lib)
        return;
    memset(lib, 0, sizeof(*lib));
}

void shell_library_free(ShellLibrary *lib)
{
    if (!lib)
        return;
    free(lib->items);
    memset(lib, 0, sizeof(*lib));
}

int shell_library_add(ShellLibrary *lib, const char *title, const char *path, ShellEntryKind kind)
{
    ShellLibEntry *nitems;
    size_t ncap, i;
    if (!lib || !title || !path)
        return -1;
    /* Dedupe by basename-ish title or full path */
    for (i = 0; i < lib->count; ++i) {
        if (strcmp(lib->items[i].path, path) == 0)
            return 0;
        if (strcmp(lib->items[i].title, title) == 0 && lib->items[i].kind == kind)
            return 0;
    }
    if (lib->count >= lib->capacity) {
        ncap = lib->capacity ? lib->capacity * 2 : 32;
        nitems = (ShellLibEntry *)realloc(lib->items, ncap * sizeof(ShellLibEntry));
        if (!nitems)
            return -1;
        lib->items = nitems;
        lib->capacity = ncap;
    }
    memset(&lib->items[lib->count], 0, sizeof(ShellLibEntry));
    strncpy(lib->items[lib->count].title, title, sizeof(lib->items[lib->count].title) - 1);
    strncpy(lib->items[lib->count].path, path, sizeof(lib->items[lib->count].path) - 1);
    lib->items[lib->count].kind = kind;
    lib->count++;
    return 0;
}

void shell_library_focus_move(ShellLibrary *lib, int delta)
{
    if (!lib || lib->count == 0)
        return;
    if (delta > 0) {
        lib->focus = (lib->focus + (size_t)delta) % lib->count;
    } else if (delta < 0) {
        size_t d = (size_t)(-delta);
        lib->focus = (lib->focus + lib->count - (d % lib->count)) % lib->count;
    }
}

static void str_tolower_copy(char *dst, size_t dst_sz, const char *src)
{
    size_t i;
    for (i = 0; src[i] && i + 1 < dst_sz; ++i)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

void shell_library_filter(const ShellLibrary *src, const char *query, ShellLibrary *dst)
{
    size_t i;
    char q[96], t[96];
    if (!src || !dst)
        return;
    shell_library_free(dst);
    shell_library_init(dst);
    if (!query || !query[0]) {
        for (i = 0; i < src->count; ++i) {
            shell_library_add(dst, src->items[i].title, src->items[i].path, src->items[i].kind);
            if (dst->count > 0)
                dst->items[dst->count - 1].favorite = src->items[i].favorite;
        }
        dst->focus = src->focus < dst->count ? src->focus : 0;
        return;
    }
    str_tolower_copy(q, sizeof(q), query);
    for (i = 0; i < src->count; ++i) {
        str_tolower_copy(t, sizeof(t), src->items[i].title);
        if (strstr(t, q)) {
            shell_library_add(dst, src->items[i].title, src->items[i].path, src->items[i].kind);
            if (dst->count > 0)
                dst->items[dst->count - 1].favorite = src->items[i].favorite;
        }
    }
}

static int ends_with_ci(const char *name, const char *ext)
{
    size_t n, e, i;
    if (!name || !ext)
        return 0;
    n = strlen(name);
    e = strlen(ext);
    if (n < e)
        return 0;
    for (i = 0; i < e; ++i) {
        char a = (char)tolower((unsigned char)name[n - e + i]);
        char b = (char)tolower((unsigned char)ext[i]);
        if (a != b)
            return 0;
    }
    return 1;
}

int shell_library_is_iso_name(const char *name)
{
    return ends_with_ci(name, ".iso") || ends_with_ci(name, ".cso") || ends_with_ci(name, ".zso") ||
           ends_with_ci(name, ".dax") || ends_with_ci(name, ".jso");
}

int shell_library_is_eboot_name(const char *name)
{
    return ends_with_ci(name, "eboot.pbp");
}

int shell_library_is_music_name(const char *name)
{
    return ends_with_ci(name, ".mp3") || ends_with_ci(name, ".aa3") || ends_with_ci(name, ".oma") ||
           ends_with_ci(name, ".wav");
}

int shell_library_toggle_favorite(ShellLibrary *lib, size_t index)
{
    if (!lib || index >= lib->count)
        return 0;
    lib->items[index].favorite = !lib->items[index].favorite;
    return lib->items[index].favorite;
}

int shell_library_find_path(const ShellLibrary *lib, const char *path)
{
    size_t i;
    if (!lib || !path)
        return -1;
    for (i = 0; i < lib->count; ++i) {
        if (strcmp(lib->items[i].path, path) == 0)
            return (int)i;
    }
    return -1;
}
