/**
 * @file file_utils.c
 * @brief 文件读取工具实现。
 */

#include "file_utils.h"

#include <stdio.h>
#include <stdlib.h>

char *file_read_all(const char *path, size_t *out_len)
{
    FILE *fp;
    long size;
    char *buffer;
    size_t bytes_read;

    if (out_len != NULL) {
        *out_len = 0;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    bytes_read = fread(buffer, 1, (size_t)size, fp);
    fclose(fp);
    buffer[bytes_read] = '\0';

    if (out_len != NULL) {
        *out_len = bytes_read;
    }
    return buffer;
}
