#ifndef SHELL_THEME_H
#define SHELL_THEME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t r, g, b;
} ShellRgb;

typedef struct {
    ShellRgb accent;
    ShellRgb bg;          /* graphite */
    ShellRgb text;        /* light */
    ShellRgb focus_text;  /* on accent / selection */
    int library_view;     /* 0 horizontal covers, 1 grid, 2 list */
    int wallpaper_id;
} ShellTheme;

typedef struct {
    const char *name_ru;
    ShellRgb accent;
} ShellThemePreset;

#define SHELL_THEME_PRESET_COUNT 5

const ShellThemePreset *shell_theme_presets(void);

void shell_theme_default(ShellTheme *t);

/* Stick polar → HSV accent. angle_rad [0, 2π), sat [0,1], value [0,1]. */
ShellRgb shell_theme_from_hsv(float hue_turns, float sat, float value);

/* Minimum relative luminance contrast between accent and bg/text. Returns 1 if OK. */
int shell_theme_accent_readable(ShellRgb accent, ShellRgb bg, ShellRgb text);

/* Apply stick sample with deadzone; returns 1 if accent changed. */
int shell_theme_stick_pick(float stick_x, float stick_y, float deadzone,
                           float brightness, ShellRgb *inout_accent);

/* Persist to ms0/ef0 only (path sanitized). Returns 0 on success. */
int shell_theme_save(const ShellTheme *t, const char *path);
int shell_theme_load(ShellTheme *t, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_THEME_H */
