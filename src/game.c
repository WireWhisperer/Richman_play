/**
 * @file game.c
 * @brief Shared game-state lifecycle implementation.
 */

#include "game.h"

#include <string.h>

int tool_shop_enter(Game *g, char *message, size_t message_size);

void handle_mine_landing(Game *g, int32_t position);
void handle_jail_landing(Game *g);

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

void game_settle_landing(Game *g)
{
    int32_t position;

    if (g == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count ||
        g->current_index >= MAX_PLAYERS) {
        return;
    }

    position = g->players[g->current_index].position;
    if (position < 0 || position >= MAP_SIZE) {
        return;
    }

    switch (g->cells[position].type) {
    case CELL_MINE:
        handle_mine_landing(g, position);
        break;

    case CELL_JAIL:
        handle_jail_landing(g);
        break;

    case CELL_TOOL_SHOP:
        (void)tool_shop_enter(g, NULL, 0);
        break;

    default:
        break;
    }
}
