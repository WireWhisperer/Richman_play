/**
 * @file expected_checker.c
 * @brief Expected 部分匹配比较实现（规范 11）。
 */

#include "expected_checker.h"

#include <string.h>

#include "cJSON.h"
#include "game.h"

/** 标量精确相等比较 */
static int scalar_equal(const cJSON *a, const cJSON *e)
{
    if (e == NULL) {
        return 1;
    }
    if (a == NULL) {
        return 0;
    }
    if (cJSON_IsString(e)) {
        return cJSON_IsString(a) && strcmp(a->valuestring, e->valuestring) == 0;
    }
    if (cJSON_IsNumber(e)) {
        return cJSON_IsNumber(a) && a->valuedouble == e->valuedouble;
    }
    if (cJSON_IsBool(e)) {
        return cJSON_IsBool(a) && a->valueint == e->valueint;
    }
    if (cJSON_IsNull(e)) {
        return cJSON_IsNull(a);
    }
    return 0;
}

/** 递归部分匹配：expected 中写出的成员必须都在 actual 中匹配 */
static int deep_match(const cJSON *actual, const cJSON *expected)
{
    cJSON *child;
    int i;
    int n;

    if (expected == NULL) {
        return 1;
    }
    if (actual == NULL) {
        return 0;
    }

    if (cJSON_IsObject(expected)) {
        if (!cJSON_IsObject(actual)) {
            return 0;
        }
        cJSON_ArrayForEach(child, expected) {
            cJSON *av = cJSON_GetObjectItem(actual, child->string);
            if (!deep_match(av, child)) {
                return 0;
            }
        }
        return 1;
    }

    if (cJSON_IsArray(expected)) {
        if (!cJSON_IsArray(actual)) {
            return 0;
        }
        n = cJSON_GetArraySize(expected);
        if (cJSON_GetArraySize(actual) < n) {
            return 0;
        }
        for (i = 0; i < n; ++i) {
            if (!deep_match(cJSON_GetArrayItem(actual, i), cJSON_GetArrayItem(expected, i))) {
                return 0;
            }
        }
        return 1;
    }

    return scalar_equal(actual, expected);
}

/** 按主键匹配列表：expected 每个元素都要在 actual 中找到同主键且匹配 */
static int match_list_by_key(const cJSON *actual, const cJSON *expected, const char *key)
{
    int i;
    int n;

    if (expected == NULL) {
        return 1;
    }
    if (!cJSON_IsArray(expected) || actual == NULL || !cJSON_IsArray(actual)) {
        return 0;
    }

    n = cJSON_GetArraySize(expected);
    for (i = 0; i < n; ++i) {
        cJSON *e = cJSON_GetArrayItem(expected, i);
        cJSON *e_key = cJSON_GetObjectItem(e, key);
        int j;
        int found = 0;

        if (e_key == NULL) {
            continue;
        }
        for (j = 0; j < cJSON_GetArraySize(actual); ++j) {
            cJSON *a = cJSON_GetArrayItem(actual, j);
            cJSON *a_key = cJSON_GetObjectItem(a, key);
            if (a_key != NULL && scalar_equal(a_key, e_key)) {
                if (!deep_match(a, e)) {
                    return 0;
                }
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    return 1;
}

/** 检查 actual 列表中不存在 positions 里的任意主键值 */
static int absent_by_key(const cJSON *actual, const cJSON *positions, const char *key)
{
    int i;
    int n;

    if (positions == NULL) {
        return 1;
    }
    if (!cJSON_IsArray(positions)) {
        return 0;
    }
    n = cJSON_GetArraySize(positions);
    for (i = 0; i < n; ++i) {
        cJSON *p = cJSON_GetArrayItem(positions, i);
        int j;
        for (j = 0; actual != NULL && j < cJSON_GetArraySize(actual); ++j) {
            cJSON *a = cJSON_GetArrayItem(actual, j);
            cJSON *a_key = cJSON_GetObjectItem(a, key);
            if (a_key != NULL && scalar_equal(a_key, p)) {
                return 0;
            }
        }
    }
    return 1;
}

int expected_checker_compare(const cJSON *actual, const cJSON *expected)
{
    cJSON *child;

    if (expected == NULL) {
        return 0;
    }
    if (!cJSON_IsObject(expected)) {
        return -RC_ASSERT_NOT_FOUND;
    }
    if (actual == NULL || !cJSON_IsObject(actual)) {
        return -RC_ASSERT_NOT_FOUND;
    }

    cJSON_ArrayForEach(child, expected) {
        const char *key = child->string;
        cJSON *av = cJSON_GetObjectItem(actual, key);

        if (strcmp(key, "players") == 0) {
            if (!match_list_by_key(av, child, "id")) {
                return -RC_ASSERT_NOT_EQUAL;
            }
        } else if (strcmp(key, "properties") == 0) {
            if (!match_list_by_key(av, child, "position")) {
                return -RC_ASSERT_NOT_EQUAL;
            }
        } else if (strcmp(key, "map_items") == 0) {
            if (!match_list_by_key(av, child, "position")) {
                return -RC_ASSERT_NOT_EQUAL;
            }
        } else if (strcmp(key, "display_players") == 0) {
            if (!match_list_by_key(av, child, "position")) {
                return -RC_ASSERT_NOT_EQUAL;
            }
        } else if (strcmp(key, "properties_absent") == 0) {
            if (!absent_by_key(cJSON_GetObjectItem(actual, "properties"), child, "position")) {
                return -RC_ASSERT_NOT_ABSENT;
            }
        } else if (strcmp(key, "map_items_absent") == 0) {
            if (!absent_by_key(cJSON_GetObjectItem(actual, "map_items"), child, "position")) {
                return -RC_ASSERT_NOT_ABSENT;
            }
        } else {
            if (!deep_match(av, child)) {
                return -RC_ASSERT_NOT_EQUAL;
            }
        }
    }

    return 0;
}
