#ifndef ACTION_EXECUTOR_H
#define ACTION_EXECUTOR_H

#include "game.h"

struct cJSON;

/**
 * 执行一个 Action 对象 {command, params}。
 * 返回 0 表示成功，负数表示 ResultCode（如 -RC_INVALID_COMMAND）。
 */
int action_executor_run(Game *g, const struct cJSON *action);

#endif /* ACTION_EXECUTOR_H */
