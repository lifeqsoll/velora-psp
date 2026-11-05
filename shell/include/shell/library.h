#ifndef SHELL_LIBRARY_H
#define SHELL_LIBRARY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHELL_ENTRY_GAME_ISO = 0,
    SHELL_ENTRY_HOMEBREW,
    SHELL_ENTRY_MUSIC,
    SHELL_ENTRY_APP
} ShellEntryKind;

typedef struct {
    char title[96];
    char path[512];
    ShellEntryKind kind;
} ShellLibEntry;

typedef struct {
    ShellLibEntry *items;
    size_t count;
    size_t capacity;
    size_t focus;
    size_t scan_cursor; /* incremental */
} ShellLibrary;

void shell_library_init(ShellLibrary *lib);
void shell_library_free(ShellLibrary *lib);

/* Append if capacity allows; returns 0 on success. */
int shell_library_add(ShellLibrary *lib, const char *title, const char *path, ShellEntryKind kind);

void shell_library_focus_move(ShellLibrary *lib, int delta);

/* Case-insensitive substring filter into dst (dst pre-inited). */
void shell_library_filter(const ShellLibrary *src, const char *query, ShellLibrary *dst);

/* Recognise common game file extensions. */
int shell_library_is_iso_name(const char *name);
int shell_library_is_eboot_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_LIBRARY_H */
