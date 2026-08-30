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

/* 测试进度/结果走 stderr，与游戏 stdout 日志分离，避免被重定向误伤。 */
#define TR_OUT stderr

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

/** 解析 map_file：相对用例目录 / 上级 / spec / cwd */
static int load_map_for_case(Game *g, const char *case_path, const char *map_file)
{
    char dir[512];
    char path[1024];
    dir_of(case_path, dir, sizeof(dir));

    /* Prefer spec/map.json so incorrect root map.json cannot shadow mines. */
    static const char *const suffixes[] = {
        "%s/../spec/%s",
        "%s/../../spec/%s",
        "%s/%s",
        "%s/../%s",
    };
    size_t i;

    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        snprintf(path, sizeof(path), suffixes[i], dir, map_file);
        if (game_load_map(g, path) == RC_OK) {
            return RC_OK;
        }
    }

    snprintf(path, sizeof(path), "spec/%s", map_file);
    if (game_load_map(g, path) == RC_OK) {
        return RC_OK;
    }
    if (game_load_map(g, map_file) == RC_OK) {
        return RC_OK;
    }
    return RC_INVALID_MAP;
}

/* ==================== 单个用例执行 ==================== */

static void outcome_error(RunOutcome *outcome, int code, const char *msg)
{
    outcome->result = RESULT_ERROR;
    outcome->code = code < 0 ? -code : code;
    snprintf(outcome->message, sizeof(outcome->message), "%s", msg);
}

static int expects_error(const TestCase *tc)
{
    return tc->expected_result[0] != '\0' &&
           (strcmp(tc->expected_result, "ERROR") == 0 ||
            strcmp(tc->expected_result, "error") == 0);
}

static int error_code_matches(const TestCase *tc, int code)
{
    const char *name;
    int abs_code = code < 0 ? -code : code;

    if (tc->expected_error_code[0] == '\0') {
        return 1;
    }
    name = result_code_name((ResultCode)abs_code);
    return strcmp(name, tc->expected_error_code) == 0;
}

static int runner_run_loaded_case(TestCase *tc, const char *case_path,
                                  const char *results_dir, RunOutcome *outcome)
{
    char errbuf[512];
    int rc;
    Game g;
    ActionResult ar;

    memset(outcome, 0, sizeof(*outcome));
    outcome->result = RESULT_ERROR;

    rc = case_validate_preset(tc->preset, errbuf, sizeof(errbuf));
    if (rc != RC_OK) {
        if (expects_error(tc) && error_code_matches(tc, rc)) {
            outcome->result = RESULT_PASS;
            outcome->code = RC_OK;
            return 0;
        }
        outcome_error(outcome, rc, errbuf);
        return 0;
    }

    game_init(&g);
    if (load_map_for_case(&g, case_path, tc->map_file) != RC_OK) {
        snprintf(errbuf, sizeof(errbuf), "地图文件错误: %s (%s)",
                 tc->map_file, game_last_error());
        outcome_error(outcome, RC_INVALID_MAP, errbuf);
        return 0;
    }

    game_reset(&g);
    rc = game_apply_preset(&g, tc->preset);
    if (rc != RC_OK) {
        if (expects_error(tc) && error_code_matches(tc, rc)) {
            outcome->result = RESULT_PASS;
            outcome->code = RC_OK;
            return 0;
        }
        outcome_error(outcome, rc, game_last_error());
        return 0;
    }

    {
        /* 静默游戏过程日志，结果行仍打印 [PASS]/[FAIL]（不重定向 stdout） */
        game_set_log_quiet(1);
        rc = action_execute_all(&g, tc->actions, &ar);
        game_set_log_quiet(0);
    }
    if (rc != RC_OK) {
        if (expects_error(tc) && error_code_matches(tc, rc)) {
            outcome->result = RESULT_PASS;
            outcome->code = RC_OK;
            return 0;
        }
        outcome_error(outcome, rc, ar.message);
        return 0;
    }

    if (expects_error(tc)) {
        snprintf(errbuf, sizeof(errbuf),
                 "期望错误 %s，但 Actions 全部成功",
                 tc->expected_error_code[0] ? tc->expected_error_code : "ERROR");
        outcome_error(outcome, RC_ASSERT_NOT_EQUAL, errbuf);
        return 0;
    }

    {
        char actual_path[1024];
        if (results_dir != NULL && results_dir[0] != '\0') {
            ensure_dir(results_dir);
            snprintf(actual_path, sizeof(actual_path), "%s/%s_actual.json",
                     results_dir, tc->case_id);
        } else {
            snprintf(actual_path, sizeof(actual_path), "%s_actual.json", tc->case_id);
        }
        if (actual_write_file(&g, tc->case_id, actual_path) != RC_OK) {
            snprintf(errbuf, sizeof(errbuf), "Actual 导出失败: %s", actual_path);
            outcome_error(outcome, RC_IO_ERROR, errbuf);
            return 0;
        }
        snprintf(outcome->actual_path, sizeof(outcome->actual_path), "%s", actual_path);
    }

    {
        cJSON *actual = actual_build(&g, tc->case_id);
        const cJSON *actual_content =
            cJSON_GetObjectItemCaseSensitive(actual, "actual");
        rc = expected_check(tc->expected, actual_content, &outcome->match_err);
        cJSON_Delete(actual);
    }

    if (rc == RC_OK) {
        outcome->result = RESULT_PASS;
        outcome->code = RC_OK;
    } else {
        outcome->result = RESULT_FAIL;
        outcome->code = rc;
        snprintf(outcome->message, sizeof(outcome->message), "%s",
                 outcome->match_err.message);
    }

    {
        char *report = runner_report_json(tc, outcome);
        if (report != NULL) {
            char report_path[1024];
            if (results_dir != NULL && results_dir[0] != '\0') {
                snprintf(report_path, sizeof(report_path), "%s/%s_report.json",
                         results_dir, tc->case_id);
            } else {
                snprintf(report_path, sizeof(report_path), "%s_report.json",
                         tc->case_id);
            }
            fu_write_file(report_path, report);
            free(report);
        }
    }
    return 0;
}

int runner_run_case(const char *case_path, const char *results_dir, RunOutcome *outcome)
{
    TestCase tc;
    char errbuf[512];
    int rc;

    test_case_init(&tc);
    rc = case_load_file(&tc, case_path, errbuf, sizeof(errbuf));
    if (rc != RC_OK) {
        memset(outcome, 0, sizeof(*outcome));
        outcome_error(outcome, rc, errbuf);
        return 0;
    }

    runner_run_loaded_case(&tc, case_path, results_dir, outcome);
    case_free(&tc);
    return 0;
}

static void print_outcome(const TestCase *tc, const RunOutcome *oc)
{
    const char *id = (tc != NULL && tc->case_id[0] != '\0') ? tc->case_id : "(unknown)";

    if (oc->result == RESULT_PASS) {
        fprintf(TR_OUT, "[PASS] %s\n", id);
        return;
    }

    fprintf(TR_OUT, "[%s] %s", test_result_to_str(oc->result), id);
    if (tc != NULL && tc->case_name[0] != '\0') {
        fprintf(TR_OUT, " — %s", tc->case_name);
    }
    fprintf(TR_OUT, "\n");

    if (oc->result == RESULT_ERROR) {
        fprintf(TR_OUT, "  code: %s\n", result_code_name((ResultCode)oc->code));
        if (oc->message[0] != '\0') {
            fprintf(TR_OUT, "  message: %s\n", oc->message);
        }
        return;
    }

    /* FAIL：输出足以定位差异的字段 */
    {
        const MatchError *m = &oc->match_err;
        fprintf(TR_OUT, "  code: %s\n", result_code_name((ResultCode)oc->code));
        if (m->path[0] != '\0') {
            fprintf(TR_OUT, "  path: %s\n", m->path);
        }
        if (m->expected[0] != '\0') {
            fprintf(TR_OUT, "  expected: %s\n", m->expected);
        }
        if (m->actual[0] != '\0') {
            fprintf(TR_OUT, "  actual: %s\n", m->actual);
        }
        if (m->message[0] != '\0') {
            fprintf(TR_OUT, "  message: %s\n", m->message);
        } else if (oc->message[0] != '\0') {
            fprintf(TR_OUT, "  message: %s\n", oc->message);
        }
        if (oc->actual_path[0] != '\0') {
            fprintf(TR_OUT, "  actual_file: %s\n", oc->actual_path);
        }
    }
}

int runner_run_file(const char *path, const char *results_dir)
{
    size_t len = 0;
    char *text;
    cJSON *root = NULL;
    char errbuf[512];
    int failures = 0;
    int ran = 0;
    const cJSON *tests;

    text = fu_read_file(path, &len);
    if (text == NULL) {
        fprintf(stderr, "无法读取: %s\n", path);
        return -1;
    }
    if (fu_parse_json(text, &root) != RC_OK || !cJSON_IsObject(root)) {
        free(text);
        fprintf(stderr, "JSON 无效: %s\n", path);
        return -1;
    }
    free(text);

    tests = cJSON_GetObjectItemCaseSensitive(root, "tests");
    if (cJSON_IsArray(tests)) {
        char schema[SCHEMA_VERSION_MAX];
        const cJSON *sv = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
        int i;

        if (!fu_json_get_string(sv, schema, sizeof(schema))) {
            snprintf(schema, sizeof(schema), "1.0");
        }

        for (i = 0; i < cJSON_GetArraySize(tests); ++i) {
            cJSON *item = cJSON_GetArrayItem(tests, i);
            cJSON *copy = cJSON_Duplicate(item, 1);
            TestCase tc;
            RunOutcome oc;

            if (copy == NULL) {
                failures++;
                continue;
            }
            if (case_load_from_object(&tc, copy, schema, errbuf, sizeof(errbuf)) !=
                RC_OK) {
                fprintf(TR_OUT, "[ERROR] %s#%d\n  message: %s\n", path, i + 1, errbuf);
                failures++;
                cJSON_Delete(copy);
                ran++;
                continue;
            }
            cJSON_Delete(copy);

            runner_run_loaded_case(&tc, path, results_dir, &oc);
            print_outcome(&tc, &oc);
            if (oc.result != RESULT_PASS) {
                failures++;
            }
            ran++;
            case_free(&tc);
        }
        cJSON_Delete(root);
        fprintf(TR_OUT, "文件 %s：%d 个用例，失败/错误 %d 个\n", path, ran, failures);
        return failures;
    }

    cJSON_Delete(root);
    {
        RunOutcome oc;
        TestCase tc;
        char load_err[512];

        test_case_init(&tc);
        if (case_load_file(&tc, path, load_err, sizeof(load_err)) != RC_OK) {
            fprintf(TR_OUT, "[ERROR] %s\n  code: %s\n  message: %s\n",
                   path, "INVALID_JSON", load_err);
            return 1;
        }
        runner_run_loaded_case(&tc, path, results_dir, &oc);
        print_outcome(&tc, &oc);
        case_free(&tc);
        return oc.result == RESULT_PASS ? 0 : 1;
    }
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

static int is_testcase_file(const char *name)
{
    /* 跳过 map.json 等非用例文件；接受 TC-*.json 或含 tests 的套件 */
    if (!has_json_ext(name)) {
        return 0;
    }
    if (strcmp(name, "map.json") == 0) {
        return 0;
    }
    return 1;
}

static int cmp_case_name(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

int runner_run_dir(const char *dir_path, const char *results_dir)
{
    enum { MAX_FILES = 256, NAME_MAX_LEN = 256 };
    char names[MAX_FILES][NAME_MAX_LEN];
    int count = 0;
    int failures = 0;
    int i;

#ifdef _WIN32
    {
        char pattern[1024];
        WIN32_FIND_DATAA fd;
        HANDLE h;

        snprintf(pattern, sizeof(pattern), "%s\\*.json", dir_path);
        h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "无法打开测试目录: %s\n", dir_path);
            return -1;
        }
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            if (!is_testcase_file(fd.cFileName) || count >= MAX_FILES) {
                continue;
            }
            snprintf(names[count], NAME_MAX_LEN, "%s", fd.cFileName);
            ++count;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(dir_path);
        struct dirent *ent;

        if (d == NULL) {
            fprintf(stderr, "无法打开测试目录: %s\n", dir_path);
            return -1;
        }
        while ((ent = readdir(d)) != NULL) {
            if (!is_testcase_file(ent->d_name) || count >= MAX_FILES) {
                continue;
            }
            snprintf(names[count], NAME_MAX_LEN, "%s", ent->d_name);
            ++count;
        }
        closedir(d);
    }
#endif

    /* Linux readdir 顺序不稳定；排序后输出与 Windows 一致、便于对照 */
    qsort(names, (size_t)count, NAME_MAX_LEN, cmp_case_name);

    for (i = 0; i < count; ++i) {
        int n;
        char case_path[1024];
#ifdef _WIN32
        snprintf(case_path, sizeof(case_path), "%s\\%s", dir_path, names[i]);
#else
        snprintf(case_path, sizeof(case_path), "%s/%s", dir_path, names[i]);
#endif
        fprintf(TR_OUT, "== %s ==\n", names[i]);
        n = runner_run_file(case_path, results_dir);
        if (n < 0) {
            failures++;
        } else {
            failures += n;
        }
    }

    fprintf(TR_OUT, "共 %d 个测试文件，失败/错误用例合计 %d 个\n", count, failures);
    (void)fflush(TR_OUT);
    return failures;
}
