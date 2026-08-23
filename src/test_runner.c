/**
 * @file test_runner.c
 * @brief 测试编排实现：加载 → 重置 → 应用 preset → 执行 actions → 导出 Actual → 比较 Expected。
 */

#include "test_runner.h"

#include <stdio.h>
#include <string.h>

#include "action_executor.h"
#include "actual_writer.h"
#include "case_loader.h"
#include "cJSON.h"
#include "expected_checker.h"
#include "game.h"

typedef struct {
    const char *result;
    const char *detail;
} RunResult;

static RunResult run_one(TestCase *tc)
{
    Game game;
    RunResult result;
    int rc;

    result.result = "PASS";
    result.detail = "";

    game_init(&game);

    rc = game_load_map(&game, tc->map_file != NULL ? tc->map_file : "map.json");
    if (rc != 0) {
        result.result = "ERROR";
        result.detail = result_code_name((ResultCode)(-rc));
        return result;
    }

    rc = game_apply_preset(&game, tc->preset);
    if (rc != 0) {
        if (tc->expected_error_code != NULL &&
            strcmp(result_code_name((ResultCode)(-rc)), tc->expected_error_code) == 0) {
            result.result = "PASS";
        } else {
            result.result = "ERROR";
            result.detail = result_code_name((ResultCode)(-rc));
        }
        return result;
    }

    if (tc->actions != NULL && cJSON_IsArray(tc->actions)) {
        cJSON *action;
        cJSON_ArrayForEach(action, tc->actions) {
            rc = action_executor_run(&game, action);
            if (rc != 0) {
                if (tc->expected_error_code != NULL &&
                    strcmp(result_code_name((ResultCode)(-rc)), tc->expected_error_code) == 0) {
                    result.result = "PASS";
                } else {
                    result.result = "ERROR";
                    result.detail = result_code_name((ResultCode)(-rc));
                }
                return result;
            }
        }
    }

    if (tc->expected_error_code != NULL) {
        result.result = "FAIL";
        result.detail = "预期应报错但未报错";
    } else {
        cJSON *actual = actual_writer_build(&game);
        rc = expected_checker_compare(actual, tc->expected);
        if (rc != 0) {
            result.result = "FAIL";
            result.detail = result_code_name((ResultCode)(-rc));
        }
        cJSON_Delete(actual);
    }

    return result;
}

int test_runner_run_file(const char *path)
{
    TestSuite *suite;
    int pass_count = 0;
    int fail_count = 0;
    int i;

    suite = case_loader_load(path);
    if (suite == NULL) {
        printf("无法加载测试文件: %s\n", path);
        return 1;
    }

    printf("=== %s (US: %s) ===\n", path, suite->user_story != NULL ? suite->user_story : "-");

    for (i = 0; i < suite->test_count; ++i) {
        TestCase *tc = &suite->tests[i];
        RunResult result = run_one(tc);

        if (strcmp(result.result, "PASS") == 0) {
            ++pass_count;
        } else {
            ++fail_count;
        }
        printf("  [%s] %s - %s",
               result.result,
               tc->case_id != NULL ? tc->case_id : "?",
               tc->case_name != NULL ? tc->case_name : "");
        if (result.detail[0] != '\0') {
            printf(" (%s)", result.detail);
        }
        printf("\n");
    }

    printf("--- %s: %d 通过, %d 失败 ---\n", path, pass_count, fail_count);
    case_loader_free(suite);
    return fail_count;
}
