#include "game.h"

int game_roll(Game *g) /* ROLL：使用预置骰子移动 */
{
    int move;
    move = rand() % 5 + 1;

    return move;
}

int game_step(Game *g, int32_t steps) /* STEP：按指定步数移动 */
{
    int32_t last_position;

    last_position = g->players[g->current_index].position;
    g->players[g->current_index].position += steps;
    g->players[g->current_index].position = g->players[g->current_index].position % 70;

    return last_position;
}