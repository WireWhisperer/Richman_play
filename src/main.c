/**
 * @file main.c
 * @brief Program entry point.
 */

#include <stdio.h>

#include "game.h"
#include "manual_ui.h"
#include "player_setup.h"

int main(void)
{
    Game game;
    PlayerSetupStatus status;

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
