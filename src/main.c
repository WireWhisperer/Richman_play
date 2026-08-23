/**
 * @file main.c
 * @brief Program entry point.
 */

#include <stdio.h>

#include "game.h"
#include "player_setup.h"

int main(void)
{
    Game game;
    PlayerSetupStatus status;

    game_init(&game);
    status = player_setup_run(&game, stdin, stdout);
    if (status != PLAYER_SETUP_OK) {
        (void)fprintf(
            stderr,
            "玩家设置失败：%s\n",
            player_setup_status_name(status)
        );
    }

    return status == PLAYER_SETUP_OK ? 0 : (int)status;
}
