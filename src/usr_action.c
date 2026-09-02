#include "game.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * 移动并结算：先逐格移动（含途中路障/财神），再处理落点；
 * 落点不产生提示时结束回合。财神随机流错误向上传播。
 */
static int finish_move(Game *g)
{
    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }

    game_settle_landing(g);
    if (g->phase != PHASE_PROMPT) {
        return game_finish_action_turn(g);
    }
    return RC_OK;
}

static void print_roll_result(Game *g, int32_t steps)
{
    const PLAYER *player = game_current_player_c(g);

    if (player == NULL) {
        return;
    }

    (void)game_print(
        "玩家 %c 掷出 %d 点，移动至位置 %d。\n",
        player->id,
        steps,
        player->position
    );
}

static int move_current_player(Game *g, int32_t steps, int announce_roll)
{
    int8_t last_position;
    int rc;

    if (g == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count) {
        return RC_INVALID_PARAMS;
    }

    last_position = g->players[g->current_index].position;
    rc = game_move_to(g, steps, last_position);
    if (rc != RC_OK) {
        return rc;
    }
    if (announce_roll) {
        print_roll_result(g, steps);
    }
    return finish_move(g);
}

int game_roll(Game *g)
{
    int32_t steps;
    int rc;

    if (g == NULL) {
        game_set_error("无法掷骰：游戏状态异常。");
        return RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        game_set_error("游戏已结束，无法再掷骰。");
        return RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_COMMAND) {
        game_set_error("现在不能掷骰，请先完成当前提示或退出商店。");
        return RC_INVALID_PHASE;
    }

    /* 规范 7：每次 ROLL 从 DICE 流消费一个值（1~6） */
    rc = random_next(g, RSTREAM_DICE, DICE_MIN, DICE_MAX, &steps);
    if (rc != RC_OK) {
        return rc;
    }

    return move_current_player(g, steps, 1);
}

int game_step(Game *g, int32_t steps)
{
    int32_t effective;

    if (g == NULL) {
        game_set_error("无法移动：游戏状态异常。");
        return RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        game_set_error("游戏已结束，无法再移动。");
        return RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_COMMAND) {
        game_set_error("现在不能移动，请先完成当前提示或退出商店。");
        return RC_INVALID_PHASE;
    }
    /* 规范 6：steps 必须为 1~2147483647 的整数；0/负数视为 INVALID_PARAMS */
    if (steps < 1 || steps > STEP_MAX) {
        game_set_error("步数须为 1~2147483647 的整数，例如：STEP 4");
        return RC_INVALID_PARAMS;
    }

    /* 规范 6：steps>70 时先对 70 取余，再按有效步数逐格移动。
       取余结果为 0 是合法 steps>70 产生的有效移动距离：不移动、
       不做落点处理，但按合法 STEP 正常结束回合。 */
    effective = (steps > MAP_SIZE) ? (steps % MAP_SIZE) : steps;

    if (effective == 0) {
        return game_finish_action_turn(g);
    }

    return move_current_player(g, effective, 0);
}
