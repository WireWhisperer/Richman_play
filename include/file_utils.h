/**
 * @file file_utils.h
 * @brief 文件读写与 JSON 类型安全工具 —— 规范 v1.1 第 2 节跨语言兼容原则
 */
#ifndef RICH_FILE_UTILS_H
#define RICH_FILE_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"

/**
 * 读取整个文件为 UTF-8 文本（规范 2.3：统一 UTF-8 编码）。
 * 若文件以 BOM 开头则拒绝并返回 NULL（BOM 会造成语义出错）。
 * @param path    文件路径
 * @param out_len 输出实际字节数（不含结尾 '\0'），可为 NULL
 * @return malloc 出的字符串（调用者 free），失败返回 NULL
 */
char *fu_read_file(const char *path, size_t *out_len);

/** 将文本写入文件（UTF-8 无 BOM），0 成功 */
int fu_write_file(const char *path, const char *data);

/**
 * 解析 JSON 文本（规范 2.1：标准 JSON，无注释/尾随逗号/NaN/Infinity）。
 * @return 0 成功（*out 需 cJSON_Delete），否则 RC_INVALID_JSON
 */
int fu_parse_json(const char *text, cJSON **out);

/**
 * 从 JSON 节点安全读取 int32（规范 2.1 类型契约）：
 *   - 先解析到 int64 再校验 int32 范围，避免溢出变成负数
 *   - 拒绝布尔值（Python bool 是 int 子类，跨语言必须单独拦截）
 *   - 拒绝浮点/字符串/null
 * @return 读取成功返回 true，否则 false
 */
bool fu_json_get_int32(const cJSON *node, int32_t *out);

/** 读取非负 int32（>=0），失败返回 false */
bool fu_json_get_uint32(const cJSON *node, int32_t *out);

/** 判断节点是否为合法布尔（规范 2.1：只认 true/false，禁止 0/1） */
bool fu_json_is_bool(const cJSON *node);

/** 读取字符串字段，空串/非字符串返回 false */
bool fu_json_get_string(const cJSON *node, char *out, size_t outsz);

#endif /* RICH_FILE_UTILS_H */
