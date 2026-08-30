/**
 * @file game_tools.c
 * @brief 路障、炸弹和机器娃娃的使用与生效规则。
 *
 * 稳定接口（声明于 game.h）：
 *   game_block(g, offset) - 在当前玩家前后 10 格内放置一次性路障；
 *   game_bomb(g, offset)  - 在当前玩家前后 10 格内放置一次性炸弹；
 *   game_robot(g)         - 清除顺时针前方 1~10 格内的全部地图道具；
 *   game_boarditem_suc()  - 逐格移动遇到地图道具时执行效果；
 *   game_remove_board_item() - 从有序地图道具数组中移除一项。
 */
#include "game.h"

#include <stdio.h>

static int32_t normalize_position(int32_t position)
{
    position %= MAP_SIZE;
    if (position < 0) {
        position += MAP_SIZE;
    }
    return position;
}

static int8_t *inventory_count(PLAYER *player, ItemKind kind)
{
    if (player == NULL) {
        return NULL;
    }

    switch (kind) {
    case ITEM_BLOCK:
        return &player->items.BLOCK;
    case ITEM_BOMB:
        return &player->items.BOMB;
    case ITEM_ROBOT:
        return &player->items.ROBOT;
    default:
        return NULL;
    }
}

static int board_item_index_at(const Game *g, int32_t position)
{
    int32_t index;

    for (index = 0; index < g->board_item_count; ++index) {
        if (g->board_items[index].position == position) {
            return (int)index;
        }
    }
    return -1;
}

static int validate_tool_action(Game *g, PLAYER **player)
{
    if (g == NULL || player == NULL) {
        return -RC_INVALID_PARAMS;
    }

    *player = game_current_player(g);
    if (*player == NULL) {
        return -RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        return -RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_COMMAND || (*player)->status != NORMAL) {
        return -RC_INVALID_PHASE;
    }
    if (g->board_item_count < 0 ||
        g->board_item_count > MAX_BOARD_ITEMS) {
        return -RC_INVALID_PARAMS;
    }
    return RC_OK;
}

static int place_board_item(Game *g, ItemKind kind, int32_t offset)
{
    PLAYER *player;
    int8_t *count;
    int32_t target;
    int32_t insert_at;
    int rc = validate_tool_action(g, &player);

    if (rc != RC_OK) {
        return rc;
    }
    if (kind != ITEM_BLOCK && kind != ITEM_BOMB) {
        return -RC_INVALID_PARAMS;
    }
    if (offset < -BLOCK_OFFSET_LIMIT || offset > BLOCK_OFFSET_LIMIT ||
        g->board_item_count >= MAX_BOARD_ITEMS) {
        return -RC_INVALID_PARAMS;
    }

    count = inventory_count(player, kind);
    if (count == NULL || *count <= 0) {
        return -RC_INVALID_PARAMS;
    }

    target = normalize_position((int32_t)player->position + offset);
    if (board_item_index_at(g, target) >= 0) {
        return -RC_INVALID_PARAMS;
    }

    /* board_items 始终按绝对位置升序，便于查询和状态输出。 */
    insert_at = g->board_item_count;
    while (insert_at > 0 &&
           g->board_items[insert_at - 1].position > target) {
        g->board_items[insert_at] = g->board_items[insert_at - 1];
        --insert_at;
    }
    g->board_items[insert_at].position = target;
    g->board_items[insert_at].kind = kind;
    ++g->board_item_count;
    --(*count);
    return RC_OK;
}

int game_block(Game *g, int32_t offset)
{
    return place_board_item(g, ITEM_BLOCK, offset);
}

int game_bomb(Game *g, int32_t offset)
{
    return place_board_item(g, ITEM_BOMB, offset);
}

int game_robot(Game *g)
{
    PLAYER *player;
    int32_t old_count;
    int32_t read_index;
    int32_t write_index = 0;
    int rc = validate_tool_action(g, &player);

    if (rc != RC_OK) {
        return rc;
    }
    if (player->items.ROBOT <= 0) {
        return -RC_INVALID_PARAMS;
    }

    old_count = g->board_item_count;
    for (read_index = 0; read_index < old_count; ++read_index) {
        int32_t distance = normalize_position(
            g->board_items[read_index].position - (int32_t)player->position
        );

        /* 只清除顺时针前方 1~10 格；脚下、身后和第 11 格均保留。 */
        if (distance < 1 || distance > ROBOT_CLEAR_RANGE) {
            if (write_index != read_index) {
                g->board_items[write_index] = g->board_items[read_index];
            }
            ++write_index;
        }
    }

    g->board_item_count = write_index;
    for (; write_index < old_count; ++write_index) {
        g->board_items[write_index].position = -1;
        g->board_items[write_index].kind = ITEM_BLOCK;
    }
    --player->items.ROBOT;
    return RC_OK;
}

void game_boarditem_suc(Game *g, BoardItem *item, int8_t index)
{
    PLAYER *player;
    ItemKind kind;
    int32_t position;

    if (g == NULL || item == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count || index < 0 ||
        index >= g->board_item_count) {
        return;
    }

    player = &g->players[g->current_index];
    kind = item->kind;
    position = item->position;

    if (kind == ITEM_BOMB) {
        /* 起点不会调用本接口；只有逐格移动实际经过炸弹时才触发。 */
        player->position = HOSPITAL_POS;
        player->status = HOSPITAL;
        player->remaining_rounds = HOSPITAL_ROUNDS;
        game_remove_board_item(g, index);
        (void)printf("您踩到炸弹，被送往医院，需住院 %d 天！\n",
                     HOSPITAL_ROUNDS);
    } else if (kind == ITEM_BLOCK) {
        player->position = (int8_t)position;
        game_remove_board_item(g, index);
        (void)printf("您已被路障阻隔在%d处！\n", position);
    }
}

void game_remove_board_item(Game *g, int index)
{
    int item_index;

    if (g == NULL || index < 0 || index >= g->board_item_count) {
        return;
    }

    for (item_index = index;
         item_index < g->board_item_count - 1;
         ++item_index) {
        g->board_items[item_index] = g->board_items[item_index + 1];
    }

    --g->board_item_count;
    g->board_items[g->board_item_count].position = -1;
    g->board_items[g->board_item_count].kind = ITEM_BLOCK;
}
