#ifndef SHELL_INPUT_H
#define SHELL_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SHELL_BTN_SELECT = 1u << 0,
    SHELL_BTN_START  = 1u << 3,
    SHELL_BTN_UP     = 1u << 4,
    SHELL_BTN_RIGHT  = 1u << 5,
    SHELL_BTN_DOWN   = 1u << 6,
    SHELL_BTN_LEFT   = 1u << 7,
    SHELL_BTN_L      = 1u << 8,
    SHELL_BTN_R      = 1u << 9,
    SHELL_BTN_TRI    = 1u << 12,
    SHELL_BTN_CIRCLE = 1u << 13,
    SHELL_BTN_CROSS  = 1u << 14,
    SHELL_BTN_SQUARE = 1u << 15
};

#define SHELL_BTN_NAV (SHELL_BTN_UP | SHELL_BTN_DOWN | SHELL_BTN_LEFT | SHELL_BTN_RIGHT | SHELL_BTN_L | SHELL_BTN_R)

typedef struct {
    uint32_t held;
    uint32_t pressed;  /* newly down */
    uint32_t released;
    uint32_t repeat;   /* pressed + autorepeat for nav */
    float ax;
    float ay;
    int hold_frames;   /* frames current nav mask held */
    uint32_t hold_mask; /* which nav bits are being timed */
} ShellInput;

/* delay/rate in frames (~60Hz). */
void shell_input_set_repeat(int delay_frames, int rate_frames);
void shell_input_begin_frame(ShellInput *in, uint32_t buttons_now, float ax, float ay);
int shell_input_stick_active(float ax, float ay, float deadzone);

#ifdef __cplusplus
}
#endif

#endif
