/**
 * @file game.c
 * @brief Shared game-state lifecycle implementation.
 */

#include "game.h"

#include <string.h>

void game_init(Game *game)
{
    int32_t index;

    if (game == NULL) {
        return;
    }

    (void)memset(game, 0, sizeof(*game));

    for (index = 0; index < MAP_SIZE; ++index) {
        game->cells[index].type = CELL_START;
    }

    game->current_index = -1;
    for (index = 0; index < MAX_PLAYERS; ++index) {
        game->players[index].id = '?';
        game->players[index].status = NORMAL;
    }

    for (index = 0; index < MAX_BOARD_ITEMS; ++index) {
        game->properties[index].position = -1;
        game->properties[index].owner_index = -1;
        game->board_items[index].position = -1;
        game->board_items[index].kind = ITEM_BLOCK;
    }

    game->phase = PHASE_COMMAND;
    game->status = GAME_RUNNING;
    game->prompt = PROMPT_NONE;
    game->winner_index = -1;
}

void game_reset(Game *game)
{
    game_init(game);
}
