/**
 * @file manual_ui.c
 * @brief 命令行交互式游戏实现（玩家设置 + 命令循环）。
 */

#include "manual_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "player_setup.h"

static void trim_newline(char *line)
{
    char *nl = strchr(line, '\n');
    if (nl != NULL) {
        *nl = '\0';
    }
    nl = strchr(line, '\r');
    if (nl != NULL) {
        *nl = '\0';
    }
}

int manual_ui_run(void)
{
    Game game;
    PlayerSetupStatus setup_status;
    char line[256];
    int i;

    game_init(&game);

    setup_status = player_setup_run(&game, stdin, stdout);
    if (setup_status != PLAYER_SETUP_OK) {
        (void)fprintf(stderr, "玩家设置失败：%s\n", player_setup_status_name(setup_status));
        return (int)setup_status;
    }

    if (game_load_map(&game, "map.json") != 0) {
        (void)fprintf(stderr, "地图加载失败\n");
        return 1;
    }

    /* 初始状态：所有玩家资金 10000、位置 0 */
    for (i = 0; i < game.user_count; ++i) {
        game.players[i].fund = 10000;
        game.players[i].credit = 0;
        game.players[i].position = 0;
        game.players[i].status = NORMAL;
        game.players[i].remaining_rounds = 0;
    }
    game.current_index = 0;

    printf("输入命令（help 查看帮助，quit 退出）：\n");
    while (game.phase != PHASE_ENDED) {
        const PLAYER *p = game_current_player_c(&game);
        if (p == NULL) {
            break;
        }

        printf("\n当前玩家 %c（位置 %d，资金 %d）：",
               p->id, (int)p->position, (int)p->fund);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        trim_newline(line);

        if (strcmp(line, "help") == 0 || strcmp(line, "HELP") == 0) {
            char buf[512];
            game_help(buf, sizeof(buf));
            printf("%s\n", buf);
        } else if (strcmp(line, "query") == 0 || strcmp(line, "QUERY") == 0) {
            char buf[512];
            game_query(&game, buf, sizeof(buf));
            printf("%s\n", buf);
        } else if (strcmp(line, "quit") == 0 || strcmp(line, "QUIT") == 0) {
            game_quit(&game);
        } else if (strncmp(line, "roll", 4) == 0 || strncmp(line, "ROLL", 4) == 0) {
            int rc = game_roll(&game);
            if (rc != 0) {
                printf("操作失败：%s\n", game_last_error());
            }
        } else if (strncmp(line, "step", 4) == 0 || strncmp(line, "STEP", 4) == 0) {
            int rc = game_step(&game, (int32_t)atoi(line + 5));
            if (rc != 0) {
                printf("操作失败：%s\n", game_last_error());
            }
        } else if (strncmp(line, "answer", 6) == 0 || strncmp(line, "ANSWER", 6) == 0) {
            int rc = game_answer(&game, line + 7);
            if (rc != 0) {
                printf("操作失败：%s\n", game_last_error());
            }
        } else if (strncmp(line, "block", 5) == 0 || strncmp(line, "BLOCK", 5) == 0) {
            int rc = game_block(&game, (int32_t)atoi(line + 6));
            if (rc != 0) {
                printf("操作失败：%s\n", game_last_error());
            }
        } else if (strncmp(line, "bomb", 4) == 0 || strncmp(line, "BOMB", 4) == 0) {
            int rc = game_bomb(&game, (int32_t)atoi(line + 5));
            if (rc != 0) {
                printf("操作失败：%s\n", game_last_error());
            }
        } else if (strcmp(line, "robot") == 0 || strcmp(line, "ROBOT") == 0) {
            int rc = game_robot(&game);
            if (rc != 0) {
                printf("操作失败：%s\n", game_last_error());
            }
        } else if (strncmp(line, "sell", 4) == 0 || strncmp(line, "SELL", 4) == 0) {
            int rc = game_sell(&game, (int32_t)atoi(line + 5));
            if (rc != 0) {
                printf("操作失败：%s\n", game_last_error());
            }
        } else {
            printf("未知命令\n");
        }
    }

    printf("游戏结束。\n");
    return 0;
}
