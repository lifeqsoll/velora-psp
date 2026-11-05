#ifndef SHELL_INPUT_H
#define SHELL_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit flags for face / shoulder / system buttons (PSP-style). */
enum {
    SHELL_BTN_SELECT = 1 << 0,
    SHELL_BTN_START  = 1 << 3,
    SHELL_BTN_UP     = 1 << 4,
    SHELL_BTN_RIGHT  = 1 << 5,
    SHELL_BTN_DOWN   = 1 << 6,
    SHELL_BTN_LEFT   = 1 << 7,
    SHELL_BTN_L      = 1 << 8,
    SHELL_BTN_R      = 1 << 9,
    SHELL_BTN_TRI    = 1 << 12,
    SHELL_BTN_CIRCLE = 1 << 13,
    SHELL_BTN_CROSS  = 1 << 14,
    SHELL_BTN_SQUARE = 1 << 15
};

typedef struct {
    uint32_t held;
    uint32_t pressed;  /* edge: newly down this frame */
    uint32_t released;
    float ax; /* -1..1 */
    float ay;
} ShellInput;

void shell_input_begin_frame(ShellInput *in, uint32_t buttons_now, float ax, float ay);
int shell_input_stick_active(float ax, float ay, float deadzone);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_INPUT_H */
