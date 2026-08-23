/**
 * @file case_loader.h
 * @brief 测试文件读取与校验 —— 规范 v1.1 第 6/7 节
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
 * 测试文件结构（规范 6）：
 * { schema_version, case_id, case_name, map_file, preset, actions, expected }
 * preset/actions/expected 保留原始 JSON，由各模块按需解释。
 */
typedef struct {
    char   schema_version[SCHEMA_VERSION_MAX];
    char   case_id[CASE_ID_MAX];
    char   case_name[CASE_NAME_MAX];
    char   map_file[CASE_MAP_FILE_MAX];
    cJSON *preset;    /* 游戏前置状态（规范 7） */
    cJSON *actions;   /* 按顺序执行的输入（规范 8） */
    cJSON *expected;  /* 需要比较的预期状态（规范 11） */
} TestCase;

void test_case_init(TestCase *tc);

/**
 * 解析测试文件 JSON 文本并读取顶层字段。
 * @return 0 成功；RC_INVALID_JSON / RC_INVALID_PRESET（顶层字段缺失或类型错误）
 */
int case_load(TestCase *tc, const char *json_text, char *errbuf, size_t errsz);

/** 读取测试文件，内部调用 fu_read_file + case_load */
int case_load_file(TestCase *tc, const char *path, char *errbuf, size_t errsz);

/** 释放 TestCase 持有的 JSON 树 */
void case_free(TestCase *tc);

/**
 * 校验 Preset 规则（规范 7.1）：
 *   users 2~4 名且不重复；players 与 users 一一对应且顺序一致；
 *   current_user 属于 users 且未破产；phase 必须为 COMMAND；
 *   position 0~69；地产位置不重复、owner 属于 users、level 0~3；
 *   背包三类道具合计 <= 10；map_items 仅 BLOCK/BOMB 且位置不重复；
 *   dice_sequence 每值 1~6。
 * @return 0 通过；RC_INVALID_PRESET 失败
 */
int case_validate_preset(const cJSON *preset, char *errbuf, size_t errsz);

#endif /* RICH_CASE_LOADER_H */
