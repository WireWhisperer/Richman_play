/**
 * @file game_toolShop.c
 * @brief 道具屋：进入商店、解析选择并购买道具。
 *
 * 对外接口由 game.h 声明：
 *   tool_shop_enter()  - 玩家落在道具屋时建立购买提示；
 *   tool_shop_answer() - 处理一次 1/2/3/F 输入。
 *
 * 道具的实际使用由 game_items.c 负责，本模块只管理购买事务。
 */
#include "game.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#define TOOL_SHOP_MIN_CREDIT 30

typedef struct {
    int32_t choice;
    ItemKind kind;
    int32_t price;
    const char *name;
} ToolShopProduct;

static const ToolShopProduct TOOL_SHOP_PRODUCTS[] = {
    {1, ITEM_BLOCK, 50, "路障"},
    {2, ITEM_ROBOT, 30, "机器娃娃"},
    {3, ITEM_BOMB, 50, "炸弹"}
};

static void write_message(char *buf, size_t bufsz, const char *format, ...)
{
    va_list args;

    if (buf == NULL || bufsz == 0) {
        return;
    }

    va_start(args, format);
    (void)vsnprintf(buf, bufsz, format, args);
    va_end(args);
}

static PLAYER *current_player(Game *g)
{
    if (g == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count ||
        g->current_index >= MAX_PLAYERS) {
        return NULL;
    }
    return &g->players[g->current_index];
}

static int32_t item_total(const PLAYER *player)
{
    return (int32_t)player->items.BLOCK +
           (int32_t)player->items.BOMB +
           (int32_t)player->items.ROBOT;
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

static const ToolShopProduct *find_product(int32_t choice)
{
    size_t index;

    for (index = 0;
         index < sizeof(TOOL_SHOP_PRODUCTS) / sizeof(TOOL_SHOP_PRODUCTS[0]);
         ++index) {
        if (TOOL_SHOP_PRODUCTS[index].choice == choice) {
            return &TOOL_SHOP_PRODUCTS[index];
        }
    }
    return NULL;
}

static void close_shop(Game *g)
{
    g->phase = PHASE_COMMAND;
    g->prompt = PROMPT_NONE;
}

static void write_catalog(char *buf, size_t bufsz)
{
    write_message(
        buf, bufsz,
        "1. 路障：50点（地图显示 #）\n"
        "2. 机器娃娃：30点\n"
        "3. 炸弹：50点（地图显示 @）\n"
        "F. 退出道具屋\n"
        "每位玩家最多持有%d个道具。",
        MAX_ITEM_TOTAL
    );
}

static int parse_choice(const char *input, int32_t *choice)
{
    const unsigned char *cursor = (const unsigned char *)input;

    if (cursor == NULL || choice == NULL) {
        return -RC_INVALID_PARAMS;
    }

    while (isspace(*cursor)) {
        ++cursor;
    }

    if (*cursor == 'F' || *cursor == 'f') {
        *choice = 0;
        ++cursor;
    } else if (*cursor >= '1' && *cursor <= '3') {
        *choice = (int32_t)(*cursor - '0');
        ++cursor;
    } else {
        return -RC_INVALID_PARAMS;
    }

    while (isspace(*cursor)) {
        ++cursor;
    }
    return *cursor == '\0' ? RC_OK : -RC_INVALID_PARAMS;
}

static int leave_shop(Game *g, char *message, size_t message_size)
{
    close_shop(g);
    write_message(message, message_size, "已退出道具屋。");
    return RC_OK;
}

static int buy_product(Game *g, int32_t choice,
                       char *message, size_t message_size)
{
    PLAYER *player = current_player(g);
    const ToolShopProduct *product = find_product(choice);
    int8_t *count;

    if (player == NULL || g->phase != PHASE_PROMPT ||
        g->prompt != PROMPT_TOOL_SHOP) {
        write_message(message, message_size,
                      "购买失败：当前不在道具屋购买阶段。");
        return -RC_INVALID_PHASE;
    }

    if (product == NULL) {
        write_message(message, message_size,
                      "购买失败：不存在编号为%d的道具。", choice);
        return -RC_INVALID_PARAMS;
    }

    if (item_total(player) >= MAX_ITEM_TOTAL) {
        write_message(message, message_size,
                      "购买失败：道具已达%d个上限，请输入F退出。",
                      MAX_ITEM_TOTAL);
        return -RC_INVALID_PARAMS;
    }

    if (player->credit < product->price) {
        write_message(message, message_size,
                      "点数不足：%s需要%d点，当前只有%d点。"
                      "您仍可选择其他道具或输入F退出。",
                      product->name, product->price, player->credit);
        return -RC_INVALID_PARAMS;
    }

    count = item_count(player, product->kind);
    if (count == NULL) {
        write_message(message, message_size, "购买失败：道具类型无效。");
        return -RC_INTERNAL;
    }

    /* 所有校验完成后再同时扣点和入包，失败路径不会产生部分更新。 */
    player->credit -= product->price;
    ++(*count);

    if (player->credit < TOOL_SHOP_MIN_CREDIT) {
        close_shop(g);
        write_message(message, message_size,
                      "购买%s成功，剩余%d点；点数不足以购买最便宜的道具，"
                      "已自动退出道具屋。",
                      product->name, player->credit);
    } else if (item_total(player) >= MAX_ITEM_TOTAL) {
        write_message(message, message_size,
                      "购买%s成功，剩余%d点；当前道具已达%d个上限，"
                      "请输入F退出。",
                      product->name, player->credit, MAX_ITEM_TOTAL);
    } else {
        write_message(message, message_size,
                      "购买%s成功，剩余%d点；可继续选择道具或输入F退出。",
                      product->name, player->credit);
    }

    return RC_OK;
}

int tool_shop_enter(Game *g, char *message, size_t message_size)
{
    PLAYER *player = current_player(g);
    char catalog[512];

    if (player == NULL || player->position < 0 ||
        player->position >= MAP_SIZE ||
        g->cells[(int32_t)player->position].type != CELL_TOOL_SHOP) {
        write_message(message, message_size,
                      "无法进入道具屋：玩家当前不在道具屋地块。");
        return -RC_INVALID_PHASE;
    }

    if (g->status != GAME_RUNNING || player->status != NORMAL) {
        write_message(message, message_size,
                      "无法进入道具屋：当前玩家不能行动。");
        return -RC_INVALID_PHASE;
    }

    if (player->credit < TOOL_SHOP_MIN_CREDIT) {
        close_shop(g);
        write_message(message, message_size,
                      "当前点数为%d，不足以购买最便宜的道具，"
                      "已自动退出道具屋。",
                      player->credit);
        return RC_OK;
    }

    g->phase = PHASE_PROMPT;
    g->prompt = PROMPT_TOOL_SHOP;
    write_catalog(catalog, sizeof(catalog));

    if (item_total(player) >= MAX_ITEM_TOTAL) {
        write_message(message, message_size,
                      "欢迎光临道具屋。当前道具已达%d个上限，"
                      "不可继续购买，请输入F退出。\n%s",
                      MAX_ITEM_TOTAL, catalog);
    } else {
        write_message(message, message_size,
                      "欢迎光临道具屋，请选择您所需要的道具。\n"
                      "当前点数：%d；背包：%d/%d。\n%s",
                      player->credit, item_total(player), MAX_ITEM_TOTAL,
                      catalog);
    }

    return RC_OK;
}

int tool_shop_answer(Game *g, const char *input,
                     char *message, size_t message_size)
{
    PLAYER *player;
    int32_t choice;

    if (g == NULL || g->status != GAME_RUNNING ||
        g->phase != PHASE_PROMPT || g->prompt != PROMPT_TOOL_SHOP) {
        write_message(message, message_size,
                      "输入失败：当前不在道具屋购买阶段。");
        return -RC_INVALID_PHASE;
    }

    player = current_player(g);
    if (player == NULL || player->status != NORMAL) {
        write_message(message, message_size,
                      "输入失败：当前玩家不能购买道具。");
        return -RC_INVALID_PARAMS;
    }

    if (player->credit < TOOL_SHOP_MIN_CREDIT) {
        close_shop(g);
        write_message(message, message_size,
                      "当前点数为%d，不足以购买最便宜的道具，"
                      "已自动退出道具屋。",
                      player->credit);
        return RC_OK;
    }

    if (parse_choice(input, &choice) != RC_OK) {
        write_message(message, message_size,
                      "输入无效：请输入1、2、3购买道具，或输入F退出。"
                      "当前点数：%d。",
                      player->credit);
        return -RC_INVALID_PARAMS;
    }

    if (choice == 0) {
        return leave_shop(g, message, message_size);
    }
    return buy_product(g, choice, message, message_size);
}
