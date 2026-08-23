/**
 * @file main.c
 * @brief Program entry point.
 */

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "game.h"
#include "manual_ui.h"
#include "player_setup.h"
#include "test_runner.h"

int main(int argc, char **argv)
{
    Game game;
    PlayerSetupStatus status;
    int total_fail = 0;

#ifdef _WIN32
    /* 玩家设置在 manual_ui 之前输出，因此必须在程序入口切换 UTF-8。 */
    (void)SetConsoleOutputCP(CP_UTF8);
    (void)SetConsoleCP(CP_UTF8);
#endif

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            total_fail += test_runner_run_file(argv[i]);
        }
        (void)printf("\n总计 %d 个用例失败\n", total_fail);
        return total_fail > 0 ? 1 : 0;
    }

    game_init(&game);
    if (game_load_map(&game, "map.json") != RC_OK) {
        (void)fprintf(stderr, "地图加载失败：%s\n", game_last_error());
        return (int)RC_INVALID_MAP;
    }
    status = player_setup_run(&game, stdin, stdout);
    if (status != PLAYER_SETUP_OK) {
        (void)fprintf(
            stderr,
            "玩家设置失败：%s\n",
            player_setup_status_name(status)
        );
    }

    if (status != PLAYER_SETUP_OK) {
        return (int)status;
    }

    return manual_ui_run(&game);
}
