#include "shell/font.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(cond, msg)                                                                         \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            fprintf(stderr, "FAIL: %s\n", msg);                                                   \
            fails++;                                                                             \
        } else {                                                                                 \
            printf("ok: %s\n", msg);                                                              \
        }                                                                                        \
    } while (0)

int main(void)
{
    const unsigned char *g;
    const char *path = "ms0:/ISO";
    const char *mixed = "Нет записей — скан ms0:/ISO";
    int w;

    g = shell_font_glyph((unsigned)'m');
    CHECK(g[0] != 0 || g[1] != 0 || g[2] != 0, "latin m has ink");
    g = shell_font_glyph((unsigned)'0');
    CHECK(g != NULL, "digit 0 glyph");
    g = shell_font_glyph((unsigned)'/');
    CHECK(g != NULL, "slash glyph");
    g = shell_font_glyph(0x2014);
    CHECK(g[3] == 0xFF, "em-dash row");
    g = shell_font_glyph(0x0418); /* И */
    CHECK(g != NULL, "Cyrillic I");

    w = shell_font_measure(path);
    CHECK(w == (int)strlen(path) * shell_font_advance(), "path measure = 8*len");
    w = shell_font_measure(mixed);
    CHECK(w > shell_font_measure("Нет"), "mixed string wider");
    CHECK(shell_font_glyph(0x1F600) == shell_font_glyph((unsigned)'?'), "unknown -> ?");

    if (fails)
        return 1;
    printf("all font tests passed\n");
    return 0;
}
