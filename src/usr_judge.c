#include "game.h"

#include <stdio.h>

void get_rent(Game *g, Property prop)
{
    PLAYER *tenant;
    PLAYER *owner;
    const Property *land;
    int32_t rent;

    if (g == NULL) {
        return;
    }

    tenant = game_current_player(g);
    if (tenant == NULL || tenant->status != NORMAL) {
        return;
    }

    /* 规范 14.5：财神护佑时免租（领取当轮立即生效） */
    if (tenant->god_of_wealth_rounds > 0) {
        (void)game_print("财神护佑，本次免过路费。\n");
        return;
    }

    land = game_property_at(g, prop.position);
    if (land == NULL || land->owner_index < 0 ||
        land->owner_index >= g->user_count) {
        return;
    }

    if (land->owner_index == g->current_index) {
        return;
    }

    owner = &g->players[land->owner_index];

    rent = property_rent(g, land);
    tenant->fund -= rent;
    owner->fund += rent;
    (void)game_print(
        "经过业主 %c 的地产，支付过路费 %d 元，当前拥有资金 %d 元。\n",
        owner->id,
        rent,
        tenant->fund
    );

    if (tenant->fund < 0) {
        (void)game_print("玩家 %c 资金不足，宣告破产！\n", tenant->id);
        game_bankrupt_player(g, g->current_index);
        game_check_finish(g);
    }
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

/**
 * 逐格移动（规范 6）：
 *   每进入一格先检查地图道具（路障拦截并停止），再检查财神
 *   （规范 14.4：第一个进入者立即领取，领取后移动继续）。
 * @return RC_OK 成功；负数 = ResultCode（如随机流耗尽）
 */
int game_move_to(Game *g, int32_t steps, int8_t last_position)
{
    int32_t step_index;
    int8_t position = last_position;
    int rc;

    if (g == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count) {
        return -RC_INVALID_PARAMS;
    }

    for (step_index = 0; step_index < steps; ++step_index) {
        int item_index;

        position = (int8_t)((position + 1) % MAP_SIZE);
        g->players[g->current_index].position = position;

        item_index = board_item_index(g, position);
        if (item_index >= 0) {
            game_boarditem_suc(g, &g->board_items[item_index], (int8_t)item_index);
            /* 路障拦截后停止继续前进 */
            break;
        }

        /* 财神：第一个进入财神位置的玩家立即领取（规范 14.4/14.5） */
        if (g->fortune.position == position) {
            rc = game_process_fortune_pickup(g);
            if (rc != RC_OK) {
                return rc;
            }
        }
    }

    return RC_OK;
}

void game_boarditem_suc(Game *g, BoardItem *b, int8_t index)
{
    PLAYER *player;

    if (g == NULL || b == NULL) {
        return;
    }

    player = &g->players[g->current_index];

    /* v2.0 地图道具只剩 BLOCK（规范 5.3） */
    if (b->kind == ITEM_BLOCK) {
        player->position = (int8_t)b->position;
        game_remove_board_item(g, index);
        (void)game_print("您被路障拦住，停在位置 %d。\n", b->position);
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
