#include "shell/ui.h"

#include <stdio.h>
#include <string.h>

static const char *TAB_RU[SHELL_TAB_COUNT] = {
    "Игры", "Музыка", "Приложения", "Файлы", "Настройки"
};

void shell_app_init(ShellApp *app)
{
    if (!app)
        return;
    memset(app, 0, sizeof(*app));
    shell_theme_default(&app->theme);
    (void)shell_theme_load(&app->theme, NULL);
    app->theme_backup = app->theme;
    shell_library_init(&app->games);
    shell_library_init(&app->music);
    shell_library_init(&app->apps);
    shell_files_init(&app->files_a, "ms0:/");
    shell_files_init(&app->files_b, "ms0:/PSP/");
    app->stick_deadzone = 0.25f;
    app->picker_brightness = 0.85f;
    app->running = 1;
    app->tab = SHELL_TAB_GAMES;
    snprintf(app->status, sizeof(app->status), "OwnShell M1");
#ifndef HOST_TEST
    (void)shell_files_refresh(&app->files_a, shell_files_list_ms, NULL);
    (void)shell_files_refresh(&app->files_b, shell_files_list_ms, NULL);
#endif
}

void shell_app_shutdown(ShellApp *app)
{
    if (!app)
        return;
    shell_library_free(&app->games);
    shell_library_free(&app->music);
    shell_library_free(&app->apps);
    shell_files_free(&app->files_a);
    shell_files_free(&app->files_b);
}

static ShellLibrary *current_lib(ShellApp *app)
{
    switch (app->tab) {
    case SHELL_TAB_MUSIC:
        return &app->music;
    case SHELL_TAB_APPS:
        return &app->apps;
    default:
        return &app->games;
    }
}

static void handle_color_pick(ShellApp *app, const ShellInput *in)
{
    ShellRgb accent = app->theme.accent;
    if (in->pressed & SHELL_BTN_CIRCLE) {
        app->theme = app->theme_backup;
        app->mode = SHELL_MODE_BROWSE;
        snprintf(app->status, sizeof(app->status), "Тема отменена");
        return;
    }
    if (in->pressed & SHELL_BTN_CROSS) {
        app->mode = SHELL_MODE_BROWSE;
        if (shell_theme_save(&app->theme, NULL) == 0)
            snprintf(app->status, sizeof(app->status), "Акцент сохранён");
        else
            snprintf(app->status, sizeof(app->status), "Акцент в памяти (запись cfg не удалась)");
        return;
    }
    if (in->pressed & SHELL_BTN_UP)
        app->picker_brightness += 0.05f;
    if (in->pressed & SHELL_BTN_DOWN)
        app->picker_brightness -= 0.05f;
    if (app->picker_brightness < 0.35f)
        app->picker_brightness = 0.35f;
    if (app->picker_brightness > 1.f)
        app->picker_brightness = 1.f;
    if (shell_theme_stick_pick(in->ax, -in->ay, app->stick_deadzone, app->picker_brightness, &accent))
        app->theme.accent = accent;
}

static void handle_browse(ShellApp *app, const ShellInput *in)
{
    ShellLibrary *lib;
    if (in->pressed & SHELL_BTN_L) {
        app->tab = (ShellTab)((app->tab + SHELL_TAB_COUNT - 1) % SHELL_TAB_COUNT);
    }
    if (in->pressed & SHELL_BTN_R) {
        app->tab = (ShellTab)((app->tab + 1) % SHELL_TAB_COUNT);
    }

    if (app->tab == SHELL_TAB_SETTINGS) {
        if (in->pressed & SHELL_BTN_CROSS) {
            app->theme_backup = app->theme;
            app->mode = SHELL_MODE_COLOR_PICK;
            snprintf(app->status, sizeof(app->status), "Палитра: стик=цвет, вверх/вниз=яркость, X=OK, O=отмена");
        }
        return;
    }

    if (app->tab == SHELL_TAB_FILES) {
        ShellFileBrowser *fb = app->files_pane ? &app->files_b : &app->files_a;
        if (in->pressed & SHELL_BTN_SELECT)
            app->files_pane ^= 1;
        if (in->pressed & SHELL_BTN_L || in->pressed & SHELL_BTN_R)
            app->files_pane ^= 1;
        if (in->pressed & SHELL_BTN_UP && fb->focus > 0)
            fb->focus--;
        if (in->pressed & SHELL_BTN_DOWN && fb->count && fb->focus + 1 < fb->count)
            fb->focus++;
#ifndef HOST_TEST
        if (in->pressed & SHELL_BTN_CIRCLE)
            (void)shell_files_up(fb, shell_files_list_ms, NULL);
        if (in->pressed & SHELL_BTN_CROSS) {
            if (fb->count == 0)
                (void)shell_files_refresh(fb, shell_files_list_ms, NULL);
            else if (fb->items[fb->focus].is_dir)
                (void)shell_files_enter(fb, shell_files_list_ms, NULL);
            else
                snprintf(app->status, sizeof(app->status), "Файл: %s", fb->items[fb->focus].name);
        }
        if (in->pressed & SHELL_BTN_TRI) {
            snprintf(app->status, sizeof(app->status),
                     "Контекст: copy/rename/delete — confirm в след. итерации");
        }
        if (in->pressed & SHELL_BTN_SQUARE)
            (void)shell_files_refresh(fb, shell_files_list_ms, NULL);
#endif
        return;
    }

    lib = current_lib(app);
    if (in->pressed & SHELL_BTN_LEFT)
        shell_library_focus_move(lib, -1);
    if (in->pressed & SHELL_BTN_RIGHT)
        shell_library_focus_move(lib, 1);
    if (in->pressed & SHELL_BTN_UP)
        shell_library_focus_move(lib, -1);
    if (in->pressed & SHELL_BTN_DOWN)
        shell_library_focus_move(lib, 1);
}

void shell_app_update(ShellApp *app, const ShellInput *in)
{
    if (!app || !in)
        return;
    if (in->pressed & SHELL_BTN_START) {
        /* reserved */
    }
    switch (app->mode) {
    case SHELL_MODE_COLOR_PICK:
        handle_color_pick(app, in);
        break;
    default:
        handle_browse(app, in);
        break;
    }
}

void shell_app_render(ShellApp *app)
{
    int i, x;
    ShellLibrary *lib;
    ShellRgb panel = {40, 44, 50};
    ShellRgb card = {48, 52, 58};
    if (!app)
        return;

    shell_platform_clear(app->theme.bg.r, app->theme.bg.g, app->theme.bg.b);

    /* Outer HOME-like panel with larger radius */
    shell_platform_rect(8, 8, 464, 256, panel, 14);

    /* Tabs */
    x = 16;
    for (i = 0; i < SHELL_TAB_COUNT; ++i) {
        ShellRgb c = (i == (int)app->tab) ? app->theme.accent : app->theme.text;
        shell_platform_text(x, 18, TAB_RU[i], c);
        x += 88;
    }

    shell_platform_rect(16, 40, 448, 1, app->theme.accent, 0);

    if (app->mode == SHELL_MODE_COLOR_PICK) {
        shell_platform_text(24, 60, "Круговая палитра (стик)", app->theme.text);
        shell_platform_rect(200, 110, 80, 80, app->theme.accent, 40);
        shell_platform_text(24, 210, app->status, app->theme.text);
        shell_platform_swap();
        return;
    }

    if (app->tab == SHELL_TAB_SETTINGS) {
        const ShellThemePreset *p = shell_theme_presets();
        shell_platform_text(24, 56, "Настройки темы", app->theme.text);
        shell_platform_text(24, 80, "X — круговая палитра", app->theme.text);
        for (i = 0; i < SHELL_THEME_PRESET_COUNT; ++i) {
            shell_platform_rect(24 + i * 70, 120, 56, 28, p[i].accent, 6);
            shell_platform_text(24 + i * 70, 154, p[i].name_ru, app->theme.text);
        }
        shell_platform_text(24, 200, "HOME/savestate флаги выкл (M1)", app->theme.text);
        shell_platform_text(24, 230, app->status, app->theme.text);
        shell_platform_swap();
        return;
    }

    if (app->tab == SHELL_TAB_FILES) {
        ShellFileBrowser *fb = app->files_pane ? &app->files_b : &app->files_a;
        size_t start, idx;
        int row = 0;
        shell_platform_text(24, 56, app->files_pane ? "Файлы B" : "Файлы A", app->theme.text);
        shell_platform_text(24, 72, fb->cwd, app->theme.accent);
        start = fb->focus > 3 ? fb->focus - 3 : 0;
        for (idx = start; idx < fb->count && row < 5; ++idx, ++row) {
            char line[300];
            int focused = (idx == fb->focus);
            ShellRgb bgc = focused ? app->theme.accent : card;
            ShellRgb tc = focused ? app->theme.focus_text : app->theme.text;
            snprintf(line, sizeof(line), "%s%s", fb->items[idx].is_dir ? "[D] " : "    ",
                     fb->items[idx].name);
            shell_platform_rect(24, 92 + row * 22, 420, 20, bgc, 6);
            shell_platform_text(32, 96 + row * 22, line, tc);
        }
        if (fb->count == 0)
            shell_platform_text(24, 120, "Square — обновить список", app->theme.text);
        shell_platform_text(24, 220, "O вверх  X открыть  L/R панель  [] меню", app->theme.text);
        shell_platform_text(24, 240, app->status, app->theme.text);
        shell_platform_swap();
        return;
    }

    lib = current_lib(app);
    shell_platform_text(24, 56, "Библиотека", app->theme.text);
    if (lib->count == 0) {
        shell_platform_text(24, 100, "Нет записей — скан ms0:/ISO и /PSP/GAME", app->theme.text);
    } else {
        size_t start = lib->focus > 2 ? lib->focus - 2 : 0;
        size_t idx;
        int row = 0;
        for (idx = start; idx < lib->count && row < 5; ++idx, ++row) {
            int focused = (idx == lib->focus);
            ShellRgb bgc = focused ? app->theme.accent : card;
            ShellRgb tc = focused ? app->theme.focus_text : app->theme.text;
            shell_platform_rect(24, 88 + row * 28, 420, 24, bgc, 6);
            shell_platform_text(32, 94 + row * 28, lib->items[idx].title, tc);
        }
    }
    shell_platform_text(24, 240, app->status, app->theme.text);
    shell_platform_swap();
}
