/*
 * OwnShell M1 — bootable userland EBOOT for PSP (ARK target).
 * Does not write flash or install plugins.
 */
#include "shell/ui.h"
#include "shell/library.h"
#include "shell/path.h"
#include "shell/font.h"

#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspkernel.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

PSP_MODULE_INFO("OwnShell", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(8 * 1024);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

static unsigned int __attribute__((aligned(16))) gulist[262144];
static void *fbp0;
static void *fbp1;
static void *zbp;
static int running = 1;
static int gu_started;

static void *get_vram(unsigned int size)
{
    static unsigned int staticOffset = 0;
    unsigned int result = staticOffset;
    staticOffset += (size + 63) & ~63u;
    return (void *)(((unsigned char *)sceGeEdramGetAddr()) + result);
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

static void gu_init(void)
{
    fbp0 = get_vram(BUF_WIDTH * SCR_HEIGHT * 4);
    fbp1 = get_vram(BUF_WIDTH * SCR_HEIGHT * 4);
    zbp = get_vram(BUF_WIDTH * SCR_HEIGHT * 2);

    sceGuInit();
    sceGuStart(GU_DIRECT, gulist);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fbp1, BUF_WIDTH);
    sceGuDepthBuffer(zbp, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void shell_platform_clear(uint8_t r, uint8_t g, uint8_t b)
{
    sceGuStart(GU_DIRECT, gulist);
    gu_started = 1;
    sceGuClearColor((0xFFu << 24) | ((unsigned)b << 16) | ((unsigned)g << 8) | r);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void shell_platform_rect(int x, int y, int w, int h, ShellRgb c, int radius)
{
    typedef struct {
        unsigned int color;
        short x, y, z;
    } Vert;
    Vert *v;
    (void)radius;
    if (!gu_started) {
        sceGuStart(GU_DIRECT, gulist);
        gu_started = 1;
    }
    v = (Vert *)sceGuGetMemory(2 * sizeof(Vert));
    v[0].color = (0xF0u << 24) | ((unsigned)c.b << 16) | ((unsigned)c.g << 8) | c.r;
    v[0].x = (short)x;
    v[0].y = (short)y;
    v[0].z = 0;
    v[1].color = v[0].color;
    v[1].x = (short)(x + w);
    v[1].y = (short)(y + h);
    v[1].z = 0;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}

static void plot_px(int px, int py, ShellRgb col, void *ud)
{
    (void)ud;
    shell_platform_rect(px, py, 1, 1, col, 0);
}

void shell_platform_text(int x, int y, const char *utf8, ShellRgb c)
{
    shell_font_utf8_draw(utf8, x, y, c, plot_px, NULL);
}

void shell_platform_swap(void)
{
    if (gu_started) {
        sceGuFinish();
        sceGuSync(0, 0);
        gu_started = 0;
    }
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
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

static void scan_game_folder(ShellLibrary *lib, const char *dir)
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
        snprintf(eboot, sizeof(eboot), "%s/EBOOT.PBP", full);
        if (stat(eboot, &st) == 0)
            shell_library_add(lib, e->d_name, eboot, SHELL_ENTRY_HOMEBREW);
    }
    closedir(d);
}

static void seed_library(ShellApp *app)
{
    scan_dir_isos(&app->games, "ms0:/ISO");
    scan_game_folder(&app->games, "ms0:/PSP/GAME");
    scan_dir_isos(&app->games, "ef0:/ISO");
    scan_game_folder(&app->games, "ef0:/PSP/GAME");
    snprintf(app->status, sizeof(app->status), "Игр: %u", (unsigned)app->games.count);
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
    gu_init();

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
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
