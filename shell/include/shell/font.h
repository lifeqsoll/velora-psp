#ifndef SHELL_FONT_H
#define SHELL_FONT_H

#include "shell/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

const unsigned char *shell_font_glyph(unsigned cp);

int shell_font_utf8_draw(const char *utf8, int x, int y, ShellRgb c,
                         void (*plot)(int px, int py, ShellRgb col, void *ud), void *ud);

#ifdef __cplusplus
}
#endif

#endif
