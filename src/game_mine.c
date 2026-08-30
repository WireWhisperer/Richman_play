#include "game.h"

/** 玩家到达矿地时，增加该矿地配置的点数。 */
void handle_mine_landing(Game *g, int32_t position)
{
    PLAYER *player = &g->players[g->current_index];
    const MapCell *cell = &g->cells[position];

    player->credit += cell->mine_points;
}