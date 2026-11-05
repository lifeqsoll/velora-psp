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
    SHELL_TAB_HOME_PREVIEW, /* labeled preview only */
    SHELL_TAB_COUNT
} ShellTab;

typedef enum {
    SHELL_MODE_BROWSE = 0,
    SHELL_MODE_COLOR_PICK,
    SHELL_MODE_CONFIRM,
    SHELL_MODE_SEARCH,
    SHELL_MODE_CONTEXT
} ShellUiMode;

typedef enum {
    SHELL_CONFIRM_NONE = 0,
    SHELL_CONFIRM_DELETE,
    SHELL_CONFIRM_OVERWRITE,
    SHELL_CONFIRM_COPY,
    SHELL_CONFIRM_MOVE
} ShellConfirmKind;

typedef struct {
    ShellTab tab;
    ShellTab tab_before_settings;
    ShellUiMode mode;
    ShellTheme theme;
    ShellTheme theme_backup;
    ShellLibrary games;
    ShellLibrary music;
    ShellLibrary apps;
    ShellLibrary filtered; /* search results */
    ShellFileBrowser files_a;
    ShellFileBrowser files_b;
    int files_pane;
    float picker_brightness;
    float stick_deadzone;
    int settings_focus;
    ShellConfirmKind confirm_kind;
    char confirm_path[512];
    char confirm_name[256];
    char search_q[48];
    char status[160];
    char recent_paths[8][512];
    int recent_count;
    int show_favorites_only;
    int running;
    int experimental_home;
    int experimental_savestate; /* UI flag only — not a claim of savestate support */
    int rename_pending;         /* search buffer used as new filename */
    /* Autotest: ms0:/PSP/SYSTEM/ownshell/autotest.cfg cycles tabs (dev only) */
    int autotest;
    int autotest_frames;
    int autotest_step;
} ShellApp;

void shell_app_init(ShellApp *app);
void shell_app_shutdown(ShellApp *app);
void shell_app_update(ShellApp *app, const ShellInput *in);
void shell_app_render(ShellApp *app);

void shell_platform_clear(uint8_t r, uint8_t g, uint8_t b);
void shell_platform_rect(int x, int y, int w, int h, ShellRgb c, int radius);
void shell_platform_px(int x, int y, ShellRgb c, uint8_t alpha);
void shell_platform_text(int x, int y, const char *utf8, ShellRgb c);
void shell_platform_swap(void);
int shell_platform_launch(const char *path, ShellEntryKind kind);
void shell_platform_clock(char *buf, size_t n); /* "" if unknown */
void shell_platform_battery(char *buf, size_t n); /* "" if unknown */

#ifdef __cplusplus
}
#endif

#endif
