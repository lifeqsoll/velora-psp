#ifndef SHELL_PATH_H
#define SHELL_PATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHELL_PATH_MAX 512

typedef enum {
    SHELL_PATH_OK = 0,
    SHELL_PATH_ERR_EMPTY,
    SHELL_PATH_ERR_TOO_LONG,
    SHELL_PATH_ERR_BAD_ROOT,
    SHELL_PATH_ERR_TRAVERSAL,
    SHELL_PATH_ERR_FLASH,
    SHELL_PATH_ERR_FORBIDDEN
} ShellPathStatus;

/* Returns 1 if root is an allowed Memory Stick / internal storage prefix. */
int shell_path_is_allowed_root(const char *path);

/* Normalize path into out (absolute ms0:/ or ef0:/ form). Rejects flash*, disc*,
 * host*, umd*, traversal past root, and empty segments that escape. */
ShellPathStatus shell_path_sanitize(const char *in, char *out, size_t out_sz);

/* Join directory + name safely under an already-sanitized directory. */
ShellPathStatus shell_path_join(const char *dir, const char *name, char *out, size_t out_sz);

/* 1 if path refers to a forbidden flash or system device for writes. */
int shell_path_is_flash_or_system(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_PATH_H */
