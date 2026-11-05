#ifndef SHELL_UI_H
#define SHELL_UI_H

#include "shell/theme.h"
#include "shell/library.h"
#include "shell/files.h"
#include "shell/input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHELL_TAB_GAMES = 0,
    SHELL_TAB_MUSIC,
    SHELL_TAB_APPS,
    SHELL_TAB_FILES,
    SHELL_TAB_SETTINGS,
    SHELL_TAB_COUNT
} ShellTab;

typedef enum {
    SHELL_MODE_BROWSE = 0,
    SHELL_MODE_COLOR_PICK,
    SHELL_MODE_CONFIRM
} ShellUiMode;

typedef struct {
    ShellTab tab;
    ShellUiMode mode;
    ShellTheme theme;
    ShellTheme theme_backup;
    ShellLibrary games;
    ShellLibrary music;
    ShellLibrary apps;
    ShellFileBrowser files_a;
    ShellFileBrowser files_b;
    int files_pane; /* 0=A 1=B */
    float picker_brightness;
    float stick_deadzone;
    int confirm_yes; /* for destructive */
    char status[128];
    int running;
    int experimental_home; /* always 0 in M1 UI toggle storage */
    int experimental_savestate;
} ShellApp;

void shell_app_init(ShellApp *app);
void shell_app_shutdown(ShellApp *app);
void shell_app_update(ShellApp *app, const ShellInput *in);
void shell_app_render(ShellApp *app);

/* Platform hooks implemented in main_psp.c / host stub */
void shell_platform_clear(uint8_t r, uint8_t g, uint8_t b);
void shell_platform_rect(int x, int y, int w, int h, ShellRgb c, int radius);
void shell_platform_text(int x, int y, const char *utf8, ShellRgb c);
void shell_platform_swap(void);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_UI_H */
