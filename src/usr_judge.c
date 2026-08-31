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

    if (tenant->god_of_wealth_rounds > 0) {
        (void)printf("财神护佑，本次免过路费。\n");
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
    if (owner->status == HOSPITAL || owner->status == IMPRISONED) {
        (void)printf("业主正在医院/监狱，本次免过路费。\n");
        return;
    }

    rent = property_rent(g, land);
    tenant->fund -= rent;
    if(tenant->fund < 0)
    {
        (void)printf("本次租金为 %d 元，您现有资产为 %d 元，支付不起本次租金！\n", rent, tenant->fund + rent);
    }
    else
    {
        owner->fund += rent;
        (void)printf(
            "经过业主 %c 的地产，支付过路费 %d 元，当前拥有资金 %d 元。\n",
            owner->id,
            rent,
            tenant->fund);
    }
    

    if (tenant->fund < 0) {
        (void)printf("玩家 %c 资金不足，宣告破产！\n", tenant->id);
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

        if (item_index >= 0)
        {
            ItemKind kind = g->board_items[item_index].kind;

            game_boarditem_suc(
                g,
                &g->board_items[item_index],
                (int8_t)item_index);

            /*
             * 炸弹和路障会中止移动；
             * 财神 F 只是拾取，不中止移动。
             */
            if (kind == ITEM_BOMB || kind == ITEM_BLOCK)
            {
                break;
            }
        }
    }

    return position;
}

static void game_fortune_item_suc(Game *g, int item_index)
{
    PLAYER *player;

    if (g == NULL ||
        g->current_index < 0 ||
        g->current_index >= g->user_count)
    {
        return;
    }

    if (item_index < 0 || item_index >= g->board_item_count)
    {
        return;
    }

    player = &g->players[g->current_index];

    if (player->status != NORMAL)
    {
        return;
    }

    /* 获得 5 回合财神 Buff */
    player->god_of_wealth_rounds = FORTUNE_BUFF_ROUNDS;

    /* F 被拾取后从地图上消失 */
    game_remove_board_item(g, item_index);

    (void)printf(
        "玩家 %c 获得财神 Buff，接下来 %d 回合免过路费！\n",
        player->id,
        FORTUNE_BUFF_ROUNDS);
}

void game_boarditem_suc(Game *g, BoardItem *b, int8_t index)
{
    PLAYER *player;

    if (g == NULL || b == NULL) {
        return;
    }

    player = &g->players[g->current_index];

    if (b->kind == ITEM_BOMB) {
        player->position = HOSPITAL_POS;
        player->status = HOSPITAL;
        player->remaining_rounds = HOSPITAL_ROUNDS;
        game_remove_board_item(g, index);
        (void)printf("您踩到炸弹，被送往医院，需住院 %d 天！\n",
                     HOSPITAL_ROUNDS);
    } else if (b->kind == ITEM_BLOCK) {
        player->position = (int8_t)b->position;
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
