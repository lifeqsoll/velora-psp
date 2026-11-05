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

typedef int (*ShellListFn)(const char *dir, ShellDirEnt **out_items, size_t *out_count, void *userdata);

ShellFileStatus shell_files_refresh(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata);
ShellFileStatus shell_files_enter(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata);
ShellFileStatus shell_files_up(ShellFileBrowser *fb, ShellListFn list_fn, void *userdata);

typedef ShellFileStatus (*ShellCopyFn)(const char *src, const char *dst, int *cancelled, void *userdata);

ShellFileStatus shell_files_copy_safe(const char *src, const char *dst_dir, const char *name,
                                      ShellCopyFn copy_fn, int *cancelled, void *userdata);

/* Format size into buf, e.g. "12.4 MB". */
void shell_files_format_size(uint64_t bytes, char *buf, size_t buf_sz);

#ifndef HOST_TEST
int shell_files_list_ms(const char *dir, ShellDirEnt **out_items, size_t *out_count, void *userdata);
ShellFileStatus shell_files_copy_stdio(const char *src, const char *dst, int *cancelled, void *userdata);
ShellFileStatus shell_files_delete_safe(const char *path);
ShellFileStatus shell_files_rename_safe(const char *src, const char *new_name);
ShellFileStatus shell_files_move_safe(const char *src, const char *dst_dir, const char *name,
                                      int *cancelled);
/* Free bytes on volume containing path; returns 0 on success. */
int shell_files_free_space(const char *path, uint64_t *out_free);
int shell_files_ensure_dir(const char *path);
#endif

#ifdef __cplusplus
}
#endif

#endif
