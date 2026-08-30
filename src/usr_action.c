#include "game.h"

#include <stdio.h>
#include <stdlib.h>

static int skip_confined_turn(Game *g)
{
    PLAYER *player;

    if (g == NULL) {
        return 0;
    }

    player = game_current_player(g);
    if (player == NULL) {
        return 0;
    }

    if (player->status != HOSPITAL && player->status != IMPRISONED) {
        return 0;
    }

    if (player->status == HOSPITAL) {
        (void)game_print("玩家 %c 正在住院，本回合无法行动。\n", player->id);
    } else {
        (void)game_print("玩家 %c 正在监狱中，本回合无法行动。\n", player->id);
    }

    player->remaining_rounds--;
    if (player->remaining_rounds <= 0) {
        player->status = NORMAL;
        player->remaining_rounds = 0;
        (void)game_print("玩家 %c 已恢复自由，下次轮到时可以行动。\n", player->id);
    }

    game_finish_action_turn(g);
    return 1;
}

static int finish_move(Game *g)
{
    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }

    game_settle_landing(g);
    if (g->phase != PHASE_PROMPT) {
        game_finish_action_turn(g);
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
    (void)game_move_to(g, steps, last_position);
    if (announce_roll) {
        print_roll_result(g, steps);
    }
    rc = finish_move(g);
    return rc;
}

int game_roll(Game *g)
{
    int32_t steps;

    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        return RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_COMMAND) {
        return RC_INVALID_PHASE;
    }

    if (skip_confined_turn(g)) {
        return RC_OK;
    }

    if (g->dice_next < g->dice_count) {
        steps = g->dice_seq[g->dice_next++];
    } else if (g->dice_preset_loaded) {
        return RC_DICE_SEQUENCE_EMPTY;
    } else {
        steps = (int32_t)(rand() % DICE_MAX) + DICE_MIN;
    }

    return move_current_player(g, steps, 1);
}

int game_step(Game *g, int32_t steps)
{
    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        return RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_COMMAND) {
        return RC_INVALID_PHASE;
    }
    if (steps < DICE_MIN || steps > MAP_SIZE) {
        return RC_INVALID_PARAMS;
    }

    if (skip_confined_turn(g)) {
        return RC_OK;
    }

    return move_current_player(g, steps, 0);
}
