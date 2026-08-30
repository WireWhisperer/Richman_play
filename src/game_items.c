/**
 * @file game_items.c
 * @brief 背包道具的使用：放置路障、放置炸弹和使用机器娃娃。
 */
#include "game.h"

static int32_t normalize_position(int32_t position)
{
    position %= MAP_SIZE;
    if (position < 0) {
        position += MAP_SIZE;
    }
    return position;
}

static int8_t *item_count(PLAYER *player, ItemKind kind)
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

static int validate_item_action(Game *g, PLAYER **player)
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
    int rc = validate_item_action(g, &player);

    if (rc != RC_OK) {
        return rc;
    }
    if (offset < -BLOCK_OFFSET_LIMIT || offset > BLOCK_OFFSET_LIMIT ||
        g->board_item_count >= MAX_BOARD_ITEMS) {
        return -RC_INVALID_PARAMS;
    }

    count = item_count(player, kind);
    if (count == NULL || *count <= 0) {
        return -RC_INVALID_PARAMS;
    }

    target = normalize_position((int32_t)player->position + offset);
    if (board_item_index_at(g, target) >= 0) {
        return -RC_INVALID_PARAMS;
    }

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
    int32_t read_index;
    int32_t write_index = 0;
    int rc = validate_item_action(g, &player);

    if (rc != RC_OK) {
        return rc;
    }
    if (player->items.ROBOT <= 0) {
        return -RC_INVALID_PARAMS;
    }

    for (read_index = 0; read_index < g->board_item_count; ++read_index) {
        int32_t distance = normalize_position(
            g->board_items[read_index].position - (int32_t)player->position
        );

        if (distance < 1 || distance > ROBOT_CLEAR_RANGE) {
            if (write_index != read_index) {
                g->board_items[write_index] = g->board_items[read_index];
            }
            ++write_index;
        }
    }

    g->board_item_count = write_index;
    --player->items.ROBOT;
    return RC_OK;
}
