/**
 * @file case_loader.c
 * @brief 测试用例文件解析实现。
 */

#include "case_loader.h"

#include <stdlib.h>

#include "cJSON.h"
#include "file_utils.h"

static const char *string_of(const cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item != NULL && cJSON_IsString(item)) {
        return item->valuestring;
    }
    return NULL;
}

TestSuite *case_loader_load(const char *path)
{
    char *text;
    cJSON *root;
    cJSON *tests_array;
    TestSuite *suite;
    int i;

    text = file_read_all(path, NULL);
    if (text == NULL) {
        return NULL;
    }

    root = cJSON_Parse(text);
    free(text);
    if (root == NULL || !cJSON_IsObject(root)) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return NULL;
    }

    suite = (TestSuite *)calloc(1, sizeof(*suite));
    if (suite == NULL) {
        cJSON_Delete(root);
        return NULL;
    }

    suite->root = root;
    suite->schema_version = string_of(root, "schema_version");
    suite->user_story = string_of(root, "user_story");

    tests_array = cJSON_GetObjectItem(root, "tests");
    if (tests_array == NULL || !cJSON_IsArray(tests_array)) {
        return suite; /* 无 tests 数组 */
    }

    suite->test_count = cJSON_GetArraySize(tests_array);
    if (suite->test_count == 0) {
        return suite;
    }

    suite->tests = (TestCase *)calloc((size_t)suite->test_count, sizeof(TestCase));
    if (suite->tests == NULL) {
        case_loader_free(suite);
        return NULL;
    }

    for (i = 0; i < suite->test_count; ++i) {
        cJSON *item = cJSON_GetArrayItem(tests_array, i);
        TestCase *tc = &suite->tests[i];

        tc->case_id = string_of(item, "case_id");
        tc->case_name = string_of(item, "case_name");
        tc->map_file = string_of(item, "map_file");
        tc->preset = cJSON_GetObjectItem(item, "preset");
        tc->actions = cJSON_GetObjectItem(item, "actions");
        tc->expected = cJSON_GetObjectItem(item, "expected");
        tc->expected_result = string_of(item, "expected_result");
        tc->expected_error_code = string_of(item, "expected_error_code");
    }

    return suite;
}

void case_loader_free(TestSuite *suite)
{
    if (suite == NULL) {
        return;
    }
    free(suite->tests);
    if (suite->root != NULL) {
        cJSON_Delete(suite->root);
    }
    free(suite);
}
