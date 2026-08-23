#include "game.h"

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

    default:
        break;
    }
}