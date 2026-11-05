/*
 * Software framebuffer platform — reliable full UI on PPSSPP/PSP.
 * Solid AA roundrects (SDF, no hollow bugs). FreeType text via alpha blend.
 */
#include "shell/ui.h"
#include "shell/font.h"
#include "shell/platform_gu.h"

#include <pspdisplay.h>
#include <pspge.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

static uint32_t *fb0;
static uint32_t *fb1;
static uint32_t *draw_fb;
static int draw_idx;

static uint32_t *vram_u32(unsigned offset_bytes)
{
    return (uint32_t *)((uintptr_t)sceGeEdramGetAddr() + offset_bytes);
}

static uint32_t pack_col(ShellRgb c, uint8_t a)
{
    return ((uint32_t)a << 24) | ((uint32_t)c.b << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.r;
}

int shell_platform_gu_init(void)
{
    fb0 = vram_u32(0);
    fb1 = vram_u32(BUF_WIDTH * SCR_HEIGHT * 4);
    draw_idx = 0;
    draw_fb = fb0;
    sceDisplaySetMode(0, SCR_WIDTH, SCR_HEIGHT);
    sceDisplaySetFrameBuf(fb1, BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
    return 0;
}

void shell_platform_gu_shutdown(void)
{
    draw_fb = NULL;
}

void shell_platform_clear(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t col = pack_col((ShellRgb){r, g, b}, 0xFF);
    int y, x;
    for (y = 0; y < SCR_HEIGHT; ++y) {
        uint32_t *row = draw_fb + y * BUF_WIDTH;
        for (x = 0; x < SCR_WIDTH; ++x)
            row[x] = col;
    }
}

static void blend_px(int px, int py, ShellRgb c, uint8_t a)
{
    uint32_t *dst, d;
    int dr, dg, db;
    if (a == 0 || !draw_fb || px < 0 || py < 0 || px >= SCR_WIDTH || py >= SCR_HEIGHT)
        return;
    dst = draw_fb + py * BUF_WIDTH + px;
    if (a >= 250) {
        *dst = pack_col(c, 0xFF);
        return;
    }
    d = *dst;
    dr = (int)(d & 0xFF);
    dg = (int)((d >> 8) & 0xFF);
    db = (int)((d >> 16) & 0xFF);
    dr = (dr * (255 - a) + c.r * a) / 255;
    dg = (dg * (255 - a) + c.g * a) / 255;
    db = (db * (255 - a) + c.b * a) / 255;
    *dst = pack_col((ShellRgb){(uint8_t)dr, (uint8_t)dg, (uint8_t)db}, 0xFF);
}

void shell_platform_px(int x, int y, ShellRgb c, uint8_t alpha)
{
    blend_px(x, y, c, alpha);
}

static float rr_sdf(float px, float py, float hw, float hh, float r)
{
    float dx = fabsf(px) - hw + r;
    float dy = fabsf(py) - hh + r;
    float ax = dx > 0.f ? dx : 0.f;
    float ay = dy > 0.f ? dy : 0.f;
    float inside = fmaxf(dx, dy);
    float corner = sqrtf(ax * ax + ay * ay);
    return (inside > 0.f ? corner : inside) - r;
}

void shell_platform_rect(int x, int y, int w, int h, ShellRgb c, int radius)
{
    int r = radius;
    int yy, xx;
    float cx, cy, hw, hh;
    if (w <= 0 || h <= 0 || !draw_fb)
        return;
    if (r < 0)
        r = 0;
    if (r * 2 > w)
        r = w / 2;
    if (r * 2 > h)
        r = h / 2;

    if (r <= 0) {
        for (yy = y; yy < y + h; ++yy) {
            if (yy < 0 || yy >= SCR_HEIGHT)
                continue;
            for (xx = x; xx < x + w; ++xx) {
                if (xx < 0 || xx >= SCR_WIDTH)
                    continue;
                draw_fb[yy * BUF_WIDTH + xx] = pack_col(c, 0xFF);
            }
        }
        return;
    }

    cx = (float)x + (float)w * 0.5f;
    cy = (float)y + (float)h * 0.5f;
    hw = (float)w * 0.5f;
    hh = (float)h * 0.5f;

    /* Full SDF fill — every pixel, solid interior (no hollow pill bug). */
    for (yy = y; yy < y + h; ++yy) {
        if (yy < 0 || yy >= SCR_HEIGHT)
            continue;
        for (xx = x; xx < x + w; ++xx) {
            float d, a;
            if (xx < 0 || xx >= SCR_WIDTH)
                continue;
            d = rr_sdf((float)xx + 0.5f - cx, (float)yy + 0.5f - cy, hw, hh, (float)r);
            if (d <= -0.75f) {
                draw_fb[yy * BUF_WIDTH + xx] = pack_col(c, 0xFF);
                continue;
            }
            if (d >= 0.75f)
                continue;
            a = 0.75f - d;
            blend_px(xx, yy, c, (uint8_t)(a * 255.f));
        }
    }
}

static void plot_px(int px, int py, ShellRgb col, uint8_t alpha, void *ud)
{
    (void)ud;
    blend_px(px, py, col, alpha);
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

void shell_platform_clock(char *buf, size_t n)
{
    ScePspDateTime tm;
    if (!buf || n == 0)
        return;
    buf[0] = '\0';
    memset(&tm, 0, sizeof(tm));
    if (sceRtcGetCurrentClockLocalTime(&tm) < 0)
        return;
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
    if (scePowerIsBatteryExist() <= 0)
        return;
    pct = scePowerGetBatteryLifePercent();
    if (pct < 0 || pct > 100)
        return;
    snprintf(buf, n, "%d%%", pct);
}
