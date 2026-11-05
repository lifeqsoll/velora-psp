#include "shell/theme.h"
#include "shell/path.h"

#include <stdio.h>
#include <string.h>

#define THEME_PATH_DEFAULT "ms0:/PSP/SYSTEM/ownshell/theme.cfg"

int shell_theme_save(const ShellTheme *t, const char *path)
{
    char clean[SHELL_PATH_MAX];
    FILE *f;
    if (!t)
        return -1;
    if (!path)
        path = THEME_PATH_DEFAULT;
    if (shell_path_sanitize(path, clean, sizeof(clean)) != SHELL_PATH_OK)
        return -1;
    f = fopen(clean, "wb");
    if (!f)
        return -1;
    fprintf(f, "accent=%u,%u,%u\n", t->accent.r, t->accent.g, t->accent.b);
    fprintf(f, "view=%d\n", t->library_view);
    fprintf(f, "wallpaper=%d\n", t->wallpaper_id);
    fclose(f);
    return 0;
}

int shell_theme_load(ShellTheme *t, const char *path)
{
    char clean[SHELL_PATH_MAX];
    FILE *f;
    unsigned r, g, b;
    int view, wall;
    if (!t)
        return -1;
    shell_theme_default(t);
    if (!path)
        path = THEME_PATH_DEFAULT;
    if (shell_path_sanitize(path, clean, sizeof(clean)) != SHELL_PATH_OK)
        return -1;
    f = fopen(clean, "rb");
    if (!f)
        return -1;
    while (!feof(f)) {
        char line[128];
        if (!fgets(line, sizeof(line), f))
            break;
        if (sscanf(line, "accent=%u,%u,%u", &r, &g, &b) == 3) {
            t->accent.r = (uint8_t)r;
            t->accent.g = (uint8_t)g;
            t->accent.b = (uint8_t)b;
        } else if (sscanf(line, "view=%d", &view) == 1) {
            t->library_view = view;
        } else if (sscanf(line, "wallpaper=%d", &wall) == 1) {
            t->wallpaper_id = wall;
        }
    }
    fclose(f);
    if (!shell_theme_accent_readable(t->accent, t->bg, t->text))
        shell_theme_default(t);
    return 0;
}
