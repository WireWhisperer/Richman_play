#ifndef EXPECTED_CHECKER_H
#define EXPECTED_CHECKER_H

struct cJSON;

/**
 * 按规范 11 的部分匹配规则比较 Actual 与 Expected。
 * 返回 0 表示匹配，负数表示 ResultCode（-RC_ASSERT_NOT_EQUAL / _NOT_FOUND / _NOT_ABSENT）。
 */
int expected_checker_compare(const struct cJSON *actual, const struct cJSON *expected);

#endif /* EXPECTED_CHECKER_H */
