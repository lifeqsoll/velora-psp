/*
 * FreeType UI text with glyph alpha cache.
 */
#include "shell/font.h"
#include "shell/font_ttf_data.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string.h>

#define FONT_PX 14
#define CACHE_N 192
#define GLYPH_MAX 48

typedef struct {
    unsigned cp;
    int valid;
    int stamp;
    int advance;
    int bearing_x;
    int bearing_y;
    int w;
    int h;
    unsigned char bits[GLYPH_MAX * GLYPH_MAX];
} GlyphSlot;

static FT_Library g_lib;
static FT_Face g_face;
static int g_ready;
static int g_ascent;
static int g_line_h;
static int g_advance;
static int g_stamp;
static GlyphSlot g_cache[CACHE_N];

static int utf8_next(const unsigned char **pp, unsigned *out)
{
    const unsigned char *p = *pp;
    unsigned cp;
    if (!p || !*p)
        return 0;
    if (p[0] < 0x80) {
        cp = p[0];
        *pp = p + 1;
    } else if ((p[0] & 0xE0) == 0xC0 && p[1]) {
        cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        *pp = p + 2;
    } else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        *pp = p + 3;
    } else if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
        cp = '?';
        *pp = p + 4;
    } else {
        cp = '?';
        *pp = p + 1;
    }
    *out = cp;
    return 1;
}

static GlyphSlot *cache_get(unsigned cp)
{
    int i, oldest = 0, oldest_stamp = g_stamp + 1;
    GlyphSlot *slot;
    FT_UInt gi;
    FT_GlyphSlot g;
    int w, h, row, col;

    for (i = 0; i < CACHE_N; ++i) {
        if (g_cache[i].valid && g_cache[i].cp == cp) {
            g_cache[i].stamp = ++g_stamp;
            return &g_cache[i];
        }
    }
    for (i = 0; i < CACHE_N; ++i) {
        if (!g_cache[i].valid) {
            oldest = i;
            break;
        }
        if (g_cache[i].stamp < oldest_stamp) {
            oldest_stamp = g_cache[i].stamp;
            oldest = i;
        }
    }
    slot = &g_cache[oldest];
    memset(slot, 0, sizeof(*slot));
    slot->cp = cp;
    slot->stamp = ++g_stamp;

    gi = FT_Get_Char_Index(g_face, cp);
    if (!gi)
        gi = FT_Get_Char_Index(g_face, '?');
    if (FT_Load_Glyph(g_face, gi, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0) {
        slot->advance = g_advance;
        slot->valid = 1;
        return slot;
    }
    g = g_face->glyph;
    slot->advance = (g->advance.x + 32) >> 6;
    if (slot->advance <= 0)
        slot->advance = g_advance;
    slot->bearing_x = g->bitmap_left;
    slot->bearing_y = g->bitmap_top;
    w = (int)g->bitmap.width;
    h = (int)g->bitmap.rows;
    if (w > GLYPH_MAX)
        w = GLYPH_MAX;
    if (h > GLYPH_MAX)
        h = GLYPH_MAX;
    slot->w = w;
    slot->h = h;
    if (g->bitmap.buffer && w > 0 && h > 0) {
        for (row = 0; row < h; ++row) {
            const unsigned char *src = g->bitmap.buffer + row * g->bitmap.pitch;
            unsigned char *dst = slot->bits + row * GLYPH_MAX;
            for (col = 0; col < w; ++col)
                dst[col] = src[col];
        }
    }
    slot->valid = 1;
    return slot;
}

int shell_font_init(void)
{
    FT_Error err;
    if (g_ready)
        return 0;
    memset(g_cache, 0, sizeof(g_cache));
    err = FT_Init_FreeType(&g_lib);
    if (err)
        return -1;
    err = FT_New_Memory_Face(g_lib, shell_font_ttf, (FT_Long)shell_font_ttf_len, 0, &g_face);
    if (err) {
        FT_Done_FreeType(g_lib);
        return -2;
    }
    err = FT_Set_Pixel_Sizes(g_face, 0, FONT_PX);
    if (err) {
        FT_Done_Face(g_face);
        FT_Done_FreeType(g_lib);
        return -3;
    }
    g_ascent = (int)((g_face->size->metrics.ascender + 32) >> 6);
    if (g_ascent < FONT_PX - 2)
        g_ascent = FONT_PX - 2;
    g_line_h = (int)((g_face->size->metrics.height + 32) >> 6);
    if (g_line_h < FONT_PX + 2)
        g_line_h = FONT_PX + 3;
    {
        GlyphSlot *sp = cache_get((unsigned)' ');
        g_advance = sp->advance > 0 ? sp->advance : (FONT_PX * 6) / 10;
    }
    g_ready = 1;
    return 0;
}

void shell_font_shutdown(void)
{
    if (!g_ready)
        return;
    FT_Done_Face(g_face);
    FT_Done_FreeType(g_lib);
    g_ready = 0;
    memset(g_cache, 0, sizeof(g_cache));
}

int shell_font_advance(void)
{
    return g_ready ? g_advance : 7;
}

int shell_font_line_height(void)
{
    return g_ready ? g_line_h : 16;
}

int shell_font_measure(const char *utf8)
{
    const unsigned char *p = (const unsigned char *)utf8;
    unsigned cp;
    int w = 0;
    if (!utf8 || !g_ready)
        return 0;
    while (utf8_next(&p, &cp))
        w += cache_get(cp)->advance;
    return w;
}

int shell_font_utf8_draw(const char *utf8, int x, int y, ShellRgb c, ShellFontPlot plot, void *ud)
{
    const unsigned char *p = (const unsigned char *)utf8;
    unsigned cp;
    int cx = x;
    int baseline;
    if (!utf8 || !plot || !g_ready)
        return 0;
    baseline = y + g_ascent;
    while (utf8_next(&p, &cp)) {
        GlyphSlot *slot = cache_get(cp);
        int gy, gx;
        int ox = cx + slot->bearing_x;
        int oy = baseline - slot->bearing_y;
        for (gy = 0; gy < slot->h; ++gy) {
            for (gx = 0; gx < slot->w; ++gx) {
                unsigned char a = slot->bits[gy * GLYPH_MAX + gx];
                if (a > 8)
                    plot(ox + gx, oy + gy, c, a, ud);
            }
        }
        cx += slot->advance;
    }
    return cx - x;
}
