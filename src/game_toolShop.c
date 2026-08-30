#include "game.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#define TOOL_SHOP_MIN_CREDIT 30
#define BLOCK_PRICE          50
#define ROBOT_PRICE          30
#define BOMB_PRICE           50

static int tool_shop_buy(Game *g, int32_t choice,
                         char *message, size_t message_size);

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

static const PLAYER *current_player_const(const Game *g)
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

static void close_shop(Game *g)
{
    g->phase = PHASE_COMMAND;
    g->prompt = PROMPT_NONE;
}

int tool_shop_show_catalog(char *buf, size_t bufsz)
{
    if (buf == NULL || bufsz == 0) {
        return -RC_INVALID_PARAMS;
    }

    write_message(
        buf, bufsz,
        "道具屋商品与使用说明：\n"
        "1. 路障（50点）：使用 block n 放在当前位置前后10步内；"
        "玩家经过时会被拦截。\n"
        "2. 机器娃娃（30点）：使用 robot 清除前方10步内的路障和炸弹。\n"
        "3. 炸弹（50点）：使用 bomb n 放在当前位置前后10步内；"
        "玩家经过时会被炸伤并送往医院。\n"
        "F. 退出道具屋（不区分大小写）。每位玩家最多持有%d个道具。",
        MAX_ITEM_TOTAL);
    return RC_OK;
}

int tool_shop_view_inventory(const Game *g, char *buf, size_t bufsz)
{
    const PLAYER *player = current_player_const(g);

    if (player == NULL || buf == NULL || bufsz == 0) {
        write_message(buf, bufsz, "无法查看道具：当前没有可行动的玩家。");
        return -RC_INVALID_PARAMS;
    }

    write_message(
        buf, bufsz,
        "当前点数：%d；道具：%d/%d。\n"
        "路障：%d个（block n，拦截经过该位置的玩家）。\n"
        "机器娃娃：%d个（robot，清除前方10步内的路障和炸弹）。\n"
        "炸弹：%d个（bomb n，炸伤经过该位置的玩家并送往医院）。",
        player->credit, item_total(player), MAX_ITEM_TOTAL,
        player->items.BLOCK, player->items.ROBOT, player->items.BOMB);
    return RC_OK;
}

int tool_shop_enter(Game *g, char *message, size_t message_size)
{
    PLAYER *player = current_player(g);
    char catalog[1024];

    if (player == NULL || player->position < 0 ||
        player->position >= MAP_SIZE ||
        g->cells[(int32_t)player->position].type != CELL_TOOL_SHOP) {
        write_message(message, message_size,
                      "无法进入道具屋：玩家当前不在道具屋地块。");
        return -RC_INVALID_PHASE;
    }

    if (g->status != GAME_RUNNING || player->status != NORMAL) {
        write_message(message, message_size,
                      "无法进入道具屋：您本回合不能行动（医院/监狱或非您的回合）。");
        return -RC_INVALID_PHASE;
    }

    if (player->credit < TOOL_SHOP_MIN_CREDIT) {
        close_shop(g);
        write_message(message, message_size,
                      "当前点数为%d，低于30点，已自动退出道具屋。",
                      player->credit);
        return RC_OK;
    }

    g->phase = PHASE_PROMPT;
    g->prompt = PROMPT_TOOL_SHOP;
    (void)tool_shop_show_catalog(catalog, sizeof(catalog));

    if (item_total(player) >= MAX_ITEM_TOTAL) {
        write_message(message, message_size,
                      "欢迎进入道具屋。当前道具已达%d个上限，不可购买；"
                      "可以查看道具或退出。\n%s",
                      MAX_ITEM_TOTAL, catalog);
    } else {
        write_message(message, message_size,
                      "欢迎进入道具屋，请选择需要的道具。\n%s", catalog);
    }

    return RC_OK;
}

int tool_shop_leave(Game *g, char *message, size_t message_size)
{
    if (g == NULL) {
        write_message(message, message_size, "退出失败：请重新开始游戏后再试。");
        return -RC_INVALID_PARAMS;
    }

    close_shop(g);
    write_message(message, message_size, "已退出道具屋。");
    return RC_OK;
}

static void write_invalid_tool_shop_input(const Game *g,
                                          char *message,
                                          size_t message_size)
{
    const PLAYER *player = current_player_const(g);
    int32_t credit = player != NULL ? player->credit : 0;

    write_message(
        message,
        message_size,
        "输入无效：请输入1、2、3购买道具，或输入F退出。您当前拥有点数 %d 点。",
        credit
    );
}

int tool_shop_answer(Game *g, const char *input,
                     char *message, size_t message_size)
{
    const unsigned char *cursor = (const unsigned char *)input;
    int32_t choice;

    if (g == NULL || g->phase != PHASE_PROMPT ||
        g->prompt != PROMPT_TOOL_SHOP) {
        write_message(message, message_size,
                      "现在不在道具屋，无法购买。走到道具屋后再选 1/2/3 或 F。");
        return -RC_INVALID_PHASE;
    }
    if (cursor == NULL) {
        write_invalid_tool_shop_input(g, message, message_size);
        return -RC_INVALID_PARAMS;
    }

    while (isspace(*cursor)) {
        ++cursor;
    }

    if (toupper(*cursor) == 'F') {
        ++cursor;
        while (isspace(*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            return tool_shop_leave(g, message, message_size);
        }
    } else if (*cursor >= '1' && *cursor <= '3') {
        choice = (int32_t)(*cursor - '0');
        ++cursor;
        while (isspace(*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            return tool_shop_buy(g, choice, message, message_size);
        }
    }

    write_invalid_tool_shop_input(g, message, message_size);
    return -RC_INVALID_PARAMS;
}

static int select_item(int32_t choice, ItemKind *kind,
                       int32_t *price, const char **name)
{
    if (kind == NULL || price == NULL || name == NULL) {
        return -RC_INVALID_PARAMS;
    }

    switch (choice) {
    case 1:
        *kind = ITEM_BLOCK;
        *price = BLOCK_PRICE;
        *name = "路障";
        return RC_OK;
    case 2:
        *kind = ITEM_ROBOT;
        *price = ROBOT_PRICE;
        *name = "机器娃娃";
        return RC_OK;
    case 3:
        *kind = ITEM_BOMB;
        *price = BOMB_PRICE;
        *name = "炸弹";
        return RC_OK;
    default:
        return -RC_INVALID_PARAMS;
    }
}

static int8_t *item_count(PLAYER *player, ItemKind kind)
{
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

static int tool_shop_buy(Game *g, int32_t choice,
                  char *message, size_t message_size)
{
    PLAYER *player = current_player(g);
    ItemKind kind;
    int32_t price;
    const char *name;
    int8_t *count;

    if (player == NULL || g->phase != PHASE_PROMPT ||
        g->prompt != PROMPT_TOOL_SHOP) {
        write_message(message, message_size,
                      "现在不在道具屋，无法购买。走到道具屋后再选 1/2/3 或 F。");
        return -RC_INVALID_PHASE;
    }

    if (player->credit < TOOL_SHOP_MIN_CREDIT) {
        close_shop(g);
        write_message(message, message_size,
                      "当前点数为%d，低于30点，已自动退出道具屋。",
                      player->credit);
        return RC_OK;
    }

    if (item_total(player) >= MAX_ITEM_TOTAL) {
        write_message(message, message_size,
                      "购买失败：道具已达%d个上限，不可购买。",
                      MAX_ITEM_TOTAL);
        return -RC_INVALID_PARAMS;
    }

    if (select_item(choice, &kind, &price, &name) != RC_OK) {
        write_message(message, message_size,
                      "购买失败：不存在编号为%d的道具。", choice);
        return -RC_INVALID_PARAMS;
    }

    if (player->credit < price) {
        write_message(message, message_size,
                      "点数不足：%s需要%d点，当前只有%d点，不可购买。",
                      name, price, player->credit);
        return -RC_INVALID_PARAMS;
    }

    count = item_count(player, kind);
    if (count == NULL) {
        write_message(message, message_size, "购买失败：道具类型无效。");
        return -RC_INTERNAL;
    }

    player->credit -= price;
    ++(*count);

    if (player->credit < TOOL_SHOP_MIN_CREDIT) {
        close_shop(g);
        write_message(message, message_size,
                      "购买%s成功，剩余%d点；点数低于30点，已自动退出道具屋。",
                      name, player->credit);
    } else if (item_total(player) >= MAX_ITEM_TOTAL) {
        write_message(message, message_size,
                      "购买%s成功，当前道具已达%d个上限，不可继续购买。",
                      name, MAX_ITEM_TOTAL);
    } else {
        write_message(message, message_size,
                      "购买%s成功，剩余%d点。", name, player->credit);
    }

    return RC_OK;
}

static int32_t normalize_position(int32_t position)
{
    position %= MAP_SIZE;
    if (position < 0) {
        position += MAP_SIZE;
    }
    return position;
}

static int board_item_index_at(const Game *g, int32_t position)
{
    int32_t i;

    for (i = 0; i < g->board_item_count; ++i) {
        if (g->board_items[i].position == position) {
            return (int)i;
        }
    }
    return -1;
}

static int validate_item_action(Game *g, PLAYER **player)
{
    *player = current_player(g);
    if (*player == NULL) {
        game_set_error("当前没有可行动的玩家。");
        return -RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        game_set_error("游戏已结束，无法再使用道具。");
        return -RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_COMMAND || (*player)->status != NORMAL) {
        game_set_error(
            "现在不能使用道具，请先完成购买/升级提示或退出商店。");
        return -RC_INVALID_PHASE;
    }
    return RC_OK;
}

static const char *item_kind_cn(ItemKind kind)
{
    switch (kind) {
    case ITEM_BLOCK: return "路障";
    case ITEM_BOMB:  return "炸弹";
    case ITEM_ROBOT: return "机器娃娃";
    default:         return "道具";
    }
}

static int place_board_item(Game *g, ItemKind kind, int32_t offset)
{
    PLAYER *player;
    int8_t *count;
    int32_t target;
    int32_t insert_at;
    const char *name = item_kind_cn(kind);
    const char *cmd = (kind == ITEM_BOMB) ? "BOMB" : "BLOCK";
    int rc = validate_item_action(g, &player);

    if (rc != RC_OK) {
        return rc;
    }
    if (offset < -BLOCK_OFFSET_LIMIT || offset > BLOCK_OFFSET_LIMIT) {
        game_set_error(
            "偏移须在 -10 到 10 之间，例如：%s 3", cmd);
        return -RC_INVALID_PARAMS;
    }

    count = item_count(player, kind);
    if (count == NULL || *count <= 0) {
        game_set_error(
            "背包里没有%s。可用 QUERY 查看道具，或到道具屋购买。",
            name);
        return -RC_INVALID_PARAMS;
    }
    if (g->board_item_count < 0 ||
        g->board_item_count >= MAX_BOARD_ITEMS) {
        game_set_error("地图上的道具已达上限，无法再放置。");
        return -RC_INVALID_PARAMS;
    }

    target = normalize_position((int32_t)player->position + offset);
    if (board_item_index_at(g, target) >= 0) {
        game_set_error(
            "位置 %d 已有路障或炸弹，请换一个偏移（-10~10）。",
            target);
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
        game_set_error(
            "背包里没有机器娃娃，无法清理前方道具。可用 QUERY 查看，或到道具屋购买。");
        return -RC_INVALID_PARAMS;
    }

    for (read_index = 0; read_index < g->board_item_count; ++read_index) {
        int32_t distance = normalize_position(
            g->board_items[read_index].position - (int32_t)player->position);

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

int tool_shop_use_item(Game *g, ItemKind kind, int32_t offset,
                       char *message, size_t message_size)
{
    PLAYER *player = current_player(g);
    int32_t before_count;
    int32_t target;
    int rc;

    if (player == NULL) {
        write_message(message, message_size,
                      "当前没有可行动的玩家，无法使用道具。");
        return -RC_INVALID_PARAMS;
    }
    if (g->phase != PHASE_COMMAND) {
        write_message(message, message_size,
                      "现在不能使用道具，请先完成购买/升级提示或退出商店。");
        return -RC_INVALID_PHASE;
    }

    switch (kind) {
    case ITEM_BLOCK:
        if (player->items.BLOCK <= 0) {
            write_message(message, message_size,
                          "背包里没有路障。可用 QUERY 查看道具，或到道具屋购买。");
            return -RC_INVALID_PARAMS;
        }
        if (offset < -BLOCK_OFFSET_LIMIT || offset > BLOCK_OFFSET_LIMIT) {
            write_message(message, message_size,
                          "偏移须在 -10 到 10 之间，例如：BLOCK 3");
            return -RC_INVALID_PARAMS;
        }
        target = normalize_position((int32_t)player->position + offset);
        rc = game_block(g, offset);
        if (rc == RC_OK) {
            write_message(message, message_size,
                          "路障使用成功，已放置在地图%d号位置。", target);
        } else {
            write_message(message, message_size, "%s",
                          game_last_error()[0] != '\0'
                              ? game_last_error()
                              : "路障放置失败，请换一个偏移（-10~10）。");
        }
        return rc;

    case ITEM_BOMB:
        if (player->items.BOMB <= 0) {
            write_message(message, message_size,
                          "背包里没有炸弹。可用 QUERY 查看道具，或到道具屋购买。");
            return -RC_INVALID_PARAMS;
        }
        if (offset < -BLOCK_OFFSET_LIMIT || offset > BLOCK_OFFSET_LIMIT) {
            write_message(message, message_size,
                          "偏移须在 -10 到 10 之间，例如：BOMB 5");
            return -RC_INVALID_PARAMS;
        }
        target = normalize_position((int32_t)player->position + offset);
        rc = game_bomb(g, offset);
        if (rc == RC_OK) {
            write_message(message, message_size,
                          "炸弹使用成功，已放置在地图%d号位置；"
                          "经过者会被炸伤并送往医院。", target);
        } else {
            write_message(message, message_size, "%s",
                          game_last_error()[0] != '\0'
                              ? game_last_error()
                              : "炸弹放置失败，请换一个偏移（-10~10）。");
        }
        return rc;

    case ITEM_ROBOT:
        if (player->items.ROBOT <= 0) {
            write_message(message, message_size,
                          "背包里没有机器娃娃，无法清理前方道具。");
            return -RC_INVALID_PARAMS;
        }
        before_count = g->board_item_count;
        rc = game_robot(g);
        if (rc == RC_OK) {
            write_message(message, message_size,
                          "机器娃娃使用成功，已清除前方10步内%d个路障或炸弹。",
                          before_count - g->board_item_count);
        } else {
            write_message(message, message_size, "%s",
                          game_last_error()[0] != '\0'
                              ? game_last_error()
                              : "现在不能使用机器娃娃。");
        }
        return rc;

    default:
        write_message(message, message_size,
                      "无法识别的道具类型。");
        return -RC_INVALID_PARAMS;
    }
}
