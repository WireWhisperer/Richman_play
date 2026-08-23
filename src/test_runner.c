/**
 * @file test_runner.c
 * @brief 测试执行总流程（规范 v1.1 第 13/14 节）
 *
 * 步骤（规范 14）：
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
#include "test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "action_executor.h"
#include "actual_writer.h"
#include "file_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

const char *test_result_to_str(TestResult r)
{
    switch (r) {
    case RESULT_PASS:  return "PASS";
    case RESULT_FAIL:  return "FAIL";
    case RESULT_ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

/* ==================== 路径与目录工具 ==================== */

static void dir_of(const char *path, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s", path);
    char *slash = strrchr(out, '/');
    char *bslash = strrchr(out, '\\');
    if (bslash != NULL && (slash == NULL || bslash > slash)) {
        slash = bslash;
    }
    if (slash != NULL) {
        *slash = '\0';
    } else {
        snprintf(out, outsz, ".");
    }
}

static int ensure_dir(const char *dir)
{
    if (dir == NULL || dir[0] == '\0') {
        return 0;
    }
#ifdef _WIN32
    if (CreateDirectoryA(dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }
    return -1;
#else
    /* POSIX：尽力创建，已存在不视为错误 */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dir);
    return system(cmd) == 0 ? 0 : -1;
#endif
}

/** 解析 map_file：先相对测试文件目录，再相对当前目录（规范 6：统一地图文件名） */
static int load_map_for_case(Game *g, const char *case_path, const char *map_file)
{
    char dir[512];
    dir_of(case_path, dir, sizeof(dir));
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, map_file);
    if (game_load_map(g, path) == RC_OK) {
        return RC_OK;
    }
    if (strcmp(path, map_file) != 0 && game_load_map(g, map_file) == RC_OK) {
        return RC_OK;
    }
    return RC_INVALID_MAP;
}

/* ==================== 单个用例执行 ==================== */

static void outcome_error(RunOutcome *outcome, int code, const char *msg)
{
    outcome->result = RESULT_ERROR;
    outcome->code = code;
    snprintf(outcome->message, sizeof(outcome->message), "%s", msg);
}

int runner_run_case(const char *case_path, const char *results_dir, RunOutcome *outcome)
{
    memset(outcome, 0, sizeof(*outcome));
    outcome->result = RESULT_ERROR;

    /* 步骤 1：读取并解析测试 JSON */
    TestCase tc;
    test_case_init(&tc);
    char errbuf[512];
    int rc = case_load_file(&tc, case_path, errbuf, sizeof(errbuf));
    if (rc != RC_OK) {
        outcome_error(outcome, rc, errbuf);
        return 0;
    }

    /* 步骤 2：校验 schema_version（case_load 已做）、地图和 Preset */
    rc = case_validate_preset(tc.preset, errbuf, sizeof(errbuf));
    if (rc != RC_OK) {
        outcome_error(outcome, rc, errbuf);
        case_free(&tc);
        return 0;
    }

    Game g;
    game_init(&g);
    if (load_map_for_case(&g, case_path, tc.map_file) != RC_OK) {
        snprintf(errbuf, sizeof(errbuf), "地图文件错误: %s (%s)", tc.map_file, game_last_error());
        outcome_error(outcome, RC_INVALID_MAP, errbuf);
        case_free(&tc);
        return 0;
    }

    /* 步骤 3~4：完整重置 + 加载 Preset（规范 7.1） */
    game_reset(&g);
    rc = game_apply_preset(&g, tc.preset);
    if (rc != RC_OK) {
        outcome_error(outcome, rc, game_last_error());
        case_free(&tc);
        return 0;
    }

    /* 步骤 5~6：按顺序执行 Actions */
    ActionResult ar;
    rc = action_execute_all(&g, tc.actions, &ar);
    if (rc != RC_OK) {
        outcome_error(outcome, rc, ar.message);
        case_free(&tc);
        return 0;
    }

    /* 步骤 7：导出完整 Actual JSON */
    char actual_path[1024];
    if (results_dir != NULL && results_dir[0] != '\0') {
        ensure_dir(results_dir);
        snprintf(actual_path, sizeof(actual_path), "%s/%s_actual.json", results_dir, tc.case_id);
    } else {
        snprintf(actual_path, sizeof(actual_path), "%s_actual.json", tc.case_id);
    }
    if (actual_write_file(&g, tc.case_id, actual_path) != RC_OK) {
        snprintf(errbuf, sizeof(errbuf), "Actual 导出失败: %s", actual_path);
        outcome_error(outcome, RC_IO_ERROR, errbuf);
        case_free(&tc);
        return 0;
    }
    snprintf(outcome->actual_path, sizeof(outcome->actual_path), "%s", actual_path);

    /* 步骤 8：部分匹配比较 Expected（规范 11） */
    cJSON *actual = actual_build(&g, tc.case_id);
    const cJSON *actual_content = cJSON_GetObjectItemCaseSensitive(actual, "actual");
    rc = expected_check(tc.expected, actual_content, &outcome->match_err);
    cJSON_Delete(actual);

    if (rc == RC_OK) {
        outcome->result = RESULT_PASS;
        outcome->code = RC_OK;
    } else {
        outcome->result = RESULT_FAIL;
        outcome->code = rc;
        snprintf(outcome->message, sizeof(outcome->message), "%s", outcome->match_err.message);
    }

    /* 步骤 9：生成报告 */
    char *report = runner_report_json(&tc, outcome);
    if (report != NULL) {
        char report_path[1024];
        if (results_dir != NULL && results_dir[0] != '\0') {
            snprintf(report_path, sizeof(report_path), "%s/%s_report.json", results_dir, tc.case_id);
        } else {
            snprintf(report_path, sizeof(report_path), "%s_report.json", tc.case_id);
        }
        fu_write_file(report_path, report);
        free(report);
    }
    case_free(&tc);
    return 0;
}

char *runner_report_json(const TestCase *tc, const RunOutcome *outcome)
{
    /* 规范 13 结果格式：
       { schema_version, case_id, result, errors: [{code,path,expected,actual,message}] } */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "schema_version", "1.0");
    cJSON_AddStringToObject(root, "case_id", tc->case_id);
    cJSON_AddStringToObject(root, "result", test_result_to_str(outcome->result));

    cJSON *errors = cJSON_CreateArray();
    if (outcome->result != RESULT_PASS) {
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "code", result_code_name((ResultCode)outcome->code));
        if (outcome->result == RESULT_FAIL && outcome->match_err.code != 0) {
            const MatchError *m = &outcome->match_err;
            if (m->path[0])     cJSON_AddStringToObject(e, "path", m->path);
            if (m->expected[0]) cJSON_AddStringToObject(e, "expected", m->expected);
            if (m->actual[0])   cJSON_AddStringToObject(e, "actual", m->actual);
            if (m->message[0])  cJSON_AddStringToObject(e, "message", m->message);
        } else if (outcome->message[0]) {
            cJSON_AddStringToObject(e, "message", outcome->message);
        }
        cJSON_AddItemToArray(errors, e);
    }
    cJSON_AddItemToObject(root, "errors", errors);

    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    return out;
}

/* ==================== 目录批量运行 ==================== */

static int has_json_ext(const char *name)
{
    size_t n = strlen(name);
    return n > 5 && strcmp(name + n - 5, ".json") == 0;
}

int runner_run_dir(const char *dir_path, const char *results_dir)
{
    int failures = 0;
    int ran = 0;

#ifdef _WIN32
    /* Windows：FindFirstFile（MSVC 无 dirent.h，规则 2 要求双平台构建） */
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.json", dir_path);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "无法打开测试目录: %s\n", dir_path);
        return -1;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!has_json_ext(fd.cFileName)) continue;
        char case_path[1024];
        snprintf(case_path, sizeof(case_path), "%s\\%s", dir_path, fd.cFileName);
        RunOutcome oc;
        runner_run_case(case_path, results_dir, &oc);
        ran++;
        printf("[%s] %s\n", test_result_to_str(oc.result), fd.cFileName);
        if (oc.result != RESULT_PASS) {
            failures++;
            if (oc.result == RESULT_ERROR) printf("  -> %s\n", oc.message);
            else printf("  -> %s | %s\n", oc.match_err.path, oc.match_err.message);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    /* POSIX：dirent */
    DIR *d = opendir(dir_path);
    if (d == NULL) {
        fprintf(stderr, "无法打开测试目录: %s\n", dir_path);
        return -1;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!has_json_ext(ent->d_name)) continue;
        char case_path[1024];
        snprintf(case_path, sizeof(case_path), "%s/%s", dir_path, ent->d_name);
        RunOutcome oc;
        runner_run_case(case_path, results_dir, &oc);
        ran++;
        printf("[%s] %s\n", test_result_to_str(oc.result), ent->d_name);
        if (oc.result != RESULT_PASS) {
            failures++;
            if (oc.result == RESULT_ERROR) printf("  -> %s\n", oc.message);
            else printf("  -> %s | %s\n", oc.match_err.path, oc.match_err.message);
        }
    }
    closedir(d);
#endif

    printf("共 %d 个用例，失败/错误 %d 个\n", ran, failures);
    return failures;
}
