/*
 * OwnShell / Velora — bootable userland EBOOT.
 * Graphics: GU via ya2d (platform_gu.c). Logic stays here.
 */
#include "shell/ui.h"
#include "shell/library.h"
#include "shell/path.h"
#include "shell/font.h"
#include "shell/prefs.h"
#include "shell/platform_gu.h"

#include <pspctrl.h>
#include <pspkernel.h>
#include <psploadexec.h>

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

PSP_MODULE_INFO("OwnShell", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(8 * 1024);

static int running = 1;

int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;
    running = 0;
    return 0;
}

int callback_thread(SceSize args, void *argp)
{
    int cbid;
    (void)args;
    (void)argp;
    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, 0);
}

static void scan_dir_isos(ShellLibrary *lib, const char *dir)
{
    DIR *d;
    struct dirent *e;
    char full[SHELL_PATH_MAX];
    char clean[SHELL_PATH_MAX];
    if (shell_path_sanitize(dir, clean, sizeof(clean)) != SHELL_PATH_OK)
        return;
    d = opendir(clean);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        if (!shell_library_is_iso_name(e->d_name))
            continue;
        if (shell_path_join(clean, e->d_name, full, sizeof(full)) != SHELL_PATH_OK)
            continue;
        shell_library_add(lib, e->d_name, full, SHELL_ENTRY_GAME_ISO);
    }
    closedir(d);
}

static void scan_game_folder(ShellLibrary *lib, const char *dir, ShellEntryKind kind)
{
    DIR *d;
    struct dirent *e;
    char full[SHELL_PATH_MAX];
    char eboot[SHELL_PATH_MAX];
    char clean[SHELL_PATH_MAX];
    struct stat st;
    if (shell_path_sanitize(dir, clean, sizeof(clean)) != SHELL_PATH_OK)
        return;
    d = opendir(clean);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        if (strcmp(e->d_name, "OWNSHELL") == 0 || strcmp(e->d_name, "OwnShell") == 0)
            continue;
        if (shell_path_join(clean, e->d_name, full, sizeof(full)) != SHELL_PATH_OK)
            continue;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;
        if (snprintf(eboot, sizeof(eboot), "%s/EBOOT.PBP", full) >= (int)sizeof(eboot))
            continue;
        if (stat(eboot, &st) == 0)
            shell_library_add(lib, e->d_name, eboot, kind);
    }
    closedir(d);
}

static void scan_dir_music(ShellLibrary *lib, const char *dir)
{
    DIR *d;
    struct dirent *e;
    char full[SHELL_PATH_MAX];
    char clean[SHELL_PATH_MAX];
    if (shell_path_sanitize(dir, clean, sizeof(clean)) != SHELL_PATH_OK)
        return;
    d = opendir(clean);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        if (!shell_library_is_music_name(e->d_name))
            continue;
        if (shell_path_join(clean, e->d_name, full, sizeof(full)) != SHELL_PATH_OK)
            continue;
        shell_library_add(lib, e->d_name, full, SHELL_ENTRY_MUSIC);
    }
    closedir(d);
}

static void seed_library(ShellApp *app)
{
    scan_dir_isos(&app->games, "ms0:/ISO");
    scan_dir_isos(&app->games, "ms0:/PSP/ISO");
    scan_dir_isos(&app->games, "ef0:/ISO");
    scan_dir_isos(&app->games, "ef0:/PSP/ISO");
    scan_game_folder(&app->apps, "ms0:/PSP/GAME", SHELL_ENTRY_HOMEBREW);
    scan_game_folder(&app->apps, "ms0:/GAME", SHELL_ENTRY_HOMEBREW);
    scan_game_folder(&app->apps, "ef0:/PSP/GAME", SHELL_ENTRY_HOMEBREW);
    scan_game_folder(&app->apps, "ef0:/GAME", SHELL_ENTRY_HOMEBREW);
    scan_dir_music(&app->music, "ms0:/MUSIC");
    scan_dir_music(&app->music, "ms0:/PSP/MUSIC");
    scan_dir_music(&app->music, "ms0:/PSP/MUSIC/MP3");
    scan_dir_music(&app->music, "ef0:/MUSIC");
    (void)shell_prefs_load_library(&app->games, &app->apps, app->recent_paths, &app->recent_count, 8);
    app->show_favorites_only = 0;
    app->status[0] = '\0';
}

int shell_platform_launch(const char *path, ShellEntryKind kind)
{
    SceKernelLoadExecParam param;
    if (!path || !path[0])
        return -1;
    if (kind == SHELL_ENTRY_GAME_ISO)
        return 1;
    if (kind == SHELL_ENTRY_HOMEBREW || kind == SHELL_ENTRY_APP) {
        memset(&param, 0, sizeof(param));
        param.size = sizeof(param);
        if (sceKernelLoadExec(path, &param) < 0)
            return -1;
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    ShellApp app;
    ShellInput input;
    SceCtrlData pad;
    (void)argc;
    (void)argv;

    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    if (shell_platform_gu_init() != 0) {
        sceKernelExitGame();
        return 1;
    }
    if (shell_font_init() != 0) {
        shell_platform_gu_shutdown();
        sceKernelExitGame();
        return 1;
    }

    memset(&input, 0, sizeof(input));
    shell_app_init(&app);
    seed_library(&app);

    while (running && app.running) {
        float ax, ay;
        sceCtrlReadBufferPositive(&pad, 1);
        ax = (pad.Lx - 128) / 128.f;
        ay = (pad.Ly - 128) / 128.f;
        shell_input_begin_frame(&input, pad.Buttons, ax, ay);
        shell_app_update(&app, &input);
        shell_app_render(&app);
    }

    shell_app_shutdown(&app);
    shell_font_shutdown();
    shell_platform_gu_shutdown();
    sceKernelExitGame();
    return 0;
}
