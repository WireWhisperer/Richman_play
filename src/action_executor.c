/**
 * @file action_executor.c
 * @brief Action 校验与执行（规范 v1.1 第 8 节、第 4 节回合边界）
 */
#include "action_executor.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
//#include <strings.h>

#include "file_utils.h"

/* Windows/MinGW 下 strings.h 可能缺失 strcasecmp，提供等价物（规则 1：只用 C11 标准函数） */
#ifndef strcasecmp
#include <ctype.h>
static int strcasecmp_portable(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
#define strcasecmp strcasecmp_portable
#endif

static void set_result(ActionResult *out, int code, const char *fmt, ...);

bool action_allowed_in_phase(GamePhase phase, const char *command)
{
    if (command == NULL) return false;
    switch (phase) {
    case PHASE_COMMAND:
        /* 规范 4.1：等待正式命令 */
        return strcasecmp(command, "ROLL") == 0 || strcasecmp(command, "STEP") == 0 ||
               strcasecmp(command, "SELL") == 0 || strcasecmp(command, "BLOCK") == 0 ||
               strcasecmp(command, "BOMB") == 0 || strcasecmp(command, "ROBOT") == 0 ||
               strcasecmp(command, "QUERY") == 0 || strcasecmp(command, "HELP") == 0 ||
               strcasecmp(command, "QUIT") == 0;
    case PHASE_PROMPT:
        /* 规范 4.1：等待购买、升级、道具屋或礼品屋回答 */
        return strcasecmp(command, "ANSWER") == 0 || strcasecmp(command, "QUERY") == 0 ||
               strcasecmp(command, "HELP") == 0 || strcasecmp(command, "QUIT") == 0;
    case PHASE_ENDED:
    default:
        return false;   /* 游戏结束后不允许后续 Action */
    }
}

int action_validate(const Game *g, const cJSON *action, ActionResult *out)
{
    if (!cJSON_IsObject(action)) {
        set_result(out, RC_INVALID_COMMAND, "Action 必须为对象");
        return RC_INVALID_COMMAND;
    }
    const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(action, "command");
    if (!cJSON_IsString(cmd)) {
        set_result(out, RC_INVALID_COMMAND, "Action 缺少 command 字符串");
        return RC_INVALID_COMMAND;
    }
    const char *c = cmd->valuestring;

    static const char *const COMMANDS[] = {
        "ROLL", "STEP", "SELL", "BLOCK", "BOMB", "ROBOT", "QUERY", "HELP", "ANSWER", "QUIT"
    };
    bool known = false;
    for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
        if (strcasecmp(c, COMMANDS[i]) == 0) { known = true; break; }
    }
    if (!known) {
        set_result(out, RC_INVALID_COMMAND, "不支持的 command: %s", c);
        return RC_INVALID_COMMAND;
    }

    /* 阶段校验（规范 4.1 / 13 ACTION_AFTER_END） */
    if (g->phase == PHASE_ENDED) {
        set_result(out, RC_ACTION_AFTER_END, "游戏结束后仍有 Action: %s", c);
        return RC_ACTION_AFTER_END;
    }
    if (!action_allowed_in_phase(g->phase, c)) {
        set_result(out, RC_INVALID_PHASE, "阶段 %s 不允许执行 %s", phase_to_str(g->phase), c);
        return RC_INVALID_PHASE;
    }

    /* 参数校验（规范 8.1~8.5 约束；所有整数走 int32 契约校验） */
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(action, "params");
    if (strcasecmp(c, "STEP") == 0) {
        int32_t steps;
        if (!cJSON_IsObject(params) ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "steps"), &steps) ||
            steps <= 0) {
            set_result(out, RC_INVALID_PARAMS, "STEP 的 params.steps 必须为正整数");
            return RC_INVALID_PARAMS;
        }
    } else if (strcasecmp(c, "SELL") == 0) {
        int32_t pos;
        if (!cJSON_IsObject(params) ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "position"), &pos) ||
            pos < 0 || pos >= MAP_SIZE) {
            set_result(out, RC_INVALID_PARAMS, "SELL 的 params.position 必须为 0~%d", MAP_SIZE - 1);
            return RC_INVALID_PARAMS;
        }
    } else if (strcasecmp(c, "BLOCK") == 0 || strcasecmp(c, "BOMB") == 0) {
        int32_t off;
        if (!cJSON_IsObject(params) ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "offset"), &off) ||
            off < -BLOCK_OFFSET_LIMIT || off > BLOCK_OFFSET_LIMIT) {
            set_result(out, RC_INVALID_PARAMS, "%s 的 params.offset 必须为 -10~10", c);
            return RC_INVALID_PARAMS;
        }
    } else if (strcasecmp(c, "ANSWER") == 0) {
        if (!cJSON_IsObject(params) ||
            !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(params, "value"))) {
            set_result(out, RC_INVALID_PARAMS, "ANSWER 的 params.value 必须为字符串");
            return RC_INVALID_PARAMS;
        }
    }
    return RC_OK;
}

int action_execute(Game *g, const cJSON *action, ActionResult *out)
{
    int rc = action_validate(g, action, out);
    if (rc != RC_OK) {
        return rc;
    }
    const char *c = cJSON_GetObjectItemCaseSensitive(action, "command")->valuestring;
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(action, "params");

    /* 命令字符串不区分大小写（规范 2.1） */
    if (strcasecmp(c, "ROLL") == 0) {
        rc = game_roll(g);
    } else if (strcasecmp(c, "STEP") == 0) {
        int32_t steps;
        fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "steps"), &steps);
        rc = game_step(g, steps);
    } else if (strcasecmp(c, "SELL") == 0) {
        int32_t pos;
        fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "position"), &pos);
        rc = game_sell(g, pos);
    } else if (strcasecmp(c, "BLOCK") == 0) {
        int32_t off;
        fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "offset"), &off);
        rc = game_block(g, off);
    } else if (strcasecmp(c, "BOMB") == 0) {
        int32_t off;
        fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "offset"), &off);
        rc = game_bomb(g, off);
    } else if (strcasecmp(c, "ROBOT") == 0) {
        rc = game_robot(g);
    } else if (strcasecmp(c, "QUERY") == 0) {
        char buf[4096];
        rc = game_query(g, buf, sizeof(buf));
        if (rc == RC_OK) {
            printf("%s", buf);   /* QUERY 文本输出，不参与状态比较（规范 2.5） */
        }
    } else if (strcasecmp(c, "HELP") == 0) {
        char buf[1024];
        rc = game_help(buf, sizeof(buf));
        if (rc == RC_OK) {
            printf("%s", buf);
        }
    } else if (strcasecmp(c, "ANSWER") == 0) {
        rc = game_answer(g, cJSON_GetObjectItemCaseSensitive(params, "value")->valuestring);
    } else if (strcasecmp(c, "QUIT") == 0) {
        rc = game_quit(g);
    } else {
        /* action_validate 已拦截未知命令，此处防御 */
        set_result(out, RC_INVALID_COMMAND, "不支持的 command: %s", c);
        return RC_INVALID_COMMAND;
    }

    if (rc < 0) {
        rc = -rc;
    }
    if (rc != RC_OK) {
        set_result(out, rc, "%s 执行失败: %s", c, game_last_error());
    }
    return rc;
}

int action_execute_all(Game *g, const cJSON *actions, ActionResult *out)
{
    if (!cJSON_IsArray(actions)) {
        set_result(out, RC_INVALID_COMMAND, "actions 必须为数组");
        return RC_INVALID_COMMAND;
    }
    for (int i = 0; i < cJSON_GetArraySize(actions); i++) {
        const cJSON *a = cJSON_GetArrayItem(actions, i);
        /* 游戏结束后仍有 Action（规范 13 ACTION_AFTER_END） */
        if (g->phase == PHASE_ENDED) {
            set_result(out, RC_ACTION_AFTER_END, "游戏结束后仍有第 %d 个 Action", i + 1);
            return RC_ACTION_AFTER_END;
        }
        int rc = action_execute(g, a, out);
        if (rc != RC_OK) {
            char tmp[sizeof(out->message)];
            snprintf(tmp, sizeof(tmp), "第 %d 个 Action: %s", i + 1, out->message);
            snprintf(out->message, sizeof(out->message), "%s", tmp);
            return rc;
        }
    }
    return RC_OK;
}

static void set_result(ActionResult *out, int code, const char *fmt, ...)
{
    out->code = code;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out->message, sizeof(out->message), fmt, ap);
    va_end(ap);
}
