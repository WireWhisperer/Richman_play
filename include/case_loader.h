/**
 * @file case_loader.h
 * @brief 测试文件读取与校验 —— 规范 v2.0 第 4/5/7/8/17 节
 */
#ifndef RICH_CASE_LOADER_H
#define RICH_CASE_LOADER_H

#include <stddef.h>
#include "cJSON.h"

#define SCHEMA_VERSION_MAX   16
#define CASE_ID_MAX          64
#define CASE_NAME_MAX       128
#define CASE_MAP_FILE_MAX   256

/**
 * 测试文件结构（规范 4）：
 * { schema_version: "2.0", case_id, case_name, mode?, map_file, preset, actions,
 *   expected_outcome?, expected_error?, expected }
 * preset/actions/expected 保留原始 JSON，由各模块按需解释。
 */
typedef struct {
    char   schema_version[SCHEMA_VERSION_MAX];
    char   case_id[CASE_ID_MAX];
    char   case_name[CASE_NAME_MAX];
    char   map_file[CASE_MAP_FILE_MAX];
    cJSON *preset;    /* 游戏前置状态（规范 8） */
    cJSON *actions;   /* 按顺序执行的输入（规范 9） */
    cJSON *expected;  /* 需要比较的预期状态（规范 11） */
    char   expected_outcome[16];    /* SUCCESS/ERROR（缺省 SUCCESS） */
    char   expected_error_code[64]; /* expected_error.code，如 INVALID_PARAMS */
    int    expected_error_action_index;  /* expected_error.action_index，-1 表示未指定 */
    char   expected_error_path[128];     /* expected_error.path，空表示未指定 */
} TestCase;

void test_case_init(TestCase *tc);

/**
 * 解析测试文件 JSON 文本并读取顶层字段。
 * @return 0 成功；RC_INVALID_JSON / RC_INVALID_PRESET / RC_UNSUPPORTED_VERSION /
 *         RC_UNSUPPORTED_MODE（mode 为 PROCESS/INTERACTIVE 时）
 */
int case_load(TestCase *tc, const char *json_text, char *errbuf, size_t errsz);

/** 读取测试文件，内部调用 fu_read_file + case_load */
int case_load_file(TestCase *tc, const char *path, char *errbuf, size_t errsz);

/**
 * 从单个用例对象加载（用于套件文件 tests[] 中的元素）。
 * schema_version 由调用方传入（套件顶层字段）。
 */
int case_load_from_object(TestCase *tc, cJSON *obj, const char *schema_version,
                          char *errbuf, size_t errsz);

/** 释放 TestCase 持有的 JSON 树 */
void case_free(TestCase *tc);

/**
 * 校验 Preset 规则（规范 7/8 + 15.2 删除项）。
 * @return 0 通过；RC_INVALID_PRESET 失败
 */
int case_validate_preset(const cJSON *preset, char *errbuf, size_t errsz);

#endif /* RICH_CASE_LOADER_H */
