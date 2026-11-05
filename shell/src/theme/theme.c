#include "shell/theme.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const ShellThemePreset PRESETS[SHELL_THEME_PRESET_COUNT] = {
    {"Циан", {0, 200, 220}},
    {"Янтарь", {230, 160, 40}},
    {"Мята", {80, 200, 140}},
    {"Роза", {220, 100, 140}},
    {"Белый", {220, 220, 230}},
};

const ShellThemePreset *shell_theme_presets(void)
{
    return PRESETS;
}

void shell_theme_default(ShellTheme *t)
{
    t->accent = PRESETS[0].accent;
    t->bg = (ShellRgb){28, 30, 34};
    t->text = (ShellRgb){230, 232, 236};
    t->focus_text = (ShellRgb){16, 18, 22};
    t->library_view = 0;
    t->wallpaper_id = 0;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

ShellRgb shell_theme_from_hsv(float hue_turns, float sat, float value)
{
    float h, s, v, c, x, m, r = 0, g = 0, b = 0;
    int hi;
    ShellRgb out;

    h = hue_turns - (float)floor(hue_turns);
    if (h < 0.f)
        h += 1.f;
    s = clampf(sat, 0.f, 1.f);
    v = clampf(value, 0.f, 1.f);
    c = v * s;
    h *= 6.f;
    x = c * (1.f - fabsf(fmodf(h, 2.f) - 1.f));
    m = v - c;
    hi = (int)h;
    switch (hi) {
    case 0:
        r = c;
        g = x;
        break;
    case 1:
        r = x;
        g = c;
        break;
    case 2:
        g = c;
        b = x;
        break;
    case 3:
        g = x;
        b = c;
        break;
    case 4:
        r = x;
        b = c;
        break;
    default:
        r = c;
        b = x;
        break;
    }
    out.r = (uint8_t)clampf((r + m) * 255.f + 0.5f, 0.f, 255.f);
    out.g = (uint8_t)clampf((g + m) * 255.f + 0.5f, 0.f, 255.f);
    out.b = (uint8_t)clampf((b + m) * 255.f + 0.5f, 0.f, 255.f);
    return out;
}

static float linearize(uint8_t c)
{
    float s = c / 255.f;
    return (s <= 0.04045f) ? (s / 12.92f) : powf((s + 0.055f) / 1.055f, 2.4f);
}

static float luminance(ShellRgb c)
{
    return 0.2126f * linearize(c.r) + 0.7152f * linearize(c.g) + 0.0722f * linearize(c.b);
}

static float contrast_ratio(ShellRgb a, ShellRgb b)
{
    float l1 = luminance(a);
    float l2 = luminance(b);
    float hi = (l1 > l2) ? l1 : l2;
    float lo = (l1 > l2) ? l2 : l1;
    return (hi + 0.05f) / (lo + 0.05f);
}

int shell_theme_accent_readable(ShellRgb accent, ShellRgb bg, ShellRgb text)
{
    (void)text;
    /* Accent must be distinguishable on graphite and usable as focus fill. */
    if (contrast_ratio(accent, bg) < 2.2f)
        return 0;
    if (contrast_ratio(accent, (ShellRgb){16, 18, 22}) < 2.0f)
        return 0;
    return 1;
}

int shell_theme_stick_pick(float stick_x, float stick_y, float deadzone, float brightness,
                           ShellRgb *inout_accent)
{
    float mag, angle, sat;
    ShellRgb candidate;

    if (!inout_accent)
        return 0;
    mag = sqrtf(stick_x * stick_x + stick_y * stick_y);
    if (mag < deadzone)
        return 0;
    sat = clampf((mag - deadzone) / (1.f - deadzone), 0.15f, 1.f);
    angle = atan2f(stick_y, stick_x); /* -pi..pi */
    if (angle < 0.f)
        angle += (float)(2.0 * M_PI);
    candidate = shell_theme_from_hsv(angle / (float)(2.0 * M_PI), sat, clampf(brightness, 0.35f, 1.f));
    if (!shell_theme_accent_readable(candidate, (ShellRgb){28, 30, 34}, (ShellRgb){230, 232, 236})) {
        /* bump value to keep readable */
        candidate = shell_theme_from_hsv(angle / (float)(2.0 * M_PI), sat, 0.85f);
    }
    *inout_accent = candidate;
    return 1;
}
