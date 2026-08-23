#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

/**
 * 运行一个测试文件（TC-US*.json），逐条执行并打印结果。
 * 返回失败用例数（0 表示全部通过）。
 */
int test_runner_run_file(const char *path);

#endif /* TEST_RUNNER_H */
