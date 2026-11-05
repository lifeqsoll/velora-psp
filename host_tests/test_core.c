#include "shell/path.h"
#include "shell/theme.h"
#include "shell/input.h"
#include "shell/library.h"
#include "shell/files.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fails;

#define CHECK(cond, msg)                                                                         \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            fprintf(stderr, "FAIL: %s\n", msg);                                                   \
            g_fails++;                                                                           \
        } else {                                                                                 \
            printf("ok: %s\n", msg);                                                              \
        }                                                                                        \
    } while (0)

static void test_path(void)
{
    char out[SHELL_PATH_MAX];
    CHECK(shell_path_sanitize("ms0:/ISO/game.iso", out, sizeof(out)) == SHELL_PATH_OK, "sanitize iso");
    CHECK(strcmp(out, "ms0:/ISO/game.iso") == 0, "iso path preserved");
    CHECK(shell_path_sanitize("ms0:/ISO/../flash0:/x", out, sizeof(out)) != SHELL_PATH_OK ||
              !shell_path_is_flash_or_system(out),
          "no flash via funny path");
    CHECK(shell_path_sanitize("flash0:/vsh", out, sizeof(out)) == SHELL_PATH_ERR_FLASH, "deny flash0");
    CHECK(shell_path_sanitize("ms0:/a/b/../../..", out, sizeof(out)) == SHELL_PATH_ERR_TRAVERSAL,
          "deny escape root");
    CHECK(shell_path_join("ms0:/PSP", "GAME", out, sizeof(out)) == SHELL_PATH_OK, "join ok");
    CHECK(shell_path_join("ms0:/PSP", "../flash0", out, sizeof(out)) == SHELL_PATH_ERR_FORBIDDEN,
          "join reject slash name");
}

static void test_theme(void)
{
    ShellRgb a;
    ShellTheme t;
    shell_theme_default(&t);
    a = shell_theme_from_hsv(0.5f, 0.8f, 0.9f);
    CHECK(shell_theme_accent_readable(a, t.bg, t.text), "cyan-ish readable");
    CHECK(!shell_theme_accent_readable((ShellRgb){30, 30, 32}, t.bg, t.text), "near-bg rejected");
    a = t.accent;
    CHECK(shell_theme_stick_pick(0.01f, 0.01f, 0.25f, 0.85f, &a) == 0, "deadzone holds");
    CHECK(shell_theme_stick_pick(0.9f, 0.0f, 0.25f, 0.85f, &a) == 1, "stick updates accent");
}

static void test_input(void)
{
    ShellInput in;
    memset(&in, 0, sizeof(in));
    shell_input_begin_frame(&in, SHELL_BTN_CROSS, 0, 0);
    CHECK(in.pressed == SHELL_BTN_CROSS, "edge press");
    shell_input_begin_frame(&in, SHELL_BTN_CROSS, 0, 0);
    CHECK(in.pressed == 0, "held not pressed again");
    CHECK(shell_input_stick_active(0.1f, 0.1f, 0.25f) == 0, "stick inactive");
    CHECK(shell_input_stick_active(0.5f, 0.0f, 0.25f) == 1, "stick active");
}

static void test_library(void)
{
    ShellLibrary lib, filtered;
    shell_library_init(&lib);
    shell_library_init(&filtered);
    shell_library_add(&lib, "Alpha", "ms0:/ISO/a.iso", SHELL_ENTRY_GAME_ISO);
    shell_library_add(&lib, "Beta Game", "ms0:/ISO/b.cso", SHELL_ENTRY_GAME_ISO);
    shell_library_add(&lib, "Gamma", "ms0:/PSP/GAME/G/EBOOT.PBP", SHELL_ENTRY_HOMEBREW);
    CHECK(shell_library_is_iso_name("x.CSO"), "cso detect");
    CHECK(shell_library_is_eboot_name("EBOOT.PBP"), "eboot detect");
    shell_library_focus_move(&lib, 1);
    CHECK(lib.focus == 1, "focus move");
    shell_library_filter(&lib, "beta", &filtered);
    CHECK(filtered.count == 1, "filter beta");
    shell_library_free(&lib);
    shell_library_free(&filtered);
}

typedef struct {
    const char *dir;
    ShellDirEnt *ents;
    size_t n;
} FakeFs;

static int fake_list(const char *dir, ShellDirEnt **out_items, size_t *out_count, void *userdata)
{
    FakeFs *fs = (FakeFs *)userdata;
    ShellDirEnt *copy;
    (void)dir;
    copy = (ShellDirEnt *)calloc(fs->n, sizeof(ShellDirEnt));
    if (!copy)
        return -1;
    memcpy(copy, fs->ents, fs->n * sizeof(ShellDirEnt));
    *out_items = copy;
    *out_count = fs->n;
    return 0;
}

static ShellFileStatus fake_copy(const char *src, const char *dst, int *cancelled, void *userdata)
{
    (void)src;
    (void)dst;
    (void)cancelled;
    (void)userdata;
    return SHELL_FOP_OK;
}

static void test_files(void)
{
    ShellFileBrowser fb;
    ShellDirEnt ents[2];
    FakeFs fs;
    int cancelled = 0;
    memset(ents, 0, sizeof(ents));
    strcpy(ents[0].name, "ISO");
    ents[0].is_dir = 1;
    strcpy(ents[1].name, "readme.txt");
    ents[1].is_dir = 0;
    ents[1].size = 10;
    fs.ents = ents;
    fs.n = 2;
    shell_files_init(&fb, "ms0:/");
    CHECK(shell_files_refresh(&fb, fake_list, &fs) == SHELL_FOP_OK, "list root");
    CHECK(fb.count == 2, "two entries");
    fb.focus = 0;
    CHECK(shell_files_enter(&fb, fake_list, &fs) == SHELL_FOP_OK, "enter ISO");
    CHECK(shell_files_copy_safe("ms0:/a.txt", "flash0:/", "a.txt", fake_copy, &cancelled, NULL) ==
              SHELL_FOP_ERR_PATH ||
          shell_files_copy_safe("ms0:/a.txt", "flash0:/", "a.txt", fake_copy, &cancelled, NULL) ==
              SHELL_FOP_ERR_FORBIDDEN ||
          shell_files_copy_safe("ms0:/a.txt", "flash0:/x", "a.txt", fake_copy, &cancelled, NULL) !=
              SHELL_FOP_OK,
          "copy reject flash dest");
    CHECK(shell_files_copy_safe("ms0:/a.txt", "ms0:/PSP", "a.txt", fake_copy, &cancelled, NULL) ==
              SHELL_FOP_OK,
          "copy allow ms0");
    shell_files_free(&fb);
}

int main(void)
{
    test_path();
    test_theme();
    test_input();
    test_library();
    test_files();
    if (g_fails) {
        fprintf(stderr, "%d host test(s) failed\n", g_fails);
        return 1;
    }
    printf("all host tests passed\n");
    return 0;
}
