#include "game.h"

#include <stdlib.h>

static int move_current_player(Game *g, int32_t steps)
{
    int8_t last_position;

    if (g == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count) {
        return RC_INVALID_PARAMS;
    }

    last_position = g->players[g->current_index].position;
    (void)game_move_to(g, steps, last_position);
    game_settle_landing(g);
    game_next_turn(g);
    return RC_OK;
}

int game_roll(Game *g)
{
    int32_t steps;

    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->phase != PHASE_COMMAND || g->status != GAME_RUNNING) {
        return RC_INVALID_PHASE;
    }

    if (g->dice_next < g->dice_count) {
        steps = g->dice_seq[g->dice_next++];
    } else {
        steps = (int32_t)(rand() % DICE_MAX) + DICE_MIN;
    }

    return move_current_player(g, steps);
}

int game_step(Game *g, int32_t steps)
{
    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->phase != PHASE_COMMAND || g->status != GAME_RUNNING) {
        return RC_INVALID_PHASE;
    }
    if (steps < DICE_MIN || steps > MAP_SIZE) {
        return RC_INVALID_PARAMS;
    }

    return move_current_player(g, steps);
}
