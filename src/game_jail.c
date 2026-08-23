#include "game.h"

/** 玩家到达监狱时，进入监禁状态并轮空两次。 */
static void handle_jail_landing(Game *g)
{
    PLAYER *player = &g->players[g->current_index];

    player->status = IMPRISONED;
    player->remaining_rounds = JAIL_ROUNDS;
}