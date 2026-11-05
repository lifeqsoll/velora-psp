#include "shell/input.h"

#include <math.h>

void shell_input_begin_frame(ShellInput *in, uint32_t buttons_now, float ax, float ay)
{
    uint32_t prev = in ? in->held : 0;
    if (!in)
        return;
    in->pressed = buttons_now & ~prev;
    in->released = prev & ~buttons_now;
    in->held = buttons_now;
    in->ax = ax;
    in->ay = ay;
}

int shell_input_stick_active(float ax, float ay, float deadzone)
{
    float mag = sqrtf(ax * ax + ay * ay);
    return mag >= deadzone;
}
