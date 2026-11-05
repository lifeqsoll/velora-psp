#include "shell/path.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int starts_with_ci(const char *s, const char *prefix)
{
    size_t i;
    for (i = 0; prefix[i]; ++i) {
        char a = (char)tolower((unsigned char)s[i]);
        char b = (char)tolower((unsigned char)prefix[i]);
        if (a != b)
            return 0;
    }
    return 1;
}

int shell_path_is_flash_or_system(const char *path)
{
    if (!path || !path[0])
        return 1;
    if (starts_with_ci(path, "flash0:") || starts_with_ci(path, "flash1:") ||
        starts_with_ci(path, "flash2:") || starts_with_ci(path, "flash3:"))
        return 1;
    if (starts_with_ci(path, "disc0:") || starts_with_ci(path, "umd0:") ||
        starts_with_ci(path, "host0:") || starts_with_ci(path, "host1:") ||
        starts_with_ci(path, "hostfs:") || starts_with_ci(path, "kernelfs:"))
        return 1;
    return 0;
}

int shell_path_is_allowed_root(const char *path)
{
    if (!path)
        return 0;
    return starts_with_ci(path, "ms0:") || starts_with_ci(path, "ef0:");
}

static void append_seg(char *out, size_t out_sz, size_t *len, const char *seg, size_t seglen)
{
    if (*len + 1 + seglen + 1 >= out_sz)
        return;
    if (*len > 0 && out[*len - 1] != '/')
        out[(*len)++] = '/';
    memcpy(out + *len, seg, seglen);
    *len += seglen;
    out[*len] = '\0';
}

ShellPathStatus shell_path_sanitize(const char *in, char *out, size_t out_sz)
{
    char tmp[SHELL_PATH_MAX];
    size_t i, n, len = 0;
    char root[8];
    const char *p;

    if (!in || !out || out_sz < 8)
        return SHELL_PATH_ERR_EMPTY;
    if (!in[0])
        return SHELL_PATH_ERR_EMPTY;
    if (strlen(in) >= SHELL_PATH_MAX)
        return SHELL_PATH_ERR_TOO_LONG;
    if (shell_path_is_flash_or_system(in))
        return SHELL_PATH_ERR_FLASH;
    if (!shell_path_is_allowed_root(in))
        return SHELL_PATH_ERR_BAD_ROOT;

    /* Copy root device ms0: or ef0: */
    root[0] = (char)tolower((unsigned char)in[0]);
    root[1] = (char)tolower((unsigned char)in[1]);
    root[2] = (char)tolower((unsigned char)in[2]);
    root[3] = in[3]; /* ':' */
    root[4] = '\0';
    if (root[3] != ':')
        return SHELL_PATH_ERR_BAD_ROOT;

    snprintf(tmp, sizeof(tmp), "%s", root);
    len = strlen(tmp);
    /* Ensure slash after device */
    if (len + 2 < sizeof(tmp)) {
        tmp[len++] = '/';
        tmp[len] = '\0';
    }

    p = in + 4; /* after ms0: */
    while (*p == '/' || *p == '\\')
        ++p;

    while (*p) {
        const char *start = p;
        size_t seglen;
        while (*p && *p != '/' && *p != '\\')
            ++p;
        seglen = (size_t)(p - start);
        while (*p == '/' || *p == '\\')
            ++p;

        if (seglen == 0)
            continue;
        if (seglen == 1 && start[0] == '.')
            continue;
        if (seglen == 2 && start[0] == '.' && start[1] == '.') {
            /* pop one segment after root */
            char *slash = strrchr(tmp, '/');
            if (!slash || slash <= tmp + 4)
                return SHELL_PATH_ERR_TRAVERSAL;
            /* If only root slash, deny */
            if (slash == tmp + 4 && slash[1] == '\0')
                return SHELL_PATH_ERR_TRAVERSAL;
            *slash = '\0';
            if (strlen(tmp) <= 4) {
                tmp[4] = '/';
                tmp[5] = '\0';
            }
            len = strlen(tmp);
            continue;
        }
        /* Reject weird names */
        for (i = 0; i < seglen; ++i) {
            if (start[i] == ':' || start[i] == '\0')
                return SHELL_PATH_ERR_FORBIDDEN;
        }
        append_seg(tmp, sizeof(tmp), &len, start, seglen);
        if (len >= sizeof(tmp) - 1)
            return SHELL_PATH_ERR_TOO_LONG;
    }

    n = strlen(tmp);
    if (n + 1 > out_sz)
        return SHELL_PATH_ERR_TOO_LONG;
    memcpy(out, tmp, n + 1);
    return SHELL_PATH_OK;
}

ShellPathStatus shell_path_join(const char *dir, const char *name, char *out, size_t out_sz)
{
    char combined[SHELL_PATH_MAX];
    char sanitized_dir[SHELL_PATH_MAX];
    ShellPathStatus st;
    size_t i;

    if (!dir || !name || !out)
        return SHELL_PATH_ERR_EMPTY;
    if (strchr(name, '/') || strchr(name, '\\') || strchr(name, ':') || strcmp(name, "..") == 0 ||
        strcmp(name, ".") == 0)
        return SHELL_PATH_ERR_FORBIDDEN;
    for (i = 0; name[i]; ++i) {
        if (name[i] < 32)
            return SHELL_PATH_ERR_FORBIDDEN;
    }

    st = shell_path_sanitize(dir, sanitized_dir, sizeof(sanitized_dir));
    if (st != SHELL_PATH_OK)
        return st;

    if (snprintf(combined, sizeof(combined), "%s/%s", sanitized_dir, name) >= (int)sizeof(combined))
        return SHELL_PATH_ERR_TOO_LONG;

    return shell_path_sanitize(combined, out, out_sz);
}
