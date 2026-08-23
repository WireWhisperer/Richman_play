#ifndef CASE_LOADER_H
#define CASE_LOADER_H

struct cJSON;

/** 单条测试用例（字段指向内部 cJSON 树，随 suite 一起释放） */
typedef struct {
    const char *case_id;
    const char *case_name;
    const char *map_file;
    struct cJSON *preset;      /* object */
    struct cJSON *actions;     /* array */
    struct cJSON *expected;    /* object */
    const char *expected_result;     /* 可选："PASS"/"FAIL"/"ERROR" */
    const char *expected_error_code; /* 可选：错误码文本，如 "INVALID_COMMAND" */
} TestCase;

/** 一个测试文件（TC-US*.json）解析结果 */
typedef struct {
    const char *schema_version;
    const char *user_story;
    struct cJSON *root;   /* 保活整个 JSON 树，free 时统一释放 */
    TestCase *tests;
    int test_count;
} TestSuite;

/**
 * 解析一个测试文件。成功返回 TestSuite（调用者负责 case_loader_free），
 * 失败返回 NULL。
 */
TestSuite *case_loader_load(const char *path);

void case_loader_free(TestSuite *suite);

#endif /* CASE_LOADER_H */
