/**
 * @file file_utils.c
 * @brief 文件读写与 JSON 类型安全工具（规范 v1.1 第 2 节）
 */
#include "file_utils.h"
#include "game.h"       /* ResultCode */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *fu_read_file(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    /* +1 为结尾 '\0'；分配结果必须检查（规范 14.1） */
    char *buf = (char *)malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    if (n != (size_t)size) {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';

    /* 拒绝 UTF-8 BOM（规范 2.3：省略 BOM 避免语义出错） */
    if (n >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        free(buf);
        return NULL;
    }

    if (out_len != NULL) {
        *out_len = n;
    }
    return buf;
}

int fu_write_file(const char *path, const char *data)
{
    FILE *fp = fopen(path, "wb");   /* 二进制模式：不转换换行符，保证 UTF-8 字节原样 */
    if (fp == NULL) {
        return RC_IO_ERROR;
    }
    size_t len = strlen(data);
    size_t n = fwrite(data, 1, len, fp);
    fclose(fp);
    return (n == len) ? RC_OK : RC_IO_ERROR;
}

int fu_parse_json(const char *text, cJSON **out)
{
    /* cJSON 标准解析器：拒绝注释/尾随逗号/NaN/Infinity（规范 2.1） */
    cJSON *root = cJSON_Parse(text);
    if (root == NULL) {
        *out = NULL;
        return RC_INVALID_JSON;
    }
    *out = root;
    return RC_OK;
}

bool fu_json_get_int32(const cJSON *node, int32_t *out)
{
    if (node == NULL) {
        return false;
    }
    /* 布尔必须先于数字拦截（规范 2.1：Python bool 是 int 子类，跨语言须单独拦截）。
       cJSON 中布尔是独立类型，此处显式检查以保持规范约束。 */
    if (cJSON_IsBool(node) || cJSON_IsTrue(node) || cJSON_IsFalse(node)) {
        return false;
    }
    if (!cJSON_IsNumber(node)) {
        return false;
    }
    /* 先解析到 int64 再校验 int32，避免溢出数值变为负数（规范 2.1/14.1） */
    double d = node->valuedouble;
    if (d != (double)(int64_t)d) {          /* 拒绝 800.0 这类浮点形式 */
        return false;
    }
    int64_t v = (int64_t)d;
    if (v < INT32_MIN || v > INT32_MAX) {
        return false;
    }
    *out = (int32_t)v;
    return true;
}

bool fu_json_get_uint32(const cJSON *node, int32_t *out)
{
    int32_t v;
    if (!fu_json_get_int32(node, &v) || v < 0) {
        return false;
    }
    *out = v;
    return true;
}

bool fu_json_is_bool(const cJSON *node)
{
    return node != NULL && cJSON_IsBool(node);
}

bool fu_json_get_string(const cJSON *node, char *out, size_t outsz)
{
    if (node == NULL || !cJSON_IsString(node)) {
        return false;
    }
    const char *s = node->valuestring;
    size_t len = strlen(s);
    if (len >= outsz) {     /* 必须检查缓冲区长度（规范 14.1） */
        return false;
    }
    memcpy(out, s, len + 1);
    return true;
}
