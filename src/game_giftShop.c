/**
 * @file game_giftShop.c
 * @brief 礼品屋：奖金、点数卡、财神
 */
#include "game.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define GIFT_BONUS_FUND     2000
#define GIFT_CREDIT_POINTS   200
#define GOD_OF_WEALTH_TURNS    5

static void write_message(char *buf, size_t bufsz, const char *text)
{
    if (buf == NULL || bufsz == 0) {
        return;
    }
    snprintf(buf, bufsz, "%s", text);
}

int gift_shop_enter(Game *g, char *message, size_t message_size)
{
    PLAYER *player;

    if (g == NULL) {
        write_message(message, message_size, "礼品屋进入失败：游戏状态无效。");
        return RC_INVALID_PARAMS;
    }

    player = game_current_player(g);
    if (player == NULL || player->position != GIFT_POS ||
        g->cells[GIFT_POS].type != CELL_GIFT_SHOP) {
        write_message(message, message_size,
                      "无法进入礼品屋：当前不在礼品屋地块。");
        return RC_INVALID_PHASE;
    }
    if (g->status != GAME_RUNNING || player->status != NORMAL) {
        write_message(message, message_size,
                      "无法进入礼品屋：当前玩家不能行动。");
        return RC_INVALID_PHASE;
    }

    g->phase = PHASE_PROMPT;
    g->prompt = PROMPT_GIFT_SHOP;
    snprintf(
        message,
        message_size,
        "欢迎光临礼品屋，请选择一件您喜欢的礼品：\n"
        "1. 奖金（+%d 元）\n"
        "2. 点数卡（+%d 点）\n"
        "3. 财神（%d 回合内免过路费）",
        GIFT_BONUS_FUND,
        GIFT_CREDIT_POINTS,
        GOD_OF_WEALTH_TURNS
    );
    return RC_OK;
}

static void close_gift_shop(Game *g)
{
    g->phase = PHASE_COMMAND;
    g->prompt = PROMPT_NONE;
}

int gift_shop_answer(Game *g, const char *input,
                     char *message, size_t message_size)
{
    PLAYER *player;
    const unsigned char *cursor;
    int32_t choice;

    if (g == NULL || input == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->phase != PHASE_PROMPT || g->prompt != PROMPT_GIFT_SHOP) {
        write_message(message, message_size,
                      "输入失败：当前不在礼品屋选择阶段。");
        return RC_INVALID_PHASE;
    }

    player = game_current_player(g);
    if (player == NULL) {
        write_message(message, message_size, "当前玩家无效。");
        return RC_INVALID_PARAMS;
    }

    cursor = (const unsigned char *)input;
    while (*cursor != '\0' && isspace(*cursor)) {
        ++cursor;
    }

    if (*cursor < '1' || *cursor > '3') {
        close_gift_shop(g);
        write_message(message, message_size,
                      "输入无效，视为放弃礼品，已离开礼品屋。");
        return RC_OK;
    }

    choice = (int32_t)(*cursor - '0');
    close_gift_shop(g);

    switch (choice) {
    case 1:
        player->fund += GIFT_BONUS_FUND;
        snprintf(message, message_size,
                 "获得奖金 %d 元！当前资金 %d 元。",
                 GIFT_BONUS_FUND, player->fund);
        break;
    case 2:
        player->credit += GIFT_CREDIT_POINTS;
        snprintf(message, message_size,
                 "获得点数卡 %d 点！当前点数 %d 点。",
                 GIFT_CREDIT_POINTS, player->credit);
        break;
    case 3:
        player->god_of_wealth_rounds = GOD_OF_WEALTH_TURNS;
        snprintf(message, message_size,
                 "财神降临！接下来 %d 回合内经过他人地产免过路费。",
                 GOD_OF_WEALTH_TURNS);
        break;
    default:
        snprintf(message, message_size, "礼品选择无效。");
        break;
    }

    return RC_OK;
}
