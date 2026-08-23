/**
 * @file main.c
 * @brief Program entry point.
 */

#include <stdio.h>
#include <string.h>

#include "console.h"
#include "game.h"
#include "manual_ui.h"
#include "path_utils.h"
#include "player_setup.h"

static int try_load_map(Game *game, const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return RC_INVALID_MAP;
    }

    return game_load_map(game, path);
}

static int load_map_with_fallback(Game *game)
{
    char exe_dir[512];
    char map_path[768];

    if (path_get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        static const char *const exe_relative_maps[] = {
            "map.json",
            "spec/map.json",
            "../spec/map.json",
            "../map.json",
        };
        size_t index;

        for (index = 0U;
             index < sizeof(exe_relative_maps) / sizeof(exe_relative_maps[0]);
             ++index) {
            if (path_join(map_path, sizeof(map_path), exe_dir,
                          exe_relative_maps[index]) == 0 &&
                try_load_map(game, map_path) == RC_OK) {
                return RC_OK;
            }
        }
    }

    {
        static const char *const cwd_maps[] = {
            "spec/map.json",
            "map.json",
            "../spec/map.json",
            "../map.json",
        };
        size_t index;

        for (index = 0U; index < sizeof(cwd_maps) / sizeof(cwd_maps[0]); ++index) {
            if (try_load_map(game, cwd_maps[index]) == RC_OK) {
                return RC_OK;
            }
        }
    }

    return RC_INVALID_MAP;
}

int main(void)
{
    Game game;
    PlayerSetupStatus status;
    int rc;

    console_init();
    game_init(&game);
    status = player_setup_run(&game, stdin, stdout);
    if (status != PLAYER_SETUP_OK) {
        (void)fprintf(
            stderr,
            "玩家设置失败：%s\n",
            player_setup_status_name(status)
        );
        console_pause_before_exit();
        return (int)status;
    }

    if (load_map_with_fallback(&game) != RC_OK) {
        (void)fprintf(
            stderr,
            "地图加载失败: %s\n"
            "请确认可执行文件同目录下有 map.json。\n",
            game_last_error()
        );
        console_pause_before_exit();
        return 2;
    }

    rc = manual_ui_run(&game);
    console_pause_before_exit();
    return rc;
}
