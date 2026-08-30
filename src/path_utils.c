/**
 * @file path_utils.c
 * @brief Executable-relative path helpers.
 */
#include "path_utils.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
/* -std=c17 下需显式打开 POSIX 接口，否则 readlink 为隐式声明 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#endif

static void trim_filename(char *path)
{
    char *slash = strrchr(path, '/');
    char *backslash = strrchr(path, '\\');
    char *end = slash;

    if (backslash != NULL && (end == NULL || backslash > end)) {
        end = backslash;
    }

    if (end != NULL) {
        *end = '\0';
    }
}

int path_get_exe_dir(char *buf, size_t bufsz)
{
    if (buf == NULL || bufsz == 0U) {
        return -1;
    }

#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, buf, (DWORD)bufsz);
    if (length == 0U || length >= bufsz) {
        return -1;
    }
    trim_filename(buf);
    return 0;
#else
    ssize_t length = readlink("/proc/self/exe", buf, bufsz - 1U);
    if (length <= 0) {
        return -1;
    }
    buf[(size_t)length] = '\0';
    trim_filename(buf);
    return 0;
#endif
}

int path_join(char *buf, size_t bufsz, const char *dir, const char *name)
{
    size_t dir_len;
    size_t name_len;
    int needs_sep;

    if (buf == NULL || bufsz == 0U || dir == NULL || name == NULL) {
        return -1;
    }

    dir_len = strlen(dir);
    name_len = strlen(name);
    needs_sep = (dir_len > 0U &&
                 dir[dir_len - 1U] != '/' &&
                 dir[dir_len - 1U] != '\\');

    if (dir_len + (size_t)needs_sep + name_len + 1U > bufsz) {
        return -1;
    }

    memcpy(buf, dir, dir_len);
    if (needs_sep) {
        buf[dir_len] = '/';
        dir_len += 1U;
    }
    memcpy(buf + dir_len, name, name_len + 1U);
    return 0;
}
