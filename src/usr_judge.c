#include "game.h"

#include <stdio.h>

void get_rent(Game *g, Property p)
{
    (void)g;
    (void)p;
}

static int board_item_index(const Game *g, int32_t position)
{
    int index;

    for (index = 0; index < g->board_item_count; ++index) {
        if (g->board_items[index].position == position) {
            return index;
        }
    }

    return -1;
}

int game_move_to(Game *g, int32_t steps, int8_t last_position)
{
    int32_t step_index;
    int8_t position = last_position;

    if (g == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count) {
        return -1;
    }

    for (step_index = 0; step_index < steps; ++step_index) {
        int item_index;

        position = (int8_t)((position + 1) % MAP_SIZE);
        g->players[g->current_index].position = position;

        item_index = board_item_index(g, position);
        if (item_index >= 0) {
            game_boarditem_suc(g, &g->board_items[item_index], (int8_t)item_index);
            if (g->players[g->current_index].status == HOSPITAL) {
                break;
            }
        }
    }

    return position;
}

void game_boarditem_suc(Game *g, BoardItem *b, int8_t index)
{
    if (g == NULL || b == NULL) {
        return;
    }

    if (b->kind == ITEM_BOMB) {
        g->players[g->current_index].position = HOSPITAL_POS;
        g->players[g->current_index].status = HOSPITAL;
        game_remove_board_item(g, index);
    } else if (b->kind == ITEM_BLOCK) {
        g->players[g->current_index].position = (int8_t)b->position;
        game_remove_board_item(g, index);
        (void)printf("您已被路障阻隔在%d处！\n", b->position);
    }
}

void game_remove_board_item(Game *g, int index)
{
    int item_index;

    if (g == NULL || index < 0 || index >= g->board_item_count) {
        return;
    }

    for (item_index = index; item_index < g->board_item_count - 1; ++item_index) {
        g->board_items[item_index] = g->board_items[item_index + 1];
    }

    g->board_item_count--;
}
