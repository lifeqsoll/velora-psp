#include "shell/ui.h"
#include "shell/font.h"
#include "shell/path.h"
#include "shell/prefs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAB_RU[] = {"Игры", "Музыка", "Приложения", "Файлы"};
enum { TAB_MAIN_COUNT = 4 };

static const char *VIEW_RU[] = {"Обложки", "Сетка", "Список"};
static const char *CTX_RU[] = {"Копировать", "Переместить", "Удалить", "Переименовать", "Обновить"};
enum { CTX_COUNT = 5 };

static const char SEARCH_CHARS[] =
    "abcdefghijklmnopqrstuvwxyz0123456789 ._-"
    "абвгдеёжзийклмнопрстуфхцчшщъыьэюя";

static void ellipsize(char *dst, size_t dst_sz, const char *src, int max_px)
{
    size_t n;
    if (!dst || dst_sz == 0)
        return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
    if (shell_font_measure(dst) <= max_px)
        return;
    n = strlen(dst);
    while (n > 1) {
        dst[n - 1] = '\0';
        n--;
        if (n >= 3) {
            dst[n - 3] = '.';
            dst[n - 2] = '.';
            dst[n - 1] = '.';
            dst[n] = '\0';
        }
        if (shell_font_measure(dst) <= max_px)
            break;
        if (n >= 3)
            dst[n - 3] = '\0';
        n = strlen(dst);
    }
}

static ShellLibrary *lib_for_tab(ShellApp *app)
{
    switch (app->tab) {
    case SHELL_TAB_MUSIC:
        return &app->music;
    case SHELL_TAB_APPS:
        return &app->apps;
    case SHELL_TAB_GAMES:
    default:
        return &app->games;
    }
}

static ShellLibrary *active_lib(ShellApp *app)
{
    if (app->mode == SHELL_MODE_SEARCH || app->search_q[0] || app->show_favorites_only)
        return &app->filtered;
    return lib_for_tab(app);
}

static void rebuild_filtered(ShellApp *app)
{
    ShellLibrary *src = lib_for_tab(app);
    size_t i;
    shell_library_free(&app->filtered);
    shell_library_init(&app->filtered);

    if (app->show_favorites_only == 2) {
        /* recent */
        for (i = 0; i < (size_t)app->recent_count; ++i) {
            int idx = shell_library_find_path(src, app->recent_paths[i]);
            if (idx >= 0)
                shell_library_add(&app->filtered, src->items[idx].title, src->items[idx].path,
                                  src->items[idx].kind);
        }
        return;
    }

    if (app->search_q[0]) {
        shell_library_filter(src, app->search_q, &app->filtered);
    } else {
        shell_library_filter(src, "", &app->filtered);
    }

    if (app->show_favorites_only == 1) {
        ShellLibrary tmp;
        shell_library_init(&tmp);
        for (i = 0; i < app->filtered.count; ++i) {
            if (app->filtered.items[i].favorite)
                shell_library_add(&tmp, app->filtered.items[i].title, app->filtered.items[i].path,
                                  app->filtered.items[i].kind);
        }
        shell_library_free(&app->filtered);
        app->filtered = tmp;
    }
}

static void push_recent(ShellApp *app, const char *path)
{
    int i, found = -1;
    if (!path || !path[0])
        return;
    for (i = 0; i < app->recent_count; ++i) {
        if (strcmp(app->recent_paths[i], path) == 0) {
            found = i;
            break;
        }
    }
    if (found > 0) {
        char tmp[512];
        strncpy(tmp, app->recent_paths[found], sizeof(tmp) - 1);
        memmove(&app->recent_paths[1], &app->recent_paths[0], (size_t)found * sizeof(app->recent_paths[0]));
        strncpy(app->recent_paths[0], tmp, sizeof(app->recent_paths[0]) - 1);
    } else if (found < 0) {
        if (app->recent_count < 8)
            app->recent_count++;
        memmove(&app->recent_paths[1], &app->recent_paths[0],
                (size_t)(app->recent_count - 1) * sizeof(app->recent_paths[0]));
        strncpy(app->recent_paths[0], path, sizeof(app->recent_paths[0]) - 1);
        app->recent_paths[0][sizeof(app->recent_paths[0]) - 1] = '\0';
    }
    (void)shell_prefs_save_library(&app->games, &app->apps, app->recent_paths, app->recent_count);
}

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
    shell_library_init(&app->filtered);
    shell_files_init(&app->files_a, "ms0:/");
    shell_files_init(&app->files_b, "ms0:/PSP/");
    app->stick_deadzone = 0.25f;
    app->picker_brightness = 0.85f;
    app->running = 1;
    app->tab = SHELL_TAB_GAMES;
    app->tab_before_settings = SHELL_TAB_GAMES;
    app->settings_focus = 0;
    snprintf(app->status, sizeof(app->status), "Velora");
#ifndef HOST_TEST
    {
        FILE *af = fopen("ms0:/PSP/SYSTEM/ownshell/autotest.cfg", "rb");
        if (af) {
            app->autotest = 1;
            fclose(af);
            snprintf(app->status, sizeof(app->status), "AUTOTEST");
        }
    }
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
    shell_library_free(&app->filtered);
    shell_files_free(&app->files_a);
    shell_files_free(&app->files_b);
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
            snprintf(app->status, sizeof(app->status), "Акцент в памяти (cfg не записан)");
        return;
    }
    if (in->repeat & SHELL_BTN_UP)
        app->picker_brightness += 0.05f;
    if (in->repeat & SHELL_BTN_DOWN)
        app->picker_brightness -= 0.05f;
    if (app->picker_brightness < 0.35f)
        app->picker_brightness = 0.35f;
    if (app->picker_brightness > 1.f)
        app->picker_brightness = 1.f;
    if (shell_theme_stick_pick(in->ax, -in->ay, app->stick_deadzone, app->picker_brightness, &accent))
        app->theme.accent = accent;
}

static void apply_preset(ShellApp *app, int idx)
{
    const ShellThemePreset *p = shell_theme_presets();
    if (idx < 0 || idx >= SHELL_THEME_PRESET_COUNT)
        return;
    app->theme.accent = p[idx].accent;
    (void)shell_theme_save(&app->theme, NULL);
    snprintf(app->status, sizeof(app->status), "Пресет: %s", p[idx].name_ru);
}

static void handle_settings(ShellApp *app, const ShellInput *in)
{
    /* 0..4 presets, 5 +, 6 view, 7 wallpaper, 8 HOME preview */
    int max_f = SHELL_THEME_PRESET_COUNT + 3;
    if (in->pressed & SHELL_BTN_CIRCLE) {
        app->tab = app->tab_before_settings;
        if (app->tab >= SHELL_TAB_SETTINGS)
            app->tab = SHELL_TAB_GAMES;
        return;
    }
    if (in->repeat & SHELL_BTN_LEFT) {
        if (app->settings_focus > 0)
            app->settings_focus--;
    }
    if (in->repeat & SHELL_BTN_RIGHT) {
        if (app->settings_focus < max_f)
            app->settings_focus++;
    }
    if (in->repeat & SHELL_BTN_UP || in->repeat & SHELL_BTN_DOWN) {
        if (app->settings_focus == SHELL_THEME_PRESET_COUNT + 1) {
            app->theme.library_view = (app->theme.library_view + 1) % 3;
            (void)shell_theme_save(&app->theme, NULL);
            snprintf(app->status, sizeof(app->status), "Вид: %s", VIEW_RU[app->theme.library_view]);
        } else if (app->settings_focus == SHELL_THEME_PRESET_COUNT + 2) {
            app->theme.wallpaper_id = (app->theme.wallpaper_id + 1) % 3;
            (void)shell_theme_save(&app->theme, NULL);
            snprintf(app->status, sizeof(app->status), "Обои: пресет %d (файлы обоев — след. этап)",
                     app->theme.wallpaper_id);
        }
    }
    if (in->pressed & SHELL_BTN_CROSS) {
        if (app->settings_focus < SHELL_THEME_PRESET_COUNT) {
            apply_preset(app, app->settings_focus);
        } else if (app->settings_focus == SHELL_THEME_PRESET_COUNT) {
            app->theme_backup = app->theme;
            app->mode = SHELL_MODE_COLOR_PICK;
            snprintf(app->status, sizeof(app->status), "Стик: цвет  вверх/вниз: яркость");
        } else if (app->settings_focus == SHELL_THEME_PRESET_COUNT + 1) {
            app->theme.library_view = (app->theme.library_view + 1) % 3;
            (void)shell_theme_save(&app->theme, NULL);
            snprintf(app->status, sizeof(app->status), "Вид: %s", VIEW_RU[app->theme.library_view]);
        } else if (app->settings_focus == SHELL_THEME_PRESET_COUNT + 2) {
            app->theme.wallpaper_id = (app->theme.wallpaper_id + 1) % 3;
            (void)shell_theme_save(&app->theme, NULL);
            snprintf(app->status, sizeof(app->status), "Обои: пресет %d (файлы обоев — след. этап)",
                     app->theme.wallpaper_id);
        } else {
            app->tab = SHELL_TAB_HOME_PREVIEW;
            snprintf(app->status, sizeof(app->status), "Предпросмотр HOME (не плагин)");
        }
    }
}

static ShellFileBrowser *active_files(ShellApp *app)
{
    return app->files_pane ? &app->files_b : &app->files_a;
}

static ShellFileBrowser *other_files(ShellApp *app)
{
    return app->files_pane ? &app->files_a : &app->files_b;
}

static void files_join_focus(ShellFileBrowser *fb, char *out, size_t out_sz)
{
    if (!fb || !out || out_sz == 0)
        return;
    out[0] = '\0';
    if (fb->count == 0 || fb->focus >= fb->count)
        return;
    if (shell_path_join(fb->cwd, fb->items[fb->focus].name, out, out_sz) != SHELL_PATH_OK)
        out[0] = '\0';
}

static void run_confirm(ShellApp *app)
{
#ifndef HOST_TEST
    ShellFileBrowser *src_fb = active_files(app);
    ShellFileBrowser *dst_fb = other_files(app);
    ShellFileStatus st = SHELL_FOP_OK;
    int cancelled = 0;
    char src[512];

    files_join_focus(src_fb, src, sizeof(src));
    if (!src[0] && app->confirm_path[0])
        strncpy(src, app->confirm_path, sizeof(src) - 1);

    switch (app->confirm_kind) {
    case SHELL_CONFIRM_DELETE:
        st = shell_files_delete_safe(src);
        snprintf(app->status, sizeof(app->status),
                 st == SHELL_FOP_OK ? "Удалено" : "Ошибка удаления");
        break;
    case SHELL_CONFIRM_COPY:
        st = shell_files_copy_safe(src, dst_fb->cwd, app->confirm_name, shell_files_copy_stdio,
                                   &cancelled, NULL);
        if (st == SHELL_FOP_OK)
            snprintf(app->status, sizeof(app->status), "Скопировано → %s",
                     app->files_pane ? "A" : "B");
        else if (st == SHELL_FOP_ERR_EXISTS)
            snprintf(app->status, sizeof(app->status), "Уже существует");
        else if (st == SHELL_FOP_ERR_FORBIDDEN)
            snprintf(app->status, sizeof(app->status), "Запись запрещена");
        else
            snprintf(app->status, sizeof(app->status), "Ошибка копирования");
        break;
    case SHELL_CONFIRM_MOVE:
        st = shell_files_move_safe(src, dst_fb->cwd, app->confirm_name, &cancelled);
        if (st == SHELL_FOP_OK)
            snprintf(app->status, sizeof(app->status), "Перемещено");
        else
            snprintf(app->status, sizeof(app->status), "Ошибка перемещения");
        break;
    default:
        break;
    }
    (void)shell_files_refresh(src_fb, shell_files_list_ms, NULL);
    (void)shell_files_refresh(dst_fb, shell_files_list_ms, NULL);
#else
    (void)app;
#endif
    app->confirm_kind = SHELL_CONFIRM_NONE;
    app->mode = SHELL_MODE_BROWSE;
}

static void handle_confirm(ShellApp *app, const ShellInput *in)
{
    if (in->pressed & SHELL_BTN_CIRCLE) {
        app->mode = SHELL_MODE_BROWSE;
        app->confirm_kind = SHELL_CONFIRM_NONE;
        snprintf(app->status, sizeof(app->status), "Отменено");
        return;
    }
    if (in->pressed & SHELL_BTN_CROSS)
        run_confirm(app);
}

static void handle_context(ShellApp *app, const ShellInput *in)
{
    ShellFileBrowser *fb = active_files(app);
    if (in->pressed & SHELL_BTN_CIRCLE) {
        app->mode = SHELL_MODE_BROWSE;
        return;
    }
    if (in->repeat & SHELL_BTN_UP && app->settings_focus > 0)
        app->settings_focus--;
    if (in->repeat & SHELL_BTN_DOWN && app->settings_focus + 1 < CTX_COUNT)
        app->settings_focus++;
    if (!(in->pressed & SHELL_BTN_CROSS))
        return;

    if (app->settings_focus == 4) {
#ifndef HOST_TEST
        (void)shell_files_refresh(fb, shell_files_list_ms, NULL);
#endif
        app->mode = SHELL_MODE_BROWSE;
        snprintf(app->status, sizeof(app->status), "Обновлено");
        return;
    }

    if (fb->count == 0 || fb->focus >= fb->count) {
        app->mode = SHELL_MODE_BROWSE;
        return;
    }

    files_join_focus(fb, app->confirm_path, sizeof(app->confirm_path));
    strncpy(app->confirm_name, fb->items[fb->focus].name, sizeof(app->confirm_name) - 1);

    if (app->settings_focus == 0) {
        app->confirm_kind = SHELL_CONFIRM_COPY;
        app->mode = SHELL_MODE_CONFIRM;
        snprintf(app->status, sizeof(app->status), "Копировать в другую локацию?");
    } else if (app->settings_focus == 1) {
        app->confirm_kind = SHELL_CONFIRM_MOVE;
        app->mode = SHELL_MODE_CONFIRM;
        snprintf(app->status, sizeof(app->status), "Переместить в другую локацию?");
    } else if (app->settings_focus == 2) {
        app->confirm_kind = SHELL_CONFIRM_DELETE;
        app->mode = SHELL_MODE_CONFIRM;
        snprintf(app->status, sizeof(app->status), "Удалить «%s»?", app->confirm_name);
    } else if (app->settings_focus == 3) {
        /* Rename via search charset: reuse search_q as new name buffer */
        app->mode = SHELL_MODE_SEARCH;
        app->search_q[0] = '\0';
        snprintf(app->status, sizeof(app->status), "Новое имя: влево/вправо, X ввод, Start применить");
        app->confirm_kind = SHELL_CONFIRM_NONE;
        app->rename_pending = 1;
    }
}

static void handle_search(ShellApp *app, const ShellInput *in)
{
    static int char_idx;
    size_t nchars = sizeof(SEARCH_CHARS) - 1;
    if (in->pressed & SHELL_BTN_CIRCLE) {
        if (app->rename_pending) {
            app->rename_pending = 0;
            app->search_q[0] = '\0';
            app->mode = SHELL_MODE_BROWSE;
            snprintf(app->status, sizeof(app->status), "Переименование отменено");
            return;
        }
        app->mode = SHELL_MODE_BROWSE;
        rebuild_filtered(app);
        return;
    }
    if (in->pressed & SHELL_BTN_CROSS) {
        size_t n = strlen(app->search_q);
        if (n + 1 < sizeof(app->search_q)) {
            app->search_q[n] = SEARCH_CHARS[char_idx % nchars];
            app->search_q[n + 1] = '\0';
            rebuild_filtered(app);
        }
        return;
    }
    if (in->pressed & SHELL_BTN_SQUARE) {
        size_t n = strlen(app->search_q);
        if (n > 0)
            app->search_q[n - 1] = '\0';
        rebuild_filtered(app);
        return;
    }
    if (in->repeat & SHELL_BTN_LEFT) {
        if (char_idx == 0)
            char_idx = (int)nchars - 1;
        else
            char_idx--;
    }
    if (in->repeat & SHELL_BTN_RIGHT)
        char_idx = (char_idx + 1) % (int)nchars;
    if (in->pressed & SHELL_BTN_START) {
        if (app->rename_pending) {
#ifndef HOST_TEST
            ShellFileStatus st;
            if (!app->search_q[0]) {
                snprintf(app->status, sizeof(app->status), "Пустое имя");
            } else {
                st = shell_files_rename_safe(app->confirm_path, app->search_q);
                if (st == SHELL_FOP_OK) {
                    snprintf(app->status, sizeof(app->status), "Переименовано");
                    (void)shell_files_refresh(active_files(app), shell_files_list_ms, NULL);
                } else if (st == SHELL_FOP_ERR_EXISTS)
                    snprintf(app->status, sizeof(app->status), "Имя занято");
                else if (st == SHELL_FOP_ERR_FORBIDDEN)
                    snprintf(app->status, sizeof(app->status), "Запрещено");
                else
                    snprintf(app->status, sizeof(app->status), "Ошибка переименования");
            }
#endif
            app->rename_pending = 0;
            app->mode = SHELL_MODE_BROWSE;
            app->search_q[0] = '\0';
            return;
        }
        app->mode = SHELL_MODE_BROWSE;
        rebuild_filtered(app);
        return;
    }
    if (app->rename_pending)
        snprintf(app->status, sizeof(app->status), "Имя: %s[%c]", app->search_q,
                 SEARCH_CHARS[char_idx % nchars]);
    else
        snprintf(app->status, sizeof(app->status), "Поиск: %s[%c]", app->search_q,
                 SEARCH_CHARS[char_idx % nchars]);
}

static void handle_files(ShellApp *app, const ShellInput *in)
{
    ShellFileBrowser *fb = active_files(app);
    if (in->pressed & SHELL_BTN_L || in->pressed & SHELL_BTN_R) {
        app->files_pane ^= 1;
        snprintf(app->status, sizeof(app->status), "Локация %s", app->files_pane ? "B" : "A");
        return;
    }
    if (in->repeat & SHELL_BTN_UP && fb->focus > 0)
        fb->focus--;
    if (in->repeat & SHELL_BTN_DOWN && fb->count && fb->focus + 1 < fb->count)
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
            snprintf(app->status, sizeof(app->status), "%s", fb->items[fb->focus].name);
    }
    if (in->pressed & SHELL_BTN_TRI) {
        app->mode = SHELL_MODE_CONTEXT;
        app->settings_focus = 0;
        snprintf(app->status, sizeof(app->status), "Контекстное меню");
    }
    if (in->pressed & SHELL_BTN_SQUARE)
        (void)shell_files_refresh(fb, shell_files_list_ms, NULL);
#else
    (void)fb;
#endif
}

static void handle_home_preview(ShellApp *app, const ShellInput *in)
{
    if (in->pressed & SHELL_BTN_CIRCLE || in->pressed & SHELL_BTN_START) {
        app->tab = SHELL_TAB_SETTINGS;
        snprintf(app->status, sizeof(app->status), "Выход из предпросмотра");
    }
}

static void handle_library(ShellApp *app, const ShellInput *in)
{
    ShellLibrary *lib = active_lib(app);
    int cols = (app->theme.library_view == 1) ? 4 : 1;

    if (in->pressed & SHELL_BTN_SELECT) {
        app->show_favorites_only = (app->show_favorites_only + 1) % 3;
        rebuild_filtered(app);
        if (app->show_favorites_only == 0)
            snprintf(app->status, sizeof(app->status), "Все");
        else if (app->show_favorites_only == 1)
            snprintf(app->status, sizeof(app->status), "Избранное");
        else
            snprintf(app->status, sizeof(app->status), "Недавние");
        return;
    }
    if (in->pressed & SHELL_BTN_TRI && app->tab == SHELL_TAB_GAMES) {
        app->mode = SHELL_MODE_SEARCH;
        snprintf(app->status, sizeof(app->status), "Поиск: влево/вправо буква, X ввод");
        return;
    }
    if (in->pressed & SHELL_BTN_SQUARE && lib->count > 0) {
        ShellLibrary *src = lib_for_tab(app);
        int idx = shell_library_find_path(src, lib->items[lib->focus].path);
        if (idx >= 0) {
            int on = shell_library_toggle_favorite(src, (size_t)idx);
            lib->items[lib->focus].favorite = on;
            (void)shell_prefs_save_library(&app->games, &app->apps, app->recent_paths,
                                           app->recent_count);
            snprintf(app->status, sizeof(app->status), on ? "В избранном" : "Убрано из избранного");
        }
        return;
    }

    if (app->tab == SHELL_TAB_MUSIC || app->theme.library_view == 2) {
        if (in->repeat & SHELL_BTN_UP)
            shell_library_focus_move(lib, -1);
        if (in->repeat & SHELL_BTN_DOWN)
            shell_library_focus_move(lib, 1);
    } else if (app->theme.library_view == 1) {
        if (in->repeat & SHELL_BTN_LEFT)
            shell_library_focus_move(lib, -1);
        if (in->repeat & SHELL_BTN_RIGHT)
            shell_library_focus_move(lib, 1);
        if (in->repeat & SHELL_BTN_UP)
            shell_library_focus_move(lib, -cols);
        if (in->repeat & SHELL_BTN_DOWN)
            shell_library_focus_move(lib, cols);
    } else {
        if (in->repeat & SHELL_BTN_LEFT)
            shell_library_focus_move(lib, -1);
        if (in->repeat & SHELL_BTN_RIGHT)
            shell_library_focus_move(lib, 1);
    }

    if (in->pressed & SHELL_BTN_CROSS && lib->count > 0) {
        const ShellLibEntry *e = &lib->items[lib->focus];
        int rc;
        push_recent(app, e->path);
        if (e->kind == SHELL_ENTRY_MUSIC) {
            snprintf(app->status, sizeof(app->status), "Плеер: следующий этап (локальный список готов)");
            return;
        }
        rc = shell_platform_launch(e->path, e->kind);
        if (rc == 0)
            snprintf(app->status, sizeof(app->status), "Запуск...");
        else if (rc == 1)
            snprintf(app->status, sizeof(app->status), "ISO: нужен CFW на PSP");
        else
            snprintf(app->status, sizeof(app->status), "Не удалось запустить");
    }
}

static void autotest_tick(ShellApp *app)
{
    /* ~90 frames per step (~1.5s at 60Hz): games→music→apps→files→settings→home→covers */
    static const ShellTab steps[] = {
        SHELL_TAB_GAMES, SHELL_TAB_MUSIC, SHELL_TAB_APPS, SHELL_TAB_FILES,
        SHELL_TAB_SETTINGS, SHELL_TAB_HOME_PREVIEW, SHELL_TAB_GAMES
    };
    enum { NSTEP = 7 };
    app->autotest_frames++;
    if (app->autotest_frames < 90)
        return;
    app->autotest_frames = 0;
    app->autotest_step = (app->autotest_step + 1) % NSTEP;
    app->tab = steps[app->autotest_step];
    app->mode = SHELL_MODE_BROWSE;
    app->status[0] = '\0';
    if (app->tab == SHELL_TAB_SETTINGS)
        app->settings_focus = 0;
    if (app->tab == SHELL_TAB_GAMES && app->autotest_step == 6)
        app->theme.library_view = (app->theme.library_view + 1) % 3;
}

void shell_app_update(ShellApp *app, const ShellInput *in)
{
    if (!app || !in)
        return;

    if (app->autotest) {
        autotest_tick(app);
        /* still allow Start to stop autotest */
        if (in->pressed & SHELL_BTN_START) {
            app->autotest = 0;
#ifndef HOST_TEST
            remove("ms0:/PSP/SYSTEM/ownshell/autotest.cfg");
#endif
            snprintf(app->status, sizeof(app->status), "Autotest выкл");
        }
        return;
    }

    if (app->mode == SHELL_MODE_COLOR_PICK) {
        handle_color_pick(app, in);
        return;
    }
    if (app->mode == SHELL_MODE_CONFIRM) {
        handle_confirm(app, in);
        return;
    }
    if (app->mode == SHELL_MODE_CONTEXT) {
        handle_context(app, in);
        return;
    }
    if (app->mode == SHELL_MODE_SEARCH) {
        handle_search(app, in);
        return;
    }

    if (in->pressed & SHELL_BTN_START) {
        if (app->tab == SHELL_TAB_HOME_PREVIEW) {
            handle_home_preview(app, in);
            return;
        }
        if (app->tab != SHELL_TAB_SETTINGS)
            app->tab_before_settings = app->tab;
        app->tab = SHELL_TAB_SETTINGS;
        snprintf(app->status, sizeof(app->status), "O назад  X выбрать  крест пресеты");
        return;
    }

    if (app->tab == SHELL_TAB_SETTINGS) {
        handle_settings(app, in);
        return;
    }
    if (app->tab == SHELL_TAB_HOME_PREVIEW) {
        handle_home_preview(app, in);
        return;
    }

    if (in->repeat & SHELL_BTN_L) {
        if ((int)app->tab > 0)
            app->tab = (ShellTab)(app->tab - 1);
        else
            app->tab = (ShellTab)(TAB_MAIN_COUNT - 1);
        app->status[0] = '\0';
        if (app->show_favorites_only || app->search_q[0])
            rebuild_filtered(app);
    }
    if (in->repeat & SHELL_BTN_R) {
        app->tab = (ShellTab)((app->tab + 1) % TAB_MAIN_COUNT);
        app->status[0] = '\0';
        if (app->show_favorites_only || app->search_q[0])
            rebuild_filtered(app);
    }

    if (app->tab == SHELL_TAB_FILES)
        handle_files(app, in);
    else
        handle_library(app, in);
}
/* —— Visual layer: match concept composition for 480×272 —— */

static ShellRgb COL_PANEL(void) { return (ShellRgb){22, 28, 36}; }
static ShellRgb COL_CARD(void) { return (ShellRgb){28, 34, 44}; }
static ShellRgb COL_MUTED(void) { return (ShellRgb){140, 150, 165}; }
static ShellRgb COL_LINE(void) { return (ShellRgb){40, 50, 65}; }

static void draw_border(int x, int y, int w, int h, ShellRgb c, int t)
{
    if (t < 1)
        t = 1;
    shell_platform_rect(x, y, w, t, c, 0);
    shell_platform_rect(x, y + h - t, w, t, c, 0);
    shell_platform_rect(x, y, t, h, c, 0);
    shell_platform_rect(x + w - t, y, t, h, c, 0);
}

static void draw_frame_fill(int x, int y, int w, int h, ShellRgb fill, ShellRgb border, int radius,
                            int border_t)
{
    shell_platform_rect(x, y, w, h, border, radius);
    if (w > border_t * 2 && h > border_t * 2)
        shell_platform_rect(x + border_t, y + border_t, w - border_t * 2, h - border_t * 2, fill,
                            radius > border_t ? radius - border_t : 0);
}

static void draw_gear(int x, int y, ShellRgb c)
{
    ShellRgb hole = COL_PANEL();
    shell_platform_rect(x + 2, y, 4, 2, c, 0);
    shell_platform_rect(x + 2, y + 6, 4, 2, c, 0);
    shell_platform_rect(x, y + 2, 2, 4, c, 0);
    shell_platform_rect(x + 6, y + 2, 2, 4, c, 0);
    shell_platform_rect(x + 1, y + 1, 6, 6, c, 2);
    shell_platform_rect(x + 3, y + 3, 2, 2, hole, 0);
}

static void draw_wifi_icon(int x, int y, ShellRgb c, int on)
{
    ShellRgb dim = COL_MUTED();
    ShellRgb use = on ? c : dim;
    shell_platform_rect(x + 3, y + 5, 2, 2, use, 0);
    shell_platform_rect(x + 1, y + 3, 6, 1, use, 0);
    shell_platform_rect(x, y + 1, 8, 1, use, 0);
}

static void draw_batt_icon(int x, int y, ShellRgb c, int pct)
{
    ShellRgb dim = COL_MUTED();
    int fill;
    shell_platform_rect(x, y + 1, 12, 7, dim, 1);
    shell_platform_rect(x + 12, y + 3, 2, 3, dim, 0);
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    fill = (pct * 10) / 100;
    if (fill > 0)
        shell_platform_rect(x + 1, y + 2, fill, 5, c, 0);
}

static void draw_hints(ShellApp *app, const char *line)
{
    ShellRgb bar = {14, 18, 24};
    ShellRgb muted = COL_MUTED();
    int y = 252;
    int x = 12;
    int gap = 16;
    int w_key, w_lbl;
    (void)line;
    shell_platform_rect(0, 246, 480, 26, bar, 0);
    shell_platform_rect(0, 245, 480, 1, COL_LINE(), 0);

    w_key = shell_font_measure("X");
    shell_platform_text(x, y, "X", app->theme.accent);
    x += w_key + 4;
    w_lbl = shell_font_measure("Выбрать");
    shell_platform_text(x, y, "Выбрать", app->theme.text);
    x += w_lbl + gap;

    w_key = shell_font_measure("O");
    shell_platform_text(x, y, "O", app->theme.accent);
    x += w_key + 4;
    w_lbl = shell_font_measure("Назад");
    shell_platform_text(x, y, "Назад", app->theme.text);
    x += w_lbl + gap;

    w_key = shell_font_measure("L/R");
    shell_platform_text(x, y, "L/R", app->theme.accent);
    x += w_key + 4;
    w_lbl = shell_font_measure("Разделы");
    shell_platform_text(x, y, "Разделы", app->theme.text);
    x += w_lbl + gap;

    if (app->status[0] && x < 460) {
        char clipped[64];
        int room = 470 - x;
        if (room > 24) {
            ellipsize(clipped, sizeof(clipped), app->status, room);
            shell_platform_text(x, y, clipped, muted);
        }
    }
}

static void draw_chrome(ShellApp *app)
{
    int i, x;
    ShellRgb muted = COL_MUTED();
    char clock[24], batt[24];
    int batt_pct = -1;
    int right_edge = 360; /* tabs must stay left of status cluster */
    int tab_y = 10;
    int lh = shell_font_line_height();

    shell_platform_clear(app->theme.bg.r, app->theme.bg.g, app->theme.bg.b);
    shell_platform_rect(0, 0, 480, 34, COL_PANEL(), 0);

    x = 14;
    for (i = 0; i < TAB_MAIN_COUNT; ++i) {
        int active = (app->tab == (ShellTab)i);
        ShellRgb c = active ? app->theme.accent : app->theme.text;
        int tw = shell_font_measure(TAB_RU[i]);
        if (x + tw > right_edge)
            break;
        shell_platform_text(x, tab_y, TAB_RU[i], c);
        if (active)
            shell_platform_rect(x, 28, tw, 2, app->theme.accent, 1);
        x += tw + 16;
    }

    clock[0] = batt[0] = '\0';
    shell_platform_clock(clock, sizeof(clock));
    shell_platform_battery(batt, sizeof(batt));
    if (batt[0])
        sscanf(batt, "%d", &batt_pct);

    /* Right cluster: clock → batt → wifi → gear (right-aligned) */
    {
        int rx = 468;
        int active = (app->tab == SHELL_TAB_SETTINGS || app->tab == SHELL_TAB_HOME_PREVIEW);
        int cw;
        draw_gear(rx - 10, 12, active ? app->theme.accent : muted);
        rx -= 18;
        draw_wifi_icon(rx - 10, 12, app->theme.accent, 0);
        rx -= 16;
        draw_batt_icon(rx - 14, 12, app->theme.accent, batt_pct >= 0 ? batt_pct : 0);
        rx -= 18;
        if (clock[0]) {
            cw = shell_font_measure(clock);
            shell_platform_text(rx - cw, tab_y, clock, muted);
        }
        (void)lh;
    }

    shell_platform_rect(0, 33, 480, 1, COL_LINE(), 0);
}

static void draw_play_icon(int x, int y, ShellRgb c)
{
    shell_platform_rect(x, y, 2, 8, c, 0);
    shell_platform_rect(x + 2, y + 1, 2, 6, c, 0);
    shell_platform_rect(x + 4, y + 2, 2, 4, c, 0);
    shell_platform_rect(x + 6, y + 3, 2, 2, c, 0);
}

static void draw_cover_row(ShellApp *app, ShellLibrary *lib)
{
    ShellRgb card = COL_CARD();
    ShellRgb slot = {36, 44, 56};
    ShellRgb muted = COL_MUTED();
    size_t i, start = 0;
    int cover_w = 64;
    int cover_h = 90; /* ~2:3; leave room for FT text + pill + footer */
    int gap = 12;
    int visible, row_w, base_x;
    char title[96];
    const char *filter_lbl = NULL;

    if (lib->count == 0)
        return;

    visible = (int)lib->count < 5 ? (int)lib->count : 5;
    if (lib->focus >= 2 && lib->count > 5)
        start = lib->focus - 2;
    if (start + (size_t)visible > lib->count && lib->count >= (size_t)visible)
        start = lib->count - (size_t)visible;

    row_w = visible * cover_w + (visible - 1) * gap;
    base_x = (480 - row_w) / 2;
    if (base_x < 16)
        base_x = 16;

    for (i = start; i < lib->count && (int)(i - start) < visible; ++i) {
        int idx = (int)(i - start);
        int focused = (i == lib->focus);
        int cw = focused ? cover_w + 14 : cover_w;
        int ch = focused ? cover_h + 14 : cover_h;
        int cx = base_x + idx * (cover_w + gap) - (focused ? 7 : 0);
        int cy = focused ? 40 : 46;
        char short_t[20];
        ShellRgb border = focused ? app->theme.accent : COL_LINE();
        int bt = focused ? 2 : 1;

        draw_frame_fill(cx, cy, cw, ch, slot, border, focused ? 12 : 10, bt);
        /* fake cover art blocks — proportion kept, no stretch */
        shell_platform_rect(cx + 8, cy + 10, cw - 16, ch / 3, card, 6);
        ellipsize(short_t, sizeof(short_t), lib->items[i].title, cw - 14);
        shell_platform_text(cx + 6, cy + ch - 20, short_t, app->theme.text);
        if (lib->items[i].favorite)
            shell_platform_rect(cx + cw - 14, cy + 8, 8, 8, app->theme.accent, 4);
    }

    if (lib->focus >= lib->count)
        return;

    ellipsize(title, sizeof(title), lib->items[lib->focus].title, 300);
    shell_platform_text(24, 150, title, app->theme.text);
    if (app->show_favorites_only == 2)
        filter_lbl = "Недавние";
    else if (app->show_favorites_only == 1)
        filter_lbl = "Избранное";
    else
        filter_lbl = "Библиотека";
    shell_platform_text(24, 150 + shell_font_line_height() + 2, filter_lbl, muted);

    /* Pill Launch — solid cyan capsule, dark label (concept) */
    {
        int bw = 168;
        int bx = (480 - bw) / 2;
        int by = 198;
        ShellRgb on_pill = {10, 15, 20};
        int tx;
        shell_platform_rect(bx, by, bw, 28, app->theme.accent, 14);
        draw_play_icon(bx + 18, by + 10, on_pill);
        tx = bx + 36;
        shell_platform_text(tx, by + (28 - shell_font_line_height()) / 2 + 1, "Запустить", on_pill);
    }
}

static void draw_grid(ShellApp *app, ShellLibrary *lib)
{
    ShellRgb card = COL_CARD();
    size_t i, start = 0;
    int cols = 4, cell_w = 104, cell_h = 52, gap = 8;
    char title[48];

    if (lib->count == 0)
        return;
    if (lib->focus >= 8)
        start = (lib->focus / (size_t)cols) * (size_t)cols;
    if (start > 0 && lib->focus < start)
        start = 0;

    for (i = start; i < lib->count && i < start + 8; ++i) {
        int local = (int)(i - start);
        int col = local % cols;
        int row = local / cols;
        int x = 16 + col * (cell_w + gap);
        int y = 40 + row * (cell_h + gap);
        int focused = (i == lib->focus);
        ShellRgb border = focused ? app->theme.accent : COL_LINE();
        draw_frame_fill(x, y, cell_w, cell_h, card, border, 10, focused ? 2 : 1);
        ellipsize(title, sizeof(title), lib->items[i].title, cell_w - 12);
        shell_platform_text(x + 8, y + 18, title, app->theme.text);
    }
}

static void draw_list(ShellApp *app, ShellLibrary *lib)
{
    ShellRgb card = COL_CARD();
    size_t i, start;
    int row = 0;
    char line[120];

    if (lib->count == 0)
        return;
    start = lib->focus > 5 ? lib->focus - 5 : 0;
    for (i = start; i < lib->count && row < 8; ++i, ++row) {
        int focused = (i == lib->focus);
        ShellRgb border = focused ? app->theme.accent : COL_LINE();
        draw_frame_fill(14, 40 + row * 28, 452, 26, card, border, 8, focused ? 2 : 1);
        ellipsize(line, sizeof(line), lib->items[i].title, 400);
        shell_platform_text(24, 46 + row * 28, line, app->theme.text);
        if (lib->items[i].favorite)
            shell_platform_rect(446, 48 + row * 28, 8, 8, app->theme.accent, 4);
    }
}

static void draw_empty_library(ShellApp *app)
{
    ShellRgb muted = COL_MUTED();
    draw_frame_fill(40, 70, 400, 120, COL_CARD(), COL_LINE(), 14, 1);
    if (app->tab == SHELL_TAB_GAMES) {
        shell_platform_text(60, 90, "Библиотека пуста", app->theme.text);
        shell_platform_text(60, 112, "ms0:/ISO   ms0:/PSP/ISO", app->theme.accent);
        shell_platform_text(60, 130, "ef0:/ISO   ef0:/PSP/ISO", muted);
        shell_platform_text(60, 152, "Положите ISO/CSO в одну из папок", muted);
    } else if (app->tab == SHELL_TAB_APPS) {
        shell_platform_text(60, 100, "Нет приложений", app->theme.text);
        shell_platform_text(60, 122, "ms0:/PSP/GAME  — папка с EBOOT.PBP", app->theme.accent);
    } else {
        shell_platform_text(60, 90, "Нет локальной музыки", app->theme.text);
        shell_platform_text(60, 112, "ms0:/MUSIC  ms0:/PSP/MUSIC", app->theme.accent);
        shell_platform_text(60, 134, "Плеер — отдельный этап", muted);
    }
}

static void draw_hslider(int x, int y, int w, int fill_pct, ShellRgb accent, ShellRgb track)
{
    int fw;
    if (fill_pct < 0)
        fill_pct = 0;
    if (fill_pct > 100)
        fill_pct = 100;
    shell_platform_rect(x, y + 5, w, 6, track, 3);
    fw = (w * fill_pct) / 100;
    if (fw > 0)
        shell_platform_rect(x, y + 5, fw, 6, accent, 3);
    shell_platform_rect(x + fw - 5, y + 2, 10, 12, accent, 5);
}

static void draw_home_preview(ShellApp *app)
{
    ShellRgb card = COL_CARD();
    ShellRgb muted = COL_MUTED();
    ShellRgb track = {40, 50, 65};
    char batt[24];
    int i;
    const char *acts[] = {"Сохранить", "Загрузить", "Скриншот", "Выйти"};

    shell_platform_text(14, 36, "Быстрое меню", app->theme.text);
    batt[0] = '\0';
    shell_platform_battery(batt, sizeof(batt));
    if (batt[0])
        shell_platform_text(400, 36, batt, muted);
    else
        shell_platform_text(380, 36, "--%", muted);

    /* Music card */
    draw_frame_fill(12, 54, 250, 88, card, COL_LINE(), 12, 1);
    shell_platform_rect(22, 64, 48, 48, (ShellRgb){45, 55, 70}, 10);
    shell_platform_text(80, 66, "Нет трека", app->theme.text);
    shell_platform_text(80, 84, "Плейлист: —", muted);
    draw_hslider(80, 102, 160, 0, app->theme.accent, track);
    shell_platform_text(80, 122, "<<   |>   >>", app->theme.accent);
    shell_platform_text(200, 122, "Музыка", muted);

    /* Sliders */
    draw_frame_fill(272, 54, 196, 88, card, COL_LINE(), 12, 1);
    shell_platform_text(284, 64, "Яркость", muted);
    draw_hslider(284, 78, 170, 70, app->theme.accent, track);
    shell_platform_text(284, 100, "Громкость", muted);
    draw_hslider(284, 114, 170, 55, app->theme.accent, track);

    /* Action tiles 4 */
    for (i = 0; i < 4; ++i) {
        int x = 12 + i * 116;
        int focused = (i == 0); /* preview only — first highlighted as concept */
        ShellRgb border = focused ? app->theme.accent : COL_LINE();
        draw_frame_fill(x, 152, 108, 40, card, border, 12, focused ? 2 : 1);
        shell_platform_text(x + 12, 164, acts[i], app->theme.text);
    }
    shell_platform_text(12, 202, "* savestate не реализован", muted);
    draw_gear(12, 218, muted);
    shell_platform_text(24, 218, "Доп. инструменты (PRX позже)", muted);
}

static void draw_mini_library_preview(ShellApp *app, int x, int y)
{
    ShellRgb card = COL_CARD();
    int k;
    draw_frame_fill(x, y, 160, 100, card, app->theme.accent, 12, 1);
    for (k = 0; k < 3; ++k) {
        int cx = x + 12 + k * 48;
        int focused = (k == 1);
        int cw = focused ? 40 : 32;
        int ch = focused ? 56 : 48;
        int cy = y + (focused ? 16 : 22);
        draw_frame_fill(cx, cy, cw, ch, (ShellRgb){40, 50, 62},
                        focused ? app->theme.accent : COL_LINE(), 8, focused ? 2 : 1);
    }
    shell_platform_text(x + 36, y + 82, "Превью", COL_MUTED());
}

void shell_app_render(ShellApp *app)
{
    ShellLibrary *lib;
    ShellRgb card = COL_CARD();
    int i;
    if (!app)
        return;

    draw_chrome(app);

    if (app->mode == SHELL_MODE_COLOR_PICK) {
        int k;
        shell_platform_text(16, 40, "Палитра акцента", app->theme.text);
        for (k = 0; k < 12; ++k) {
            float ang = (float)k / 12.f;
            ShellRgb sw = shell_theme_from_hsv(ang, 0.9f, app->picker_brightness);
            int rx = 220 + (int)(52.f * cosf(ang * 6.2831853f));
            int ry = 120 + (int)(40.f * sinf(ang * 6.2831853f));
            shell_platform_rect(rx, ry, 16, 16, sw, 8);
        }
        shell_platform_rect(204, 104, 44, 44, app->theme.accent, 22);
        draw_border(202, 102, 48, 48, app->theme.text, 1);
        shell_platform_text(16, 190, "Стик — цвет   вверх/вниз — яркость", COL_MUTED());
        draw_hints(app, app->status);
        shell_platform_swap();
        return;
    }

    if (app->mode == SHELL_MODE_CONFIRM) {
        draw_frame_fill(50, 80, 380, 100, card, app->theme.accent, 14, 2);
        shell_platform_text(70, 110, app->status, app->theme.text);
        shell_platform_text(70, 140, "X — да     O — нет", app->theme.accent);
        draw_hints(app, "Подтверждение");
        shell_platform_swap();
        return;
    }

    if (app->mode == SHELL_MODE_CONTEXT) {
        draw_frame_fill(130, 48, 220, 32 + CTX_COUNT * 26, card, app->theme.accent, 14, 2);
        shell_platform_text(146, 58, "Меню файла", app->theme.text);
        for (i = 0; i < CTX_COUNT; ++i) {
            if (app->settings_focus == i)
                shell_platform_rect(142, 80 + i * 26, 196, 22, app->theme.accent, 8);
            shell_platform_text(152, 84 + i * 26, CTX_RU[i],
                                app->settings_focus == i ? app->theme.focus_text : app->theme.text);
        }
        draw_hints(app, "");
        shell_platform_swap();
        return;
    }

    if (app->mode == SHELL_MODE_SEARCH) {
        shell_platform_text(16, 40, app->rename_pending ? "Переименование" : "Поиск", app->theme.text);
        shell_platform_text(16, 60, app->status, app->theme.accent);
        if (!app->rename_pending) {
            lib = &app->filtered;
            if (lib->count == 0)
                shell_platform_text(16, 100, "Нет совпадений", COL_MUTED());
            else
                draw_list(app, lib);
        }
        draw_hints(app, "");
        shell_platform_swap();
        return;
    }

    if (app->tab == SHELL_TAB_HOME_PREVIEW) {
        draw_home_preview(app);
        draw_hints(app, "");
        shell_platform_swap();
        return;
    }

    if (app->tab == SHELL_TAB_SETTINGS) {
        const ShellThemePreset *p = shell_theme_presets();
        ShellRgb muted = COL_MUTED();
        shell_platform_text(16, 36, "Оформление", app->theme.text);

        /* Circular accent swatches */
        for (i = 0; i < SHELL_THEME_PRESET_COUNT; ++i) {
            int focused = (app->settings_focus == i);
            int cx = 24 + i * 36;
            shell_platform_rect(cx, 56, 24, 24, p[i].accent, 12);
            if (focused)
                draw_border(cx - 2, 54, 28, 28, app->theme.text, 2);
        }
        {
            int focused = (app->settings_focus == SHELL_THEME_PRESET_COUNT);
            int cx = 24 + SHELL_THEME_PRESET_COUNT * 36;
            shell_platform_rect(cx, 56, 24, 24, COL_CARD(), 12);
            draw_border(cx, 56, 24, 24, focused ? app->theme.accent : COL_LINE(), focused ? 2 : 1);
            shell_platform_text(cx + 8, 62, "+", app->theme.text);
        }

        /* Wallpaper thumbs */
        shell_platform_text(16, 92, "Обои", muted);
        for (i = 0; i < 3; ++i) {
            int focused = (app->settings_focus == SHELL_THEME_PRESET_COUNT + 2 &&
                           app->theme.wallpaper_id == i);
            int wx = 16 + i * 52;
            ShellRgb tint = (i == 0)   ? (ShellRgb){10, 15, 20}
                            : (i == 1) ? (ShellRgb){24, 28, 36}
                                       : (ShellRgb){32, 28, 28};
            draw_frame_fill(wx, 106, 44, 28, tint,
                            focused || app->theme.wallpaper_id == i ? app->theme.accent : COL_LINE(),
                            8, 1);
        }

        /* Layout segmented control */
        {
            int focused_row = (app->settings_focus == SHELL_THEME_PRESET_COUNT + 1);
            int vx = 16;
            shell_platform_text(16, 144, "Вид библиотеки", muted);
            for (i = 0; i < 3; ++i) {
                int on = (app->theme.library_view == i);
                int tw = shell_font_measure(VIEW_RU[i]) + 12;
                draw_frame_fill(vx, 158, tw, 22, on ? app->theme.accent : COL_CARD(),
                                (focused_row && on) ? app->theme.text : COL_LINE(), 8, 1);
                shell_platform_text(vx + 6, 162, VIEW_RU[i], on ? app->theme.focus_text : app->theme.text);
                vx += tw + 6;
            }
        }

        {
            int focused = (app->settings_focus == SHELL_THEME_PRESET_COUNT + 3);
            ShellRgb c = focused ? app->theme.accent : app->theme.text;
            shell_platform_text(16, 190, "Предпросмотр HOME-панели", c);
        }

        draw_mini_library_preview(app, 300, 100);
        draw_hints(app, "");
        shell_platform_swap();
        return;
    }

    if (app->tab == SHELL_TAB_FILES) {
        ShellFileBrowser *fb = active_files(app);
        size_t start, idx;
        int row = 0;
        char path_show[96], sz[24], line[300], fullname[280];
        ShellRgb muted = COL_MUTED();

        shell_platform_text(16, 36, app->files_pane ? "Локация B" : "Локация A", app->theme.accent);
        ellipsize(path_show, sizeof(path_show), fb->cwd, 280);
        shell_platform_text(110, 36, path_show, app->theme.text);
        shell_platform_text(340, 36, "Свободно: неизвестно", muted);

        start = fb->focus > 5 ? fb->focus - 5 : 0;
        for (idx = start; idx < fb->count && row < 7; ++idx, ++row) {
            int focused = (idx == fb->focus);
            ShellRgb border = focused ? app->theme.accent : COL_LINE();
            if (fb->items[idx].is_dir)
                snprintf(line, sizeof(line), "/%s", fb->items[idx].name);
            else {
                shell_files_format_size(fb->items[idx].size, sz, sizeof(sz));
                snprintf(line, sizeof(line), " %s  %s", fb->items[idx].name, sz);
            }
            ellipsize(line, sizeof(line), line, 420);
            draw_frame_fill(12, 56 + row * 28, 456, 26, COL_CARD(), border, 8, focused ? 2 : 1);
            shell_platform_text(22, 62 + row * 28, line, app->theme.text);
        }
        if (fb->count && fb->focus < fb->count) {
            ellipsize(fullname, sizeof(fullname), fb->items[fb->focus].name, 400);
            shell_platform_text(16, 232, fullname, muted);
        } else if (fb->count == 0) {
            shell_platform_text(16, 120, "Папка пуста или недоступна", muted);
        }
        draw_hints(app, "");
        shell_platform_swap();
        return;
    }

    lib = active_lib(app);
    if (lib->count == 0) {
        /* Filtered empty (favorites/recent) still show filter label, not "library empty" */
        if (app->show_favorites_only && app->games.count > 0 &&
            (app->tab == SHELL_TAB_GAMES || app->tab == SHELL_TAB_APPS || app->tab == SHELL_TAB_MUSIC)) {
            ShellRgb muted = COL_MUTED();
            const char *msg = app->show_favorites_only == 2 ? "Нет недавних" : "Нет избранного";
            shell_platform_text(24, 120, msg, app->theme.text);
            shell_platform_text(24, 140, "SELECT — сбросить фильтр", muted);
        } else {
            draw_empty_library(app);
        }
    } else if (app->tab == SHELL_TAB_MUSIC || app->theme.library_view == 2) {
        draw_list(app, lib);
    } else if (app->theme.library_view == 1) {
        draw_grid(app, lib);
    } else {
        draw_cover_row(app, lib);
    }

    draw_hints(app, "");
    shell_platform_swap();
}
