#include "shell/prefs.h"
#include "shell/path.h"
#include "shell/files.h"

#include <stdio.h>
#include <string.h>

#define PREFS_PATH "ms0:/PSP/SYSTEM/ownshell/library.cfg"
#define PREFS_DIR "ms0:/PSP/SYSTEM/ownshell"

static void mark_fav(ShellLibrary *lib, const char *path)
{
    int idx;
    if (!lib || !path)
        return;
    idx = shell_library_find_path(lib, path);
    if (idx >= 0)
        lib->items[idx].favorite = 1;
}

int shell_prefs_save_library(const ShellLibrary *games, const ShellLibrary *apps,
                             const char recent_paths[][512], int recent_count)
{
    char clean[SHELL_PATH_MAX];
    FILE *f;
    size_t i;
#ifndef HOST_TEST
    (void)shell_files_ensure_dir(PREFS_DIR);
#endif
    if (shell_path_sanitize(PREFS_PATH, clean, sizeof(clean)) != SHELL_PATH_OK)
        return -1;
    f = fopen(clean, "wb");
    if (!f)
        return -1;
    if (games) {
        for (i = 0; i < games->count; ++i) {
            if (games->items[i].favorite)
                fprintf(f, "fav=%s\n", games->items[i].path);
        }
    }
    if (apps) {
        for (i = 0; i < apps->count; ++i) {
            if (apps->items[i].favorite)
                fprintf(f, "fav=%s\n", apps->items[i].path);
        }
    }
    for (i = 0; i < (size_t)recent_count && i < 8; ++i)
        fprintf(f, "recent=%s\n", recent_paths[i]);
    fclose(f);
    return 0;
}

int shell_prefs_load_library(ShellLibrary *games, ShellLibrary *apps,
                             char recent_paths[][512], int *recent_count, int recent_cap)
{
    char clean[SHELL_PATH_MAX];
    FILE *f;
    if (recent_count)
        *recent_count = 0;
    if (shell_path_sanitize(PREFS_PATH, clean, sizeof(clean)) != SHELL_PATH_OK)
        return -1;
    f = fopen(clean, "rb");
    if (!f)
        return -1;
    while (!feof(f)) {
        char line[600];
        char *nl;
        if (!fgets(line, sizeof(line), f))
            break;
        nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';
        if (strncmp(line, "fav=", 4) == 0) {
            mark_fav(games, line + 4);
            mark_fav(apps, line + 4);
        } else if (strncmp(line, "recent=", 7) == 0 && recent_paths && recent_count &&
                   *recent_count < recent_cap) {
            strncpy(recent_paths[*recent_count], line + 7, 511);
            recent_paths[*recent_count][511] = '\0';
            (*recent_count)++;
        }
    }
    fclose(f);
    return 0;
}
