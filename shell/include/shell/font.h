#ifndef SHELL_FONT_H
#define SHELL_FONT_H

#include "shell/theme.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FreeType-backed UI font (embedded TTF). Call once at boot. */
int shell_font_init(void);
void shell_font_shutdown(void);

int shell_font_advance(void);
int shell_font_line_height(void);
int shell_font_measure(const char *utf8);

/* plot receives alpha 0..255 for AA text; y is top of line box */
typedef void (*ShellFontPlot)(int px, int py, ShellRgb col, uint8_t alpha, void *ud);

int shell_font_utf8_draw(const char *utf8, int x, int y, ShellRgb c, ShellFontPlot plot, void *ud);

#ifdef __cplusplus
}
#endif

#endif
