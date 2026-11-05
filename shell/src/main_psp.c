/*
 * OwnShell / Velora — bootable userland EBOOT.
 * Software framebuffer UI (480×272): reliable text, no GU 1×1 sprite overflow.
 */
#include "shell/ui.h"
#include "shell/library.h"
#include "shell/path.h"
#include "shell/font.h"

#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspkernel.h>
#include <psploadexec.h>
#include <psppower.h>
#include <psprtc.h>

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

PSP_MODULE_INFO("OwnShell", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(8 * 1024);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

static int running = 1;
static uint32_t *fb0;
static uint32_t *fb1;
static uint32_t *draw_fb;
static int draw_idx;

static uint32_t *vram_u32(unsigned offset_bytes)
{
    return (uint32_t *)((uintptr_t)sceGeEdramGetAddr() + offset_bytes);
}

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

static void fb_init(void)
{
    /* Two 8888 buffers at start of VRAM; CPU writes via EDRAM base. */
    fb0 = vram_u32(0);
    fb1 = vram_u32(BUF_WIDTH * SCR_HEIGHT * 4);
    draw_idx = 0;
    draw_fb = fb0;
    sceDisplaySetMode(0, SCR_WIDTH, SCR_HEIGHT);
    sceDisplaySetFrameBuf(fb1, BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
}

static uint32_t pack_col(ShellRgb c, uint8_t a)
{
    return ((uint32_t)a << 24) | ((uint32_t)c.b << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.r;
}

void shell_platform_clear(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t col = ((uint32_t)0xFF << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    int y;
    for (y = 0; y < SCR_HEIGHT; ++y) {
        uint32_t *row = draw_fb + y * BUF_WIDTH;
        int x;
        for (x = 0; x < SCR_WIDTH; ++x)
            row[x] = col;
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t col)
{
    int yy, xx;
    if (w <= 0 || h <= 0)
        return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SCR_WIDTH)
        w = SCR_WIDTH - x;
    if (y + h > SCR_HEIGHT)
        h = SCR_HEIGHT - y;
    if (w <= 0 || h <= 0)
        return;
    for (yy = y; yy < y + h; ++yy) {
        uint32_t *row = draw_fb + yy * BUF_WIDTH + x;
        for (xx = 0; xx < w; ++xx)
            row[xx] = col;
    }
}

/* Soft rounded rect: outer radius approximated with corner cut. */
void shell_platform_rect(int x, int y, int w, int h, ShellRgb c, int radius)
{
    uint32_t col = pack_col(c, 0xFF);
    int r = radius;
    if (r < 0)
        r = 0;
    if (r * 2 > w)
        r = w / 2;
    if (r * 2 > h)
        r = h / 2;
    if (r <= 0) {
        fill_rect(x, y, w, h, col);
        return;
    }
    fill_rect(x + r, y, w - 2 * r, h, col);
    fill_rect(x, y + r, r, h - 2 * r, col);
    fill_rect(x + w - r, y + r, r, h - 2 * r, col);
    /* cheap corner blocks */
    fill_rect(x + 1, y + 1, r - 1, r - 1, col);
    fill_rect(x + w - r, y + 1, r - 1, r - 1, col);
    fill_rect(x + 1, y + h - r, r - 1, r - 1, col);
    fill_rect(x + w - r, y + h - r, r - 1, r - 1, col);
}

static void plot_px(int px, int py, ShellRgb col, void *ud)
{
    uint32_t *fb = (uint32_t *)ud;
    if (!fb || px < 0 || py < 0 || px >= SCR_WIDTH || py >= SCR_HEIGHT)
        return;
    fb[py * BUF_WIDTH + px] = pack_col(col, 0xFF);
}

void shell_platform_text(int x, int y, const char *utf8, ShellRgb c)
{
    shell_font_utf8_draw(utf8, x, y, c, plot_px, draw_fb);
}

void shell_platform_swap(void)
{
    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf(draw_fb, BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    draw_idx ^= 1;
    draw_fb = draw_idx ? fb1 : fb0;
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
    /* Common layouts across CFW and PPSSPP memstick roots. */
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
    /* Keep first-frame status as control hints; counts are in library meta (n/m). */
    (void)app->games.count;
    (void)app->music.count;
    (void)app->apps.count;
    snprintf(app->status, sizeof(app->status),
             "L/R вкладки  крест фокус  X запуск  Select фильтр  Start настройки");
}

void shell_platform_clock(char *buf, size_t n)
{
    ScePspDateTime tm;
    if (!buf || n == 0)
        return;
    buf[0] = '\0';
    memset(&tm, 0, sizeof(tm));
    if (sceRtcGetCurrentClockLocalTime(&tm) < 0)
        return;
    /* Only show if values look sane — never invent. */
    if (tm.hour > 23 || tm.minute > 59)
        return;
    snprintf(buf, n, "%02d:%02d", (int)tm.hour, (int)tm.minute);
}

void shell_platform_battery(char *buf, size_t n)
{
    int pct;
    if (!buf || n == 0)
        return;
    buf[0] = '\0';
    pct = scePowerGetBatteryLifePercent();
    if (pct < 0 || pct > 100)
        return;
    snprintf(buf, n, "%d%%", pct);
}

int shell_platform_launch(const char *path, ShellEntryKind kind)
{
    SceKernelLoadExecParam param;
    if (!path || !path[0])
        return -1;
    if (kind == SHELL_ENTRY_GAME_ISO) {
        /* ISO boot needs CFW (PRO/ARK) — do not fake success. */
        return 1;
    }
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
    fb_init();

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
    sceKernelExitGame();
    return 0;
}
