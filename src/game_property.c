/**
 * @file game_property.c
 * @brief 地产购买、升级、出售与破产处理
 */
#include "game.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int is_land_type(CellType type)
{
    return type == CELL_LAND_1 || type == CELL_LAND_2 || type == CELL_LAND_3;
}

static int property_insert(Game *g, int32_t position,
                           int32_t owner_index, int32_t level)
{
    int32_t insert_at;

    if (g == NULL || g->property_count >= MAX_BOARD_ITEMS) {
        return -1;
    }

    insert_at = g->property_count;
    while (insert_at > 0 &&
           g->properties[insert_at - 1].position > position) {
        g->properties[insert_at] = g->properties[insert_at - 1];
        --insert_at;
    }

    g->properties[insert_at].position = position;
    g->properties[insert_at].owner_index = owner_index;
    g->properties[insert_at].level = level;
    ++g->property_count;
    return 0;
}

static void property_remove_index(Game *g, int32_t index)
{
    int32_t i;

    if (g == NULL || index < 0 || index >= g->property_count) {
        return;
    }

    for (i = index; i < g->property_count - 1; ++i) {
        g->properties[i] = g->properties[i + 1];
    }
    --g->property_count;
}

static int property_index_at(const Game *g, int32_t position)
{
    int32_t i;

    for (i = 0; i < g->property_count; ++i) {
        if (g->properties[i].position == position) {
            return (int)i;
        }
    }
    return -1;
}

static int parse_yes_no(const char *value, int *is_yes)
{
    char c;

    if (value == NULL || is_yes == NULL) {
        return -1;
    }

    while (*value == ' ' || *value == '\t') {
        ++value;
    }
    c = (char)toupper((unsigned char)*value);
    if (c == 'Y') {
        *is_yes = 1;
        return 0;
    }
    if (c == 'N') {
        *is_yes = 0;
        return 0;
    }
    return -1;
}

static const char *land_level_name(int32_t level)
{
    switch (level) {
    case 0: return "空地";
    case 1: return "茅屋";
    case 2: return "洋房";
    case 3: return "摩天楼";
    default: return "建筑";
    }
}

void game_bankrupt_player(Game *g, int32_t player_index)
{
    int32_t i;

    if (g == NULL || player_index < 0 || player_index >= g->user_count) {
        return;
    }

    g->players[player_index].status = BANKRUPT;
    g->players[player_index].fund = 0;
    g->players[player_index].remaining_rounds = 0;

    for (i = g->property_count - 1; i >= 0; --i) {
        if (g->properties[i].owner_index == player_index) {
            property_remove_index(g, i);
        }
    }
}

void game_finish_action_turn(Game *g)
{
    PLAYER *player;

    if (g == NULL || g->phase == PHASE_PROMPT || g->status != GAME_RUNNING) {
        return;
    }

    player = game_current_player(g);
    if (player != NULL && player->god_of_wealth_rounds > 0) {
        --player->god_of_wealth_rounds;
    }

    game_next_turn(g);
    game_check_finish(g);
}

void handle_land_landing(Game *g, int32_t position)
{
    const Property *prop;
    PLAYER *player;
    const MapCell *cell;
    int32_t upgrade_cost;

    if (g == NULL || position < 0 || position >= MAP_SIZE) {
        return;
    }

    if (!is_land_type(g->cells[position].type)) {
        return;
    }

    player = game_current_player(g);
    if (player == NULL || player->status != NORMAL) {
        return;
    }

    prop = game_property_at(g, position);
    if (prop == NULL) {
        cell = &g->cells[position];
        g->phase = PHASE_PROMPT;
        g->prompt = PROMPT_BUY;
        (void)printf(
            "此地尚未出售，购买价格 %d 元。您当前拥有资金 %d 元。是否购买？(Y/N)：",
            cell->price,
            player->fund
        );
        return;
    }

    if (prop->owner_index == g->current_index) {
        if (prop->level >= LAND_MAX_LEVEL) {
            (void)printf("这是您自己的%s，已达最高等级。\n",
                         land_level_name(prop->level));
            return;
        }
        cell = &g->cells[position];
        upgrade_cost = cell->upgrade_cost;
        if (player->fund < upgrade_cost) {
            (void)printf(
                "这是您自己的%s，升级需 %d 元，资金不足。\n",
                land_level_name(prop->level),
                upgrade_cost
            );
            return;
        }
        g->phase = PHASE_PROMPT;
        g->prompt = PROMPT_UPGRADE;
        (void)printf(
            "这是您自己的%s，升级至%s需 %d 元。是否升级？(Y/N)：",
            land_level_name(prop->level),
            land_level_name(prop->level + 1),
            upgrade_cost
        );
        return;
    }

    get_rent(g, *prop);
}

int land_answer_buy(Game *g, const char *value,
                    char *message, size_t message_size)
{
    PLAYER *player;
    int32_t position;
    const MapCell *cell;
    int is_yes;
    int32_t price;

    if (g == NULL || value == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->phase != PHASE_PROMPT || g->prompt != PROMPT_BUY) {
        snprintf(message, message_size, "当前不在购买提示阶段。");
        return RC_INVALID_PHASE;
    }

    player = game_current_player(g);
    if (player == NULL) {
        snprintf(message, message_size, "当前玩家无效。");
        return RC_INVALID_PARAMS;
    }

    position = player->position;
    if (position < 0 || position >= MAP_SIZE ||
        game_property_at(g, position) != NULL) {
        g->phase = PHASE_COMMAND;
        g->prompt = PROMPT_NONE;
        snprintf(message, message_size, "此地已不可购买。");
        return RC_OK;
    }

    if (parse_yes_no(value, &is_yes) != 0) {
        snprintf(message, message_size, "请输入 Y（购买）或 N（放弃）。");
        return -RC_INVALID_PARAMS;
    }

    g->phase = PHASE_COMMAND;
    g->prompt = PROMPT_NONE;

    if (!is_yes) {
        snprintf(message, message_size, "已放弃购买。");
        return RC_OK;
    }

    cell = &g->cells[position];
    price = cell->price;
    if (player->fund < price) {
        snprintf(message, message_size,
                 "资金不足：需要 %d 元，当前只有 %d 元。",
                 price, player->fund);
        return RC_OK;
    }

    if (property_insert(g, position, g->current_index, 0) != 0) {
        snprintf(message, message_size, "购买失败：地产数量已达上限。");
        return RC_INTERNAL;
    }

    player->fund -= price;
    snprintf(message, message_size,
             "购买成功！位置 %d 已成为您的地产，花费 %d 元，剩余资金 %d 元。",
             position, price, player->fund);
    return RC_OK;
}

int land_answer_upgrade(Game *g, const char *value,
                        char *message, size_t message_size)
{
    PLAYER *player;
    int32_t position;
    int prop_index;
    Property *prop;
    const MapCell *cell;
    int is_yes;
    int32_t cost;
    int32_t old_level;

    if (g == NULL || value == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->phase != PHASE_PROMPT || g->prompt != PROMPT_UPGRADE) {
        snprintf(message, message_size, "当前不在升级提示阶段。");
        return RC_INVALID_PHASE;
    }

    player = game_current_player(g);
    if (player == NULL) {
        snprintf(message, message_size, "当前玩家无效。");
        return RC_INVALID_PARAMS;
    }

    position = player->position;
    prop_index = property_index_at(g, position);
    if (prop_index < 0) {
        g->phase = PHASE_COMMAND;
        g->prompt = PROMPT_NONE;
        snprintf(message, message_size, "此地没有您的地产。");
        return RC_OK;
    }

    prop = &g->properties[prop_index];
    if (prop->owner_index != g->current_index) {
        g->phase = PHASE_COMMAND;
        g->prompt = PROMPT_NONE;
        snprintf(message, message_size, "此地不属于您。");
        return RC_OK;
    }

    if (parse_yes_no(value, &is_yes) != 0) {
        snprintf(message, message_size, "请输入 Y（升级）或 N（放弃）。");
        return -RC_INVALID_PARAMS;
    }

    g->phase = PHASE_COMMAND;
    g->prompt = PROMPT_NONE;

    if (!is_yes) {
        snprintf(message, message_size, "已放弃升级。");
        return RC_OK;
    }

    if (prop->level >= LAND_MAX_LEVEL) {
        snprintf(message, message_size, "该地产已达最高等级。");
        return RC_OK;
    }

    cell = &g->cells[position];
    cost = cell->upgrade_cost;
    if (player->fund < cost) {
        snprintf(message, message_size,
                 "资金不足：升级需 %d 元，当前只有 %d 元。",
                 cost, player->fund);
        return RC_OK;
    }

    old_level = prop->level;
    prop->level += 1;
    player->fund -= cost;
    snprintf(message, message_size,
             "升级成功！位置 %d 由%s升级为%s，花费 %d 元，剩余资金 %d 元。",
             position,
             land_level_name(old_level),
             land_level_name(prop->level),
             cost,
             player->fund);
    return RC_OK;
}

int game_sell_property(Game *g, int32_t position)
{
    PLAYER *player;
    int prop_index;
    Property *prop;
    int32_t price;

    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        return RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_COMMAND) {
        return RC_INVALID_PHASE;
    }

    player = game_current_player(g);
    if (player == NULL || player->status != NORMAL) {
        return RC_INVALID_PHASE;
    }
    if (position < 0 || position >= MAP_SIZE) {
        return RC_INVALID_PARAMS;
    }

    prop_index = property_index_at(g, position);
    if (prop_index < 0) {
        (void)printf("位置 %d 没有可出售的地产。\n", position);
        return RC_INVALID_PARAMS;
    }

    prop = &g->properties[prop_index];
    if (prop->owner_index != g->current_index) {
        (void)printf("位置 %d 的地产不属于您。\n", position);
        return RC_INVALID_PARAMS;
    }

    price = property_sell_price(g, prop);
    player->fund += price;
    (void)printf(
        "已出售位置 %d 的%s，获得 %d 元，当前资金 %d 元。\n",
        position,
        land_level_name(prop->level),
        price,
        player->fund
    );
    property_remove_index(g, prop_index);
    return RC_OK;
}
