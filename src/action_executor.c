/**
 * @file action_executor.c
 * @brief Action 分发执行实现（规范 8）。命令匹配忽略大小写（约束 C04）。
 */

#include "action_executor.h"

#include <string.h>

#include "cJSON.h"

/** 忽略大小写比较 */
static int streq_ci(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'a' && ca <= 'z') {
            ca = (char)(ca - 'a' + 'A');
        }
        if (cb >= 'a' && cb <= 'z') {
            cb = (char)(cb - 'a' + 'A');
        }
        if (ca != cb) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

static int param_int(const cJSON *params, const char *key, int32_t *out)
{
    cJSON *item;

    if (params == NULL || !cJSON_IsObject(params)) {
        return 0;
    }
    item = cJSON_GetObjectItem(params, key);
    if (item == NULL || !cJSON_IsNumber(item)) {
        return 0;
    }
    *out = (int32_t)item->valuedouble;
    return 1;
}

int action_executor_run(Game *g, const cJSON *action)
{
    cJSON *command;
    cJSON *params;
    const char *cmd;

    if (g == NULL || action == NULL || !cJSON_IsObject(action)) {
        return -RC_INVALID_COMMAND;
    }
    if (g->phase == PHASE_ENDED) {
        return -RC_ACTION_AFTER_END;
    }

    command = cJSON_GetObjectItem(action, "command");
    if (command == NULL || !cJSON_IsString(command)) {
        return -RC_INVALID_COMMAND;
    }
    cmd = command->valuestring;
    params = cJSON_GetObjectItem(action, "params");

    if (streq_ci(cmd, "ROLL")) {
        return game_roll(g);
    }
    if (streq_ci(cmd, "STEP")) {
        int32_t steps;
        if (!param_int(params, "steps", &steps)) {
            return -RC_INVALID_PARAMS;
        }
        return game_step(g, steps);
    }
    if (streq_ci(cmd, "SELL")) {
        int32_t position;
        if (!param_int(params, "position", &position)) {
            return -RC_INVALID_PARAMS;
        }
        return game_sell(g, position);
    }
    if (streq_ci(cmd, "BLOCK")) {
        int32_t offset;
        if (!param_int(params, "offset", &offset)) {
            return -RC_INVALID_PARAMS;
        }
        return game_block(g, offset);
    }
    if (streq_ci(cmd, "BOMB")) {
        int32_t offset;
        if (!param_int(params, "offset", &offset)) {
            return -RC_INVALID_PARAMS;
        }
        return game_bomb(g, offset);
    }
    if (streq_ci(cmd, "ROBOT")) {
        return game_robot(g);
    }
    if (streq_ci(cmd, "QUERY")) {
        char buf[512];
        return game_query(g, buf, sizeof(buf));
    }
    if (streq_ci(cmd, "HELP")) {
        char buf[512];
        return game_help(buf, sizeof(buf));
    }
    if (streq_ci(cmd, "ANSWER")) {
        cJSON *value;
        if (params == NULL || !cJSON_IsObject(params)) {
            return -RC_INVALID_PARAMS;
        }
        value = cJSON_GetObjectItem(params, "value");
        return game_answer(g, (value != NULL && cJSON_IsString(value)) ? value->valuestring : "");
    }
    if (streq_ci(cmd, "QUIT")) {
        return game_quit(g);
    }

    return -RC_INVALID_COMMAND;
}
