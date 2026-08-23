/**
 * @file expected_checker.h
 * @brief Expected 部分匹配比较器 —— 规范 v1.1 第 11 节
 *
 * 部分匹配算法：
 *   标量（字符串/数字/布尔/null）→ 完全相等；
 *   对象 → 递归部分匹配，只比对 expected 里写出的键；
 *   数组 → 按主键匹配，不按顺序、不比长度：
 *     players/id、properties/position、map_items/position、
 *     display_players/position；
 *   properties_absent / map_items_absent → 断言对应位置不存在。
 */
#ifndef RICH_EXPECTED_CHECKER_H
#define RICH_EXPECTED_CHECKER_H

#include "cJSON.h"
#include "game.h"

#define MATCH_PATH_MAX   256
#define MATCH_VALUE_MAX  256

typedef struct {
    int  code;                        /* 0 = 匹配；否则 RC_ASSERT_*（规范 13） */
    char path[MATCH_PATH_MAX];        /* 出错路径，如 actual.players[id=A].items.BOMB */
    char expected[MATCH_VALUE_MAX];   /* 期望值文本（数字转十进制字符串） */
    char actual[MATCH_VALUE_MAX];     /* 实际值文本（数字转十进制字符串） */
    char message[MATCH_VALUE_MAX];    /* 人类可读描述 */
} MatchError;

void match_error_init(MatchError *err);

/**
 * 执行部分匹配（规范 11）。
 * @return 0 完全匹配；否则 err->code 为 RC_ASSERT_NOT_EQUAL /
 *         RC_ASSERT_NOT_FOUND / RC_ASSERT_NOT_ABSENT 之一
 */
int expected_check(const cJSON *expected, const cJSON *actual, MatchError *err);

#endif /* RICH_EXPECTED_CHECKER_H */
