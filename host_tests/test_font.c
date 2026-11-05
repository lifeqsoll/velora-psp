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
    const char *path = "ms0:/ISO";
    const char *mixed = "Нет записей — скан ms0:/ISO";
    int w;

    CHECK(shell_font_init() == 0, "font init");
    CHECK(shell_font_advance() > 0, "advance > 0");
    CHECK(shell_font_line_height() >= 12, "line height sane");

    w = shell_font_measure(path);
    CHECK(w > 0, "path measure > 0");
    CHECK(w >= shell_font_measure("ms0"), "path wider than prefix");
    w = shell_font_measure(mixed);
    CHECK(w > shell_font_measure("Нет"), "mixed string wider");
    CHECK(shell_font_measure("????") > 0, "unknown glyphs still advance");

    shell_font_shutdown();
    if (fails)
        return 1;
    printf("all font tests passed\n");
    return 0;
}
