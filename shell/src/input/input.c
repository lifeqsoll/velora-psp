#include "shell/input.h"

#include <math.h>

static int g_delay = 18; /* ~300ms */
static int g_rate = 5;   /* ~12Hz repeat */

void shell_input_set_repeat(int delay_frames, int rate_frames)
{
    if (delay_frames > 0)
        g_delay = delay_frames;
    if (rate_frames > 0)
        g_rate = rate_frames;
}

void shell_input_begin_frame(ShellInput *in, uint32_t buttons_now, float ax, float ay)
{
    uint32_t prev, nav_now, nav_pressed, fire;
    if (!in)
        return;
    prev = in->held;
    in->pressed = buttons_now & ~prev;
    in->released = prev & ~buttons_now;
    in->held = buttons_now;
    in->ax = ax;
    in->ay = ay;

    nav_now = buttons_now & SHELL_BTN_NAV;
    nav_pressed = in->pressed & SHELL_BTN_NAV;
    fire = nav_pressed;

    if (nav_now == 0) {
        in->hold_frames = 0;
        in->hold_mask = 0;
    } else if (nav_now != in->hold_mask) {
        /* direction changed — treat as fresh press, no multi-skip */
        in->hold_mask = nav_now;
        in->hold_frames = 0;
        fire |= nav_pressed ? nav_pressed : 0;
    } else {
        in->hold_frames++;
        if (in->hold_frames >= g_delay) {
            int since = in->hold_frames - g_delay;
            if (since % g_rate == 0)
                fire |= nav_now;
        }
    }
    in->repeat = (in->pressed & ~SHELL_BTN_NAV) | fire;
}

int shell_input_stick_active(float ax, float ay, float deadzone)
{
    float mag = sqrtf(ax * ax + ay * ay);
    return mag >= deadzone;
}
