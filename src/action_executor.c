/**
 * @file action_executor.c
 * @brief Action 校验与执行（规范 v2.0 第 6/9 节、第 4 节回合边界）
 *
 * v2.0 变更：
 *   - 删除 BOMB 命令（INVALID_COMMAND）；新增 STATE 测试控制 Action ADVANCE_TURN；
 *   - STEP 的 steps 必须为 1~2147483647 的整数（0/负数/非整数 → INVALID_PARAMS）；
 *   - 输入 command 按 ASCII 不区分大小写（解析后统一大写语义）。
 */
#include "action_executor.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifndef _MSC_VER
#include <strings.h>
#endif

#include "file_utils.h"

/* Windows/MSVC 下 strings.h 缺失 strcasecmp，提供等价物 */
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
static void set_param_error(ActionResult *out, int code, const char *command,
                            const char *fmt, ...);

bool action_allowed_in_phase(GamePhase phase, const char *command)
{
    if (command == NULL) return false;
    switch (phase) {
    case PHASE_COMMAND:
        /* 规范 6：等待正式命令（STATE 模式额外允许 ADVANCE_TURN） */
        return strcasecmp(command, "ROLL") == 0 || strcasecmp(command, "STEP") == 0 ||
               strcasecmp(command, "SELL") == 0 || strcasecmp(command, "BLOCK") == 0 ||
               strcasecmp(command, "ROBOT") == 0 || strcasecmp(command, "QUERY") == 0 ||
               strcasecmp(command, "HELP") == 0 || strcasecmp(command, "QUIT") == 0 ||
               strcasecmp(command, "ADVANCE_TURN") == 0;
    case PHASE_PROMPT:
        /* 规范 6：等待购买、升级、道具屋或礼品屋回答 */
        return strcasecmp(command, "ANSWER") == 0 || strcasecmp(command, "QUERY") == 0 ||
               strcasecmp(command, "HELP") == 0 || strcasecmp(command, "QUIT") == 0;
    case PHASE_ENDED:
    default:
        return false;   /* 游戏结束后不允许后续 Action */
    }
}

/** command 对应的参数错误路径后缀（规范 17 expected_error.path） */
static const char *param_path_suffix(const char *command)
{
    if (strcasecmp(command, "STEP") == 0)  return "params.steps";
    if (strcasecmp(command, "SELL") == 0)  return "params.position";
    if (strcasecmp(command, "BLOCK") == 0) return "params.offset";
    if (strcasecmp(command, "ANSWER") == 0) return "params.value";
    return NULL;
}

int action_validate(const Game *g, const cJSON *action, ActionResult *out)
{
    out->error_path[0] = '\0';
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
        "ROLL", "STEP", "SELL", "BLOCK", "ROBOT", "QUERY", "HELP",
        "ANSWER", "QUIT", "ADVANCE_TURN"
    };
    bool known = false;
    for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
        if (strcasecmp(c, COMMANDS[i]) == 0) { known = true; break; }
    }
    if (!known) {
        set_result(out, RC_INVALID_COMMAND, "不支持的 command: %s", c);
        return RC_INVALID_COMMAND;
    }

    /* 阶段校验（规范 6 / 18 ACTION_AFTER_END） */
    if (g->phase == PHASE_ENDED) {
        set_result(out, RC_ACTION_AFTER_END, "游戏结束后仍有 Action: %s", c);
        return RC_ACTION_AFTER_END;
    }
    if (!action_allowed_in_phase(g->phase, c)) {
        set_result(out, RC_INVALID_PHASE, "阶段 %s 不允许执行 %s", phase_to_str(g->phase), c);
        return RC_INVALID_PHASE;
    }

    /* 参数校验（规范 9；所有整数走 int32 契约校验） */
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(action, "params");
    if (strcasecmp(c, "STEP") == 0) {
        int32_t steps;
        if (!cJSON_IsObject(params) ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "steps"), &steps) ||
            steps < 1 || steps > STEP_MAX) {
            set_param_error(out, RC_INVALID_PARAMS, c,
                            "STEP 的 params.steps 必须为 1~%d 的整数", STEP_MAX);
            return RC_INVALID_PARAMS;
        }
    } else if (strcasecmp(c, "SELL") == 0) {
        int32_t pos;
        if (!cJSON_IsObject(params) ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "position"), &pos) ||
            pos < 0 || pos >= MAP_SIZE) {
            set_param_error(out, RC_INVALID_PARAMS, c,
                            "SELL 的 params.position 必须为 0~%d", MAP_SIZE - 1);
            return RC_INVALID_PARAMS;
        }
    } else if (strcasecmp(c, "BLOCK") == 0) {
        int32_t off;
        if (!cJSON_IsObject(params) ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(params, "offset"), &off) ||
            off < -BLOCK_OFFSET_LIMIT || off > BLOCK_OFFSET_LIMIT) {
            set_param_error(out, RC_INVALID_PARAMS, c,
                            "BLOCK 的 params.offset 必须为 -10~10");
            return RC_INVALID_PARAMS;
        }
    } else if (strcasecmp(c, "ANSWER") == 0) {
        if (!cJSON_IsObject(params) ||
            !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(params, "value"))) {
            set_param_error(out, RC_INVALID_PARAMS, c,
                            "ANSWER 的 params.value 必须为字符串");
            return RC_INVALID_PARAMS;
        }
    } else if (strcasecmp(c, "ADVANCE_TURN") == 0 || strcasecmp(c, "ROLL") == 0 ||
               strcasecmp(c, "ROBOT") == 0 || strcasecmp(c, "QUERY") == 0 ||
               strcasecmp(c, "HELP") == 0 || strcasecmp(c, "QUIT") == 0) {
        /* 这些命令不接受参数 */
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

    /* 命令字符串不区分大小写（规范 2.2） */
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
    } else if (strcasecmp(c, "ROBOT") == 0) {
        rc = game_robot(g);
    } else if (strcasecmp(c, "QUERY") == 0) {
        char buf[4096];
        rc = game_query(g, buf, sizeof(buf));
        if (rc == RC_OK) {
            game_print("%s", buf);   /* QUERY 文本输出，不参与状态比较 */
        }
    } else if (strcasecmp(c, "HELP") == 0) {
        char buf[1024];
        rc = game_help(buf, sizeof(buf));
        if (rc == RC_OK) {
            game_print("%s", buf);
        }
    } else if (strcasecmp(c, "ANSWER") == 0) {
        rc = game_answer(g, cJSON_GetObjectItemCaseSensitive(params, "value")->valuestring);
        if (rc != RC_OK) {
            /* 非法回答（如购买/升级提示输入 x）定位到 params.value（规范 17） */
            snprintf(out->error_path, sizeof(out->error_path), "params.value");
        }
    } else if (strcasecmp(c, "ADVANCE_TURN") == 0) {
        rc = game_advance_turn(g);
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
        out->action_index = i;
        /* 游戏结束后仍有 Action（规范 18 ACTION_AFTER_END） */
        if (g->phase == PHASE_ENDED) {
            out->error_path[0] = '\0';
            set_result(out, RC_ACTION_AFTER_END, "游戏结束后仍有第 %d 个 Action", i + 1);
            return RC_ACTION_AFTER_END;
        }
        int rc = action_execute(g, a, out);
        if (rc != RC_OK) {
            char tmp[sizeof(out->message)];
            snprintf(tmp, sizeof(tmp), "第 %d 个 Action: %s", i + 1, out->message);
            snprintf(out->message, sizeof(out->message), "%s", tmp);

            /* 组合错误路径（规范 17）：actions[i].params.<字段> / actions[i].command */
            if (out->error_path[0] == '\0') {
                const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(a, "command");
                const char *cname = cJSON_IsString(cmd) ? cmd->valuestring : "";
                if (rc == RC_INVALID_COMMAND || cname[0] == '\0') {
                    snprintf(out->error_path, sizeof(out->error_path),
                             "actions[%d].command", i);
                } else {
                    snprintf(out->error_path, sizeof(out->error_path),
                             "actions[%d]", i);
                }
            } else {
                char composed[128];
                snprintf(composed, sizeof(composed), "actions[%d].%s", i, out->error_path);
                snprintf(out->error_path, sizeof(out->error_path), "%s", composed);
            }
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

/** 参数错误时记录字段后缀，action_execute_all 组合为 actions[i].params.<字段> */
static void set_param_error(ActionResult *out, int code, const char *command,
                            const char *fmt, ...)
{
    const char *suffix = param_path_suffix(command);

    out->code = code;
    if (suffix != NULL) {
        snprintf(out->error_path, sizeof(out->error_path), "%s", suffix);
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out->message, sizeof(out->message), fmt, ap);
    va_end(ap);
}
