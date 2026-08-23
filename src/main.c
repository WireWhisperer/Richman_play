/**
 * @file main.c
 * @brief 程序入口。
 *
 * 带参数时进入测试模式，运行传入的测试文件（TC-US*.json）；
 * 无参数时进入命令行交互式游戏。
 */

#include <stdio.h>

#include "manual_ui.h"
#include "test_runner.h"

int main(int argc, char **argv)
{
    int i;
    int total_fail = 0;

    if (argc > 1) {
        for (i = 1; i < argc; ++i) {
            total_fail += test_runner_run_file(argv[i]);
        }
        printf("\n总计 %d 个用例失败\n", total_fail);
        return total_fail > 0 ? 1 : 0;
    }

    return manual_ui_run();
}
