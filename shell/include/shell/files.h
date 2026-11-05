#ifndef SHELL_FILES_H
#define SHELL_FILES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHELL_FOP_OK = 0,
    SHELL_FOP_ERR_PATH,
    SHELL_FOP_ERR_IO,
    SHELL_FOP_ERR_EXISTS,
    SHELL_FOP_ERR_NOSPACE,
    SHELL_FOP_ERR_CANCELLED,
    SHELL_FOP_ERR_FORBIDDEN
} ShellFileStatus;

typedef struct {
    char name[256];
    int is_dir;
    uint64_t size;
} ShellDirEnt;

typedef struct {
    ShellDirEnt *items;
    size_t count;
    size_t capacity;
    char cwd[512];
    size_t focus;
} ShellFileBrowser;

void shell_files_init(ShellFileBrowser *fb, const char *root);
void shell_files_free(ShellFileBrowser *fb);

/* Host/PSP implement listing via callback to keep core testable. */
typedef int (*ShellListFn)(const char *dir, ShellDirEnt **out_items, size_t *out_count, void *userdata);

ShellFileStatus shell_files_refresh(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata);
ShellFileStatus shell_files_enter(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata);
ShellFileStatus shell_files_up(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata);

/* Validate dest path then invoke copy callback; core enforces sanitize + no flash. */
typedef ShellFileStatus (*ShellCopyFn)(const char *src, const char *dst, int *cancelled, void *userdata);

ShellFileStatus shell_files_copy_safe(const char *src, const char *dst_dir, const char *name,
                                      ShellCopyFn copy_fn, int *cancelled, void *userdata);

#ifndef HOST_TEST
int shell_files_list_ms(const char *dir, ShellDirEnt **out_items, size_t *out_count, void *userdata);
ShellFileStatus shell_files_copy_stdio(const char *src, const char *dst, int *cancelled, void *userdata);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SHELL_FILES_H */
