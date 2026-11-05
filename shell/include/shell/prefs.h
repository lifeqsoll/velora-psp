#ifndef SHELL_PREFS_H
#define SHELL_PREFS_H

#include "shell/library.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int shell_prefs_save_library(const ShellLibrary *games, const ShellLibrary *apps,
                             const char recent_paths[][512], int recent_count);
int shell_prefs_load_library(ShellLibrary *games, ShellLibrary *apps,
                             char recent_paths[][512], int *recent_count, int recent_cap);

#ifdef __cplusplus
}
#endif

#endif
