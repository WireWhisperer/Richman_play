/**
 * @file test_runner.h
 * @brief 测试执行总流程 —— 规范 v1.1 第 13/14 节
 *
 * 统一执行步骤（规范 14）：
 *   1 读取并解析测试 JSON
 *   2 校验 schema_version、地图和 Preset
 *   3 完整重置游戏状态
 *   4 加载 Preset
 *   5 按顺序执行 Actions
 *   6 使用正式游戏逻辑处理命令和回答
 *   7 导出完整 Actual JSON
 *   8 按部分匹配规则比较 Expected
 *   9 生成 PASS / FAIL / ERROR 报告
 */
#ifndef RICH_TEST_RUNNER_H
#define RICH_TEST_RUNNER_H

#include "cJSON.h"
#include "game.h"
#include "case_loader.h"
#include "expected_checker.h"

typedef enum { RESULT_PASS, RESULT_FAIL, RESULT_ERROR } TestResult;

const char *test_result_to_str(TestResult r);

/** 单次运行的完整结果 */
typedef struct {
    TestResult result;          /* 规范 13：PASS / FAIL / ERROR */
    int        code;            /* FAIL/ERROR 时的错误码（ResultCode） */
    MatchError match_err;       /* FAIL 时的部分匹配错误详情 */
    char       message[256];    /* 总括错误描述 */
    char       actual_path[512];/* Actual 导出文件路径（成功导出时） */
} RunOutcome;

/**
 * 运行单个测试用例（规范 14 完整流程）。
 * map_file 相对测试文件所在目录解析；Actual 导出到 results/ 下。
 * @return 0 表示执行流程完成（结果看 outcome->result），负数表示运行器本身失败
 */
int runner_run_case(const char *case_path, const char *results_dir, RunOutcome *outcome);

/**
 * 生成规范 13 格式的结果报告 JSON：
 *   { schema_version, case_id, result, errors: [{code,path,expected,actual,message}] }
 * 返回 malloc 字符串（调用者 free）；errors 仅 FAIL/ERROR 时非空。
 */
char *runner_report_json(const TestCase *tc, const RunOutcome *outcome);

/**
 * 运行一个测试文件：支持单用例，或含 tests[] 的套件文件。
 * @return 失败/错误用例数
 */
int runner_run_file(const char *path, const char *results_dir);

/** 运行目录下全部 .json 测试文件并输出汇总，0 全部 PASS */
int runner_run_dir(const char *dir_path, const char *results_dir);

#endif /* RICH_TEST_RUNNER_H */
