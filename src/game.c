#include "game.h"

/** 玩家到达矿地时，增加该矿地配置的点数。 */
static void handle_mine_landing(Game *g, int32_t position)
{
    PLAYER *player = &g->players[g->current_index];
    const MapCell *cell = &g->cells[position];

    player->credit += cell->mine_points;
}

/** 玩家到达监狱时，进入监禁状态并轮空两次。 */
static void handle_jail_landing(Game *g)
{
    PLAYER *player = &g->players[g->current_index];

    player->status = IMPRISONED;
    player->remaining_rounds = JAIL_ROUNDS;
}

/** 根据玩家最终落点处理矿地或监狱效果。 */
void game_settle_landing(Game *g, int32_t position)
{
    if (g == NULL || position < 0 || position >= MAP_SIZE ||
        g->current_index < 0 || g->current_index >= g->user_count ||
        g->current_index >= MAX_PLAYERS) {
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
