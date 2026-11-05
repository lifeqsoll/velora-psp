/*
 * GU platform: ya2d + AA roundrect textures (bilinear) + FreeType into FB.
 * Matches how smooth PSP launchers look — textured chrome, not soft 1×1 jaggies.
 */
#include "shell/ui.h"
#include "shell/font.h"

#include <ya2d.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <psppower.h>
#include <psprtc.h>
#include <vram.h>

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ya2d pulls these from oslib; provide libm stubs so we don't link all of OSL. */
float vfpu_cosf(float x) { return cosf(x); }
float vfpu_sinf(float x) { return sinf(x); }

#define SCR_W 480
#define SCR_H 272
#define BUF_W 512

#define RR_CACHE 32
#define RR_MAX 480

typedef struct {
    int w, h, r;
    int stamp;
    struct ya2d_texture *tex;
} RrSlot;

static int g_drawing;
static int g_stamp;
static RrSlot g_rr[RR_CACHE];
static uint32_t *g_fb;

static uint32_t rgba_u32(ShellRgb c, uint8_t a)
{
    /* PSP 8888 framebuffer / ya2d: A B G R */
    return ((uint32_t)a << 24) | ((uint32_t)c.b << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.r;
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

static void bake_roundrect(struct ya2d_texture *tex, int w, int h, int r)
{
    int x, y;
    int stride_px;
    uint32_t *pix;
    float cx, cy, hw, hh;
    if (!tex || !tex->data || w <= 0 || h <= 0)
        return;
    /* ya2d: stride is bytes */
    stride_px = tex->stride / 4;
    if (stride_px < w)
        stride_px = tex->pow2_w;
    pix = (uint32_t *)tex->data;
    memset(pix, 0, (size_t)tex->data_size);
    cx = w * 0.5f;
    cy = h * 0.5f;
    hw = w * 0.5f;
    hh = h * 0.5f;
    if (r < 0)
        r = 0;
    if (r * 2 > w)
        r = w / 2;
    if (r * 2 > h)
        r = h / 2;

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            float d, a;
            uint8_t alpha;
            if (r <= 0) {
                pix[y * stride_px + x] = 0xFFFFFFFFu;
                continue;
            }
            d = rr_sdf(x + 0.5f - cx, y + 0.5f - cy, hw, hh, (float)r);
            if (d <= -1.0f)
                a = 1.f;
            else if (d >= 1.0f)
                a = 0.f;
            else
                a = 0.5f * (1.0f - d); /* wider AA fringe */
            if (a < 0.02f)
                continue;
            if (a > 1.f)
                a = 1.f;
            alpha = (uint8_t)(a * 255.f);
            /* white RGB, coverage in A — tinted via MODULATE */
            pix[y * stride_px + x] = ((uint32_t)alpha << 24) | 0x00FFFFFFu;
        }
    }
    ya2d_swizzle_texture(tex);
    ya2d_flush_texture(tex);
}

/* Tint white AA texture; force LINEAR + MODULATE (ya2d defaults NEAREST/REPLACE). */
static void draw_tint_tex(struct ya2d_texture *tex, int x, int y, uint32_t color)
{
    struct {
        unsigned int color;
        short u, v;
        short x, y, z;
    } *vertices;

    if (!tex)
        return;
    ya2d_set_texture(tex);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    vertices = sceGuGetMemory(2 * sizeof(*vertices));
    vertices[0].color = color;
    vertices[0].u = 0;
    vertices[0].v = 0;
    vertices[0].x = (short)x;
    vertices[0].y = (short)y;
    vertices[0].z = 0;
    vertices[1].color = color;
    vertices[1].u = (short)tex->width;
    vertices[1].v = (short)tex->height;
    vertices[1].x = (short)(x + tex->width);
    vertices[1].y = (short)(y + tex->height);
    vertices[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2,
                   0, vertices);
}

static RrSlot *rr_get(int w, int h, int r)
{
    int i, oldest = 0, oldest_stamp = g_stamp + 1;
    RrSlot *s;
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    if (w > RR_MAX)
        w = RR_MAX;
    if (h > RR_MAX)
        h = RR_MAX;

    for (i = 0; i < RR_CACHE; ++i) {
        if (g_rr[i].tex && g_rr[i].w == w && g_rr[i].h == h && g_rr[i].r == r) {
            g_rr[i].stamp = ++g_stamp;
            return &g_rr[i];
        }
    }
    for (i = 0; i < RR_CACHE; ++i) {
        if (!g_rr[i].tex) {
            oldest = i;
            break;
        }
        if (g_rr[i].stamp < oldest_stamp) {
            oldest_stamp = g_rr[i].stamp;
            oldest = i;
        }
    }
    s = &g_rr[oldest];
    if (s->tex) {
        ya2d_free_texture(s->tex);
        s->tex = NULL;
    }
    s->tex = ya2d_create_texture(w, h, GU_PSM_8888, YA2D_PLACE_RAM);
    if (!s->tex)
        return NULL;
    s->w = w;
    s->h = h;
    s->r = r;
    s->stamp = ++g_stamp;
    bake_roundrect(s->tex, w, h, r);
    return s;
}

static uint32_t *fb_abs(void)
{
    void *p = ya2d_get_drawbuffer();
    if (!p)
        return NULL;
    /* After swap ya2d may hand a GE-relative pointer — CPU needs absolute. */
    return (uint32_t *)vabsptr(p);
}

static void ui_gu_state(void)
{
    /* ya2d enables CULL+DEPTH; 2D UI tris get culled → black screen. */
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDisable(GU_ALPHA_TEST);
}

static void ensure_frame(void)
{
    if (!g_drawing) {
        ya2d_start_drawing();
        ui_gu_state();
        g_drawing = 1;
        g_fb = fb_abs();
    }
}

int shell_platform_gu_init(void)
{
    int rc;
    memset(g_rr, 0, sizeof(g_rr));
    g_drawing = 0;
    g_fb = NULL;
    /* ya2d_init returns 1 on success, 2 if already inited — never 0 on OK */
    rc = ya2d_init();
    if (rc != 1 && rc != 2)
        return -1;
    ya2d_set_vsync(1);
    return 0;
}

void shell_platform_gu_shutdown(void)
{
    int i;
    if (g_drawing) {
        ya2d_finish_drawing();
        g_drawing = 0;
    }
    for (i = 0; i < RR_CACHE; ++i) {
        if (g_rr[i].tex) {
            ya2d_free_texture(g_rr[i].tex);
            g_rr[i].tex = NULL;
        }
    }
    ya2d_shutdown();
}

void shell_platform_clear(uint8_t r, uint8_t g, uint8_t b)
{
    ensure_frame();
    ui_gu_state();
    ya2d_clear_screen(rgba_u32((ShellRgb){r, g, b}, 0xFF));
    g_fb = fb_abs();
}

void shell_platform_rect(int x, int y, int w, int h, ShellRgb c, int radius)
{
    RrSlot *s;
    ensure_frame();
    if (w <= 0 || h <= 0)
        return;
    if (radius <= 0) {
        ya2d_draw_rect(x, y, w, h, rgba_u32(c, 0xFF), 1);
        return;
    }
    s = rr_get(w, h, radius);
    if (!s || !s->tex) {
        ya2d_draw_rect(x, y, w, h, rgba_u32(c, 0xFF), 1);
        return;
    }
    draw_tint_tex(s->tex, x, y, rgba_u32(c, 0xFF));
}

void shell_platform_px(int x, int y, ShellRgb c, uint8_t alpha)
{
    uint32_t *dst, d;
    int dr, dg, db;
    ensure_frame();
    if (!g_fb || alpha == 0 || x < 0 || y < 0 || x >= SCR_W || y >= SCR_H)
        return;
    dst = g_fb + y * BUF_W + x;
    if (alpha >= 250) {
        *dst = rgba_u32(c, 0xFF);
        return;
    }
    d = *dst;
    dr = (int)(d & 0xFF);
    dg = (int)((d >> 8) & 0xFF);
    db = (int)((d >> 16) & 0xFF);
    dr = (dr * (255 - alpha) + c.r * alpha) / 255;
    dg = (dg * (255 - alpha) + c.g * alpha) / 255;
    db = (db * (255 - alpha) + c.b * alpha) / 255;
    *dst = rgba_u32((ShellRgb){(uint8_t)dr, (uint8_t)dg, (uint8_t)db}, 0xFF);
}

static void plot_px(int px, int py, ShellRgb col, uint8_t alpha, void *ud)
{
    (void)ud;
    shell_platform_px(px, py, col, alpha);
}

void shell_platform_text(int x, int y, const char *utf8, ShellRgb c)
{
    ensure_frame();
    /* Finish GU batch before CPU blends into the same FB. */
    ya2d_finish_drawing();
    g_drawing = 0;
    g_fb = fb_abs();
    shell_font_utf8_draw(utf8, x, y, c, plot_px, g_fb);
    /* Resume GU for further rects this frame */
    ya2d_start_drawing();
    ui_gu_state();
    g_drawing = 1;
}

void shell_platform_swap(void)
{
    if (g_drawing) {
        ya2d_finish_drawing();
        g_drawing = 0;
    }
    ya2d_swapbuffers();
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
    if (pct < 0)
        return;
    snprintf(buf, n, "%d%%", pct);
}
