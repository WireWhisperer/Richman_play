/**
 * @file expected_checker.c
 * @brief Expected 部分匹配比较器（规范 v1.1 第 11 节）
 *
 * 算法：
 *   标量（字符串/数字/布尔/null）→ 完全相等；
 *   对象 → 递归部分匹配，只比对 expected 里写出的键；
 *   数组 → 按主键匹配，不按顺序、不比长度：
 *     players/id、properties/position、map_items/position、display_players/position；
 *   *_absent → 断言对应主键在 Actual 中不存在。
 */
#include "expected_checker.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

void match_error_init(MatchError *err)
{
    memset(err, 0, sizeof(*err));
    err->code = 0;
}

/* ==================== 工具 ==================== */

static void value_to_text(const cJSON *node, char *buf, size_t sz)
{
    if (node == NULL) {
        snprintf(buf, sz, "<不存在>");
    } else if (cJSON_IsString(node)) {
        snprintf(buf, sz, "\"%s\"", node->valuestring);
    } else if (cJSON_IsNumber(node)) {
        double d = node->valuedouble;
        if (d == (double)(long long)d) {
            /* 整数值用 %.0f 输出，避开平台相关的 64 位格式符：
               mingw-w64 的 msvcrt 不认 %lld，而它的 PRId64 是 "I64d"，
               在 -pedantic 下又会报错。 */
            snprintf(buf, sz, "%.0f", d);
        } else {
            snprintf(buf, sz, "%g", d);
        }
    } else if (cJSON_IsTrue(node)) {
        snprintf(buf, sz, "true");
    } else if (cJSON_IsFalse(node)) {
        snprintf(buf, sz, "false");
    } else if (cJSON_IsNull(node)) {
        snprintf(buf, sz, "null");
    } else if (cJSON_IsArray(node)) {
        snprintf(buf, sz, "<数组>");
    } else {
        snprintf(buf, sz, "<对象>");
    }
}

/** 数组主键映射（规范 11）：数组名 -> 匹配主键 */
static const char *match_key_for(const char *key)
{
    static const char *const KEYS[][2] = {
        { "players",         "id" },
        { "properties",      "position" },
        { "map_items",       "position" },
        { "display_players", "position" },
    };
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        if (strcmp(key, KEYS[i][0]) == 0) {
            return KEYS[i][1];
        }
    }
    return NULL;
}

/** 标量完全相等（规范 11：标量 → 完全相等） */
static bool scalar_equal(const cJSON *e, const cJSON *a)
{
    if (e == NULL || a == NULL) return false;
    if (cJSON_IsString(e) && cJSON_IsString(a)) {
        return strcmp(e->valuestring, a->valuestring) == 0;
    }
    if (cJSON_IsNumber(e) && cJSON_IsNumber(a)) {
        return e->valuedouble == a->valuedouble;
    }
    if (cJSON_IsBool(e) && cJSON_IsBool(a)) {
        return cJSON_IsTrue(e) == cJSON_IsTrue(a);
    }
    if (cJSON_IsNull(e) && cJSON_IsNull(a)) {
        return true;
    }
    return false;   /* 类型不同 */
}

static int fail(MatchError *err, int code, const char *path,
                const cJSON *exp, const cJSON *act, const char *fmt)
{
    err->code = code;
    snprintf(err->path, sizeof(err->path), "%s", path);
    value_to_text(exp, err->expected, sizeof(err->expected));
    value_to_text(act, err->actual, sizeof(err->actual));
    snprintf(err->message, sizeof(err->message), "%s", fmt);
    return code;
}

/* ==================== 递归匹配 ==================== */

static int match_node(const cJSON *exp, const cJSON *act, const char *path, MatchError *err);
static int match_object_in(const cJSON *exp, const cJSON *act, const char *base_path, MatchError *err);

/** 在 Actual 数组中按主键查找与 exp 项主键相同的元素 */
static cJSON *find_by_key(const cJSON *arr, const char *pk, const cJSON *exp_item)
{
    const cJSON *exp_key = cJSON_GetObjectItemCaseSensitive(exp_item, pk);
    if (exp_key == NULL || !cJSON_IsArray(arr)) {
        return NULL;
    }
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr) {
        const cJSON *act_key = cJSON_GetObjectItemCaseSensitive(item, pk);
        if (scalar_equal(exp_key, act_key)) {
            return item;
        }
    }
    return NULL;
}

static int match_array(const char *key, const cJSON *exp, const cJSON *act,
                       const char *base_path, MatchError *err)
{
    const char *pk = match_key_for(key);
    char path[MATCH_PATH_MAX];

    cJSON *exp_item = NULL;
    cJSON_ArrayForEach(exp_item, exp) {
        if (pk != NULL && cJSON_IsObject(exp_item)) {
            /* 按主键匹配（规范 11：不按顺序、不比长度） */
            cJSON *act_item = find_by_key(act, pk, exp_item);
            const cJSON *exp_key = cJSON_GetObjectItemCaseSensitive(exp_item, pk);
            char keytext[MATCH_VALUE_MAX];
            value_to_text(exp_key, keytext, sizeof(keytext));
            if (act_item == NULL) {
                snprintf(path, sizeof(path), "%s.%s[%s=%s]", base_path, key, pk, keytext);
                return fail(err, RC_ASSERT_NOT_FOUND, path, exp_item, NULL,
                            "Expected 要求的主键记录不存在");
            }
            snprintf(path, sizeof(path), "%s.%s[%s=%s]", base_path, key, pk, keytext);
            int rc = match_object_in(exp_item, act_item, path, err);
            if (rc != RC_OK) {
                return rc;
            }
        } else {
            /* 无主键数组：按位置顺次比较（规范未覆盖，防御性实现） */
            int i = 0;
            cJSON *node = exp_item;
            snprintf(path, sizeof(path), "%s.%s[%d]", base_path, key, i);
            int rc = match_node(node, cJSON_GetArrayItem(act, i), path, err);
            if (rc != RC_OK) {
                return rc;
            }
        }
    }
    return RC_OK;
}

static int match_node(const cJSON *exp, const cJSON *act, const char *path, MatchError *err)
{
    if (cJSON_IsObject(exp)) {
        if (!cJSON_IsObject(act)) {
            return fail(err, RC_ASSERT_NOT_EQUAL, path, exp, act, "类型不匹配：期望对象");
        }
        return match_object_in(exp, act, path, err);
    }
    if (cJSON_IsArray(exp)) {
        if (!cJSON_IsArray(act)) {
            return fail(err, RC_ASSERT_NOT_EQUAL, path, exp, act, "类型不匹配：期望数组");
        }
        return match_array("", exp, act, path, err);
    }
    if (!scalar_equal(exp, act)) {
        return fail(err, RC_ASSERT_NOT_EQUAL, path, exp, act, "标量不相等");
    }
    return RC_OK;
}

/** 递归部分匹配一个对象（规范 11：只比对 expected 里写出的键） */
static int match_object_in(const cJSON *exp, const cJSON *act, const char *base_path, MatchError *err)
{
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, exp) {
        const char *key = child->string;
        char path[MATCH_PATH_MAX];
        snprintf(path, sizeof(path), "%s.%s", base_path, key);

        /* *_absent 断言：对应主键在 Actual 中不存在（规范 11） */
        size_t klen = strlen(key);
        if (klen > 7 && strcmp(key + klen - 7, "_absent") == 0) {
            char arrname[64];
            snprintf(arrname, sizeof(arrname), "%.*s", (int)(klen - 7), key);
            const cJSON *act_arr = cJSON_GetObjectItemCaseSensitive(act, arrname);
            const char *pk = match_key_for(arrname);
            if (!cJSON_IsArray(child)) {
                return fail(err, RC_ASSERT_NOT_EQUAL, path, child, NULL,
                            "_absent 断言的值必须为数组");
            }
            if (pk != NULL) {
                /* 断言数组中是裸主键值（如 properties_absent: [5]），
                   在 Actual 数组中查找主键相同的记录 */
                cJSON *pos_item = NULL;
                cJSON_ArrayForEach(pos_item, child) {
                    cJSON *hit = NULL;
                    cJSON *ai = NULL;
                    cJSON_ArrayForEach(ai, act_arr) {
                        const cJSON *ak = cJSON_GetObjectItemCaseSensitive(ai, pk);
                        if (scalar_equal(pos_item, ak)) {
                            hit = ai;
                            break;
                        }
                    }
                    if (hit != NULL) {
                        char keytext[MATCH_VALUE_MAX];
                        value_to_text(pos_item, keytext, sizeof(keytext));
                        snprintf(path, sizeof(path), "%s.%s[%s=%s]", base_path, arrname, pk, keytext);
                        return fail(err, RC_ASSERT_NOT_ABSENT, path, pos_item, hit,
                                    "应不存在的对象实际存在");
                    }
                }
            }
            continue;
        }

        const cJSON *act_child = cJSON_GetObjectItemCaseSensitive(act, key);
        if (act_child == NULL) {
            return fail(err, RC_ASSERT_NOT_FOUND, path, child, NULL,
                        "Actual 缺少该键");
        }
        if (cJSON_IsArray(child)) {
            if (!cJSON_IsArray(act_child)) {
                return fail(err, RC_ASSERT_NOT_EQUAL, path, child, act_child,
                            "类型不匹配：期望数组");
            }
            int rc = match_array(key, child, act_child, base_path, err);
            if (rc != RC_OK) {
                return rc;
            }
            continue;
        }
        int rc = match_node(child, act_child, path, err);
        if (rc != RC_OK) {
            return rc;
        }
    }
    return RC_OK;
}

int expected_check(const cJSON *expected, const cJSON *actual, MatchError *err)
{
    match_error_init(err);
    if (!cJSON_IsObject(expected) || !cJSON_IsObject(actual)) {
        return fail(err, RC_ASSERT_NOT_EQUAL, "actual", expected, actual,
                    "expected/actual 必须为对象");
    }
    /* 错误路径示例：actual.players[id=A].items.BOMB（规范 13） */
    return match_object_in(expected, actual, "actual", err);
}
