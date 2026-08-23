/**
 * @file action_executor.h
 * @brief Action 校验与执行 —— 规范 v1.1 第 8 节、第 4 节回合边界
 *
 * 回合边界（规范 4）：
 *   SELL/BLOCK/BOMB/ROBOT/QUERY/HELP 不结束回合；
 *   ROLL/STEP 完成整个落点流程后才结束回合；
 *   ANSWER 处理完提示后结束回合并切换玩家。
 */
#ifndef RICH_ACTION_EXECUTOR_H
#define RICH_ACTION_EXECUTOR_H

#include "cJSON.h"
#include "game.h"

typedef struct {
    int  code;               /* 0 = 成功；否则为 ResultCode（规范 13 错误码） */
    char message[256];       /* 人类可读的错误描述 */
} ActionResult;

/** 按 phase 判断该 command 是否被允许（规范 4.1 允许的 Action 表） */
bool action_allowed_in_phase(GamePhase phase, const char *command);

/**
 * 仅校验单个 Action 的格式与参数约束（规范 8.1~8.5），不修改游戏状态。
 * @return 0 通过；RC_INVALID_COMMAND / RC_INVALID_PARAMS / RC_INVALID_PHASE
 */
int action_validate(const Game *g, const cJSON *action, ActionResult *out);

/**
 * 校验并执行单个 Action（规范 8）。
 * 命令字符串不区分大小写（规范 2.1）。
 * @return 0 成功；否则对应 ResultCode
 */
int action_execute(Game *g, const cJSON *action, ActionResult *out);

/**
 * 按顺序执行全部 Actions（规范 14 步骤 5）。
 * 任一 Action 失败即停止，返回该错误码（游戏结束后再有 Action 报
 * RC_ACTION_AFTER_END，规范 13）。
 */
int action_execute_all(Game *g, const cJSON *actions, ActionResult *out);

#endif /* RICH_ACTION_EXECUTOR_H */
