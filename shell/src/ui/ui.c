#include "shell/ui.h"
#include "shell/font.h"
#include "shell/path.h"

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
    /* 0..4 presets, 5 +, 6 view, 7 HOME preview */
    int max_f = SHELL_THEME_PRESET_COUNT + 2;
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

static void draw_hints(ShellApp *app, const char *line)
{
    ShellRgb bar = {30, 32, 36};
    char clipped[120];
    shell_platform_rect(6, 250, 468, 18, bar, 0);
    ellipsize(clipped, sizeof(clipped), line, 450);
    shell_platform_text(12, 254, clipped, app->theme.text);
}

static void draw_gear(int x, int y, ShellRgb c)
{
    /* Compact gear: ring + teeth, no glow */
    ShellRgb hole = {36, 38, 42};
    shell_platform_rect(x + 2, y, 4, 2, c, 0);
    shell_platform_rect(x + 2, y + 6, 4, 2, c, 0);
    shell_platform_rect(x, y + 2, 2, 4, c, 0);
    shell_platform_rect(x + 6, y + 2, 2, 4, c, 0);
    shell_platform_rect(x + 1, y + 1, 6, 6, c, 2);
    shell_platform_rect(x + 3, y + 3, 2, 2, hole, 0);
}

static void draw_chrome(ShellApp *app)
{
    int i, x;
    ShellRgb panel = {36, 38, 42};
    ShellRgb muted = {160, 164, 170};
    char clock[24], batt[24], sys[64];

    shell_platform_clear(app->theme.bg.r, app->theme.bg.g, app->theme.bg.b);
    shell_platform_rect(4, 4, 472, 264, panel, 10);

    x = 12;
    for (i = 0; i < TAB_MAIN_COUNT; ++i) {
        int active = (app->tab == (ShellTab)i);
        ShellRgb c = active ? app->theme.accent : app->theme.text;
        int tw = shell_font_measure(TAB_RU[i]);
        shell_platform_text(x, 10, TAB_RU[i], c);
        if (active)
            shell_platform_rect(x, 22, tw, 2, app->theme.accent, 0);
        x += tw + 14;
    }

    {
        int active = (app->tab == SHELL_TAB_SETTINGS || app->tab == SHELL_TAB_HOME_PREVIEW);
        ShellRgb c = active ? app->theme.accent : muted;
        draw_gear(456, 10, c);
        if (active)
            shell_platform_rect(456, 22, 8, 2, app->theme.accent, 0);
    }

    clock[0] = batt[0] = '\0';
    shell_platform_clock(clock, sizeof(clock));
    shell_platform_battery(batt, sizeof(batt));
    sys[0] = '\0';
    if (clock[0] && batt[0])
        snprintf(sys, sizeof(sys), "%s %s", clock, batt);
    else if (clock[0])
        snprintf(sys, sizeof(sys), "%s", clock);
    else if (batt[0])
        snprintf(sys, sizeof(sys), "%s", batt);
    if (sys[0])
        shell_platform_text(360, 10, sys, muted);

    shell_platform_rect(12, 28, 456, 1, app->theme.accent, 0);
}

static void draw_cover_row(ShellApp *app, ShellLibrary *lib)
{
    ShellRgb card = {48, 52, 58};
    ShellRgb slot = {62, 66, 72};
    ShellRgb wash = {42, 48, 54};
    size_t i, start = 0;
    int cover_w = 86;
    int cover_h = 110;
    int gap = 10;
    int row_w, base_x, visible;
    char title[96];
    ShellRgb muted = {150, 154, 160};

    if (lib->count == 0)
        return;

    /* soft wash behind covers — muted, not glass */
    shell_platform_rect(12, 34, 456, 150, wash, 8);

    visible = (int)lib->count < 4 ? (int)lib->count : 4;
    if (lib->focus >= 2 && lib->count > 4)
        start = lib->focus - 1;
    if (start + (size_t)visible > lib->count && lib->count >= (size_t)visible)
        start = lib->count - (size_t)visible;

    row_w = visible * cover_w + (visible - 1) * gap;
    base_x = 12 + (456 - row_w) / 2;
    if (base_x < 16)
        base_x = 16;

    for (i = start; i < lib->count && (int)(i - start) < visible; ++i) {
        int idx = (int)(i - start);
        int focused = (i == lib->focus);
        int cw = focused ? cover_w + 12 : cover_w;
        int ch = focused ? cover_h + 12 : cover_h;
        int cx = base_x + idx * (cover_w + gap) - (focused ? 6 : 0);
        int cy = focused ? 42 : 48;
        ShellRgb border = focused ? app->theme.accent : card;
        const char *t = lib->items[i].title;
        shell_platform_rect(cx - 3, cy - 3, cw + 6, ch + 6, border, focused ? 10 : 8);
        shell_platform_rect(cx, cy, cw, ch, slot, focused ? 8 : 6);
        {
            char short_t[24];
            ellipsize(short_t, sizeof(short_t), t, cw - 12);
            shell_platform_text(cx + 6, cy + ch / 2 - 4, short_t, app->theme.text);
        }
        if (lib->items[i].favorite)
            shell_platform_rect(cx + 6, cy + 6, 6, 6, app->theme.accent, 2);
    }

    if (lib->focus < lib->count) {
        const char *action = (lib->items[lib->focus].kind == SHELL_ENTRY_MUSIC) ? "Выбрать" : "Запустить";
        int aw;
        ellipsize(title, sizeof(title), lib->items[lib->focus].title, 360);
        shell_platform_text(20, 196, title, app->theme.text);
        aw = shell_font_measure(action) + 16;
        shell_platform_rect(18, 214, aw, 20, app->theme.accent, 5);
        shell_platform_text(26, 218, action, app->theme.focus_text);
        {
            char meta[48];
            snprintf(meta, sizeof(meta), "%u / %u", (unsigned)(lib->focus + 1), (unsigned)lib->count);
            shell_platform_text(18 + aw + 10, 218, meta, muted);
        }
    }
}

static void draw_grid(ShellApp *app, ShellLibrary *lib)
{
    ShellRgb card = {48, 52, 58};
    ShellRgb slot = {58, 62, 68};
    size_t i;
    int cols = 4, cell_w = 100, cell_h = 48, gap = 6;
    size_t start = 0;
    char title[48];

    if (lib->count == 0)
        return;
    if (lib->focus >= 8)
        start = (lib->focus / (size_t)cols - 1) * (size_t)cols;

    for (i = start; i < lib->count && i < start + 8; ++i) {
        int local = (int)(i - start);
        int col = local % cols;
        int row = local / cols;
        int x = 18 + col * (cell_w + gap);
        int y = 40 + row * (cell_h + gap);
        int focused = (i == lib->focus);
        ShellRgb b = focused ? app->theme.accent : card;
        shell_platform_rect(x - 1, y - 1, cell_w + 2, cell_h + 2, b, 5);
        shell_platform_rect(x, y, cell_w, cell_h, slot, 4);
        ellipsize(title, sizeof(title), lib->items[i].title, cell_w - 12);
        shell_platform_text(x + 6, y + 18, title, focused ? app->theme.focus_text : app->theme.text);
    }
}

static void draw_list(ShellApp *app, ShellLibrary *lib)
{
    ShellRgb card = {48, 52, 58};
    size_t i, start;
    int row = 0;
    char line[120];

    if (lib->count == 0)
        return;
    start = lib->focus > 4 ? lib->focus - 4 : 0;
    for (i = start; i < lib->count && row < 7; ++i, ++row) {
        int focused = (i == lib->focus);
        ShellRgb bgc = focused ? app->theme.accent : card;
        ShellRgb tc = focused ? app->theme.focus_text : app->theme.text;
        ellipsize(line, sizeof(line), lib->items[i].title, 380);
        shell_platform_rect(16, 38 + row * 26, 448, 24, bgc, 5);
        shell_platform_text(22, 44 + row * 26, line, tc);
        if (lib->items[i].favorite)
            shell_platform_text(440, 44 + row * 26, "*", app->theme.accent);
    }
}

static void draw_empty_library(ShellApp *app)
{
    if (app->tab == SHELL_TAB_GAMES) {
        shell_platform_text(20, 80, "Библиотека пуста", app->theme.text);
        shell_platform_text(20, 100, "Проверено:", app->theme.text);
        shell_platform_text(20, 118, "ms0:/ISO  ms0:/PSP/ISO", app->theme.accent);
        shell_platform_text(20, 136, "ef0:/ISO  ef0:/PSP/ISO", app->theme.accent);
        shell_platform_text(20, 160, "Положите ISO/CSO в одну из папок", app->theme.text);
    } else if (app->tab == SHELL_TAB_APPS) {
        shell_platform_text(20, 80, "Нет приложений", app->theme.text);
        shell_platform_text(20, 100, "Проверено: ms0:/PSP/GAME", app->theme.accent);
        shell_platform_text(20, 120, "Папка с EBOOT.PBP", app->theme.text);
    } else {
        shell_platform_text(20, 80, "Нет локальной музыки", app->theme.text);
        shell_platform_text(20, 100, "Проверено: ms0:/MUSIC  ms0:/PSP/MUSIC", app->theme.accent);
        shell_platform_text(20, 120, "Форматы: MP3, AA3, WAV", app->theme.text);
        shell_platform_text(20, 144, "Воспроизведение — отдельный этап", app->theme.text);
    }
}

static void draw_home_preview(ShellApp *app)
{
    ShellRgb dim = {20, 22, 26};
    ShellRgb card = {48, 52, 58};
    ShellRgb muted = {160, 164, 170};

    shell_platform_rect(10, 36, 460, 208, dim, 10);
    shell_platform_text(18, 44, "ПРЕДПРОСМОТР HOME — не внутриигровой модуль", app->theme.accent);

    shell_platform_rect(18, 64, 200, 100, card, 8);
    shell_platform_text(26, 72, "Музыка", app->theme.text);
    shell_platform_text(26, 90, "— нет трека —", muted);
    shell_platform_text(26, 108, "<<  |>  >>", app->theme.accent);
    shell_platform_rect(26, 130, 160, 6, muted, 2);
    shell_platform_rect(26, 130, 40, 6, app->theme.accent, 2);

    shell_platform_rect(230, 64, 100, 100, card, 8);
    shell_platform_text(238, 72, "Яркость", app->theme.text);
    shell_platform_rect(246, 100, 10, 50, muted, 2);
    shell_platform_rect(246, 120, 10, 30, app->theme.accent, 2);

    shell_platform_rect(340, 64, 100, 100, card, 8);
    shell_platform_text(348, 72, "Громкость", app->theme.text);
    shell_platform_rect(354, 100, 10, 50, muted, 2);
    shell_platform_rect(354, 110, 10, 40, app->theme.accent, 2);

    shell_platform_rect(18, 174, 442, 36, card, 6);
    shell_platform_text(26, 186, "Сохранить*  Загрузить*  Скрин  Выход", app->theme.text);
    shell_platform_text(26, 220, "* savestate: не реализовано (не подмена сохранений)", muted);
}

void shell_app_render(ShellApp *app)
{
    ShellLibrary *lib;
    ShellRgb card = {48, 52, 58};
    int i;
    if (!app)
        return;

    draw_chrome(app);

    if (app->mode == SHELL_MODE_COLOR_PICK) {
        int k;
        shell_platform_text(20, 40, "Круговая палитра", app->theme.text);
        /* Hue ring samples — stick picks continuously, ring is guide */
        for (k = 0; k < 12; ++k) {
            float ang = (float)k / 12.f;
            ShellRgb sw = shell_theme_from_hsv(ang, 0.9f, app->picker_brightness);
            int rx = 220 + (int)(48.f * cosf(ang * 6.2831853f));
            int ry = 120 + (int)(36.f * sinf(ang * 6.2831853f));
            shell_platform_rect(rx, ry, 14, 14, sw, 4);
        }
        shell_platform_rect(206, 106, 40, 40, app->theme.accent, 20);
        shell_platform_text(20, 180, "Стик — цвет, вверх/вниз — яркость", app->theme.text);
        shell_platform_text(20, 198, "X применить   O отмена", app->theme.text);
        draw_hints(app, app->status);
        shell_platform_swap();
        return;
    }

    if (app->mode == SHELL_MODE_CONFIRM) {
        shell_platform_rect(40, 80, 400, 100, card, 10);
        shell_platform_text(56, 100, app->status, app->theme.text);
        shell_platform_text(56, 140, "X — да    O — нет", app->theme.accent);
        draw_hints(app, "Подтверждение");
        shell_platform_swap();
        return;
    }

    if (app->mode == SHELL_MODE_CONTEXT) {
        shell_platform_rect(130, 48, 220, 28 + CTX_COUNT * 24, card, 10);
        shell_platform_text(146, 58, "Меню файла", app->theme.text);
        for (i = 0; i < CTX_COUNT; ++i) {
            ShellRgb c = (app->settings_focus == i) ? app->theme.accent : app->theme.text;
            if (app->settings_focus == i)
                shell_platform_rect(142, 80 + i * 24, 196, 20, app->theme.accent, 4);
            shell_platform_text(150, 84 + i * 24, CTX_RU[i],
                                app->settings_focus == i ? app->theme.focus_text : c);
        }
        draw_hints(app, "X выбрать  O назад");
        shell_platform_swap();
        return;
    }

    if (app->mode == SHELL_MODE_SEARCH) {
        shell_platform_text(20, 48, app->rename_pending ? "Переименование" : "Поиск", app->theme.text);
        shell_platform_text(20, 70, app->status, app->theme.accent);
        if (!app->rename_pending) {
            lib = &app->filtered;
            if (lib->count == 0)
                shell_platform_text(20, 110, "Нет совпадений", app->theme.text);
            else
                draw_list(app, lib);
        }
        draw_hints(app, app->rename_pending
                            ? "Влево/вправо  X символ  Square стереть  Start применить  O отмена"
                            : "Влево/вправо  X ввод  Square стереть  O готово");
        shell_platform_swap();
        return;
    }

    if (app->tab == SHELL_TAB_HOME_PREVIEW) {
        draw_home_preview(app);
        draw_hints(app, "O/Start — назад в настройки");
        shell_platform_swap();
        return;
    }

    if (app->tab == SHELL_TAB_SETTINGS) {
        const ShellThemePreset *p = shell_theme_presets();
        shell_platform_text(20, 40, "Настройки", app->theme.text);
        for (i = 0; i < SHELL_THEME_PRESET_COUNT; ++i) {
            int focused = (app->settings_focus == i);
            int x = 20 + i * 68;
            ShellRgb b = focused ? app->theme.text : card;
            shell_platform_rect(x - 2, 70, 56, 28, b, 6);
            shell_platform_rect(x, 72, 52, 24, p[i].accent, 5);
            shell_platform_text(x, 104, p[i].name_ru, app->theme.text);
        }
        {
            int focused = (app->settings_focus == SHELL_THEME_PRESET_COUNT);
            int x = 20 + SHELL_THEME_PRESET_COUNT * 68;
            ShellRgb b = focused ? app->theme.accent : card;
            shell_platform_rect(x - 2, 70, 40, 28, b, 6);
            shell_platform_text(x + 12, 78, "+", app->theme.text);
            shell_platform_text(x, 104, "палитра", app->theme.text);
        }
        {
            int focused = (app->settings_focus == SHELL_THEME_PRESET_COUNT + 1);
            char vline[64];
            ShellRgb c = focused ? app->theme.accent : app->theme.text;
            snprintf(vline, sizeof(vline), "Вид библиотеки: %s", VIEW_RU[app->theme.library_view]);
            shell_platform_text(20, 130, vline, c);
        }
        {
            int focused = (app->settings_focus == SHELL_THEME_PRESET_COUNT + 2);
            ShellRgb c = focused ? app->theme.accent : app->theme.text;
            shell_platform_text(20, 152, "Предпросмотр HOME-панели", c);
        }
        shell_platform_text(20, 180, "Акцент только для фокуса и ползунков", app->theme.text);
        draw_hints(app, app->status[0] ? app->status : "O назад  X выбрать");
        shell_platform_swap();
        return;
    }

    if (app->tab == SHELL_TAB_FILES) {
        ShellFileBrowser *fb = active_files(app);
        size_t start, idx;
        int row = 0;
        char path_show[96], sz[24], line[300];
        char fullname[280];

        shell_platform_text(20, 38, app->files_pane ? "Локация B" : "Локация A", app->theme.accent);
        ellipsize(path_show, sizeof(path_show), fb->cwd, 360);
        shell_platform_text(20, 54, path_show, app->theme.text);

        start = fb->focus > 4 ? fb->focus - 4 : 0;
        for (idx = start; idx < fb->count && row < 6; ++idx, ++row) {
            int focused = (idx == fb->focus);
            ShellRgb bgc = focused ? app->theme.accent : card;
            ShellRgb tc = focused ? app->theme.focus_text : app->theme.text;
            if (fb->items[idx].is_dir)
                snprintf(line, sizeof(line), "/%s", fb->items[idx].name);
            else {
                shell_files_format_size(fb->items[idx].size, sz, sizeof(sz));
                snprintf(line, sizeof(line), " %s  %s", fb->items[idx].name, sz);
            }
            ellipsize(line, sizeof(line), line, 420);
            shell_platform_rect(16, 72 + row * 24, 448, 22, bgc, 4);
            shell_platform_text(22, 76 + row * 24, line, tc);
        }

        if (fb->count && fb->focus < fb->count) {
            ellipsize(fullname, sizeof(fullname), fb->items[fb->focus].name, 400);
            shell_platform_text(20, 222, fullname, app->theme.text);
        } else if (fb->count == 0) {
            shell_platform_text(20, 120, "Папка пуста или недоступна", app->theme.text);
        }
        draw_hints(app, "X открыть  O вверх  Triangle меню  L/R локация");
        shell_platform_swap();
        return;
    }

    lib = active_lib(app);
    if (app->show_favorites_only == 1)
        shell_platform_text(300, 38, "Избранное", app->theme.accent);
    else if (app->show_favorites_only == 2)
        shell_platform_text(300, 38, "Недавние", app->theme.accent);

    if (lib->count == 0) {
        draw_empty_library(app);
    } else if (app->tab == SHELL_TAB_MUSIC || app->theme.library_view == 2) {
        /* Music is always a readable track list */
        draw_list(app, lib);
    } else if (app->theme.library_view == 1) {
        draw_grid(app, lib);
    } else {
        draw_cover_row(app, lib);
    }

    {
        const char *hint = app->status;
        if (!hint[0]) {
            if (app->tab == SHELL_TAB_MUSIC)
                hint = "L/R вкладки  крест фокус  X выбрать (плеер — след. этап)";
            else if (app->tab == SHELL_TAB_APPS)
                hint = "L/R вкладки  крест фокус  X запуск  Square избранное";
            else
                hint = "L/R вкладки  крест фокус  X запуск  Select фильтр  Start настройки";
        }
        draw_hints(app, hint);
    }
    shell_platform_swap();
}
