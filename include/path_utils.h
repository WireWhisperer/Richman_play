/**
 * @file path_utils.h
 * @brief Executable-relative path helpers.
 */
#ifndef RICH_PATH_UTILS_H
#define RICH_PATH_UTILS_H

#include <stddef.h>

/** Write the directory containing the current executable, or return 0 on failure. */
int path_get_exe_dir(char *buf, size_t bufsz);

/** Join directory and relative path into buf. Returns 0 on success. */
int path_join(char *buf, size_t bufsz, const char *dir, const char *name);

#endif /* RICH_PATH_UTILS_H */
