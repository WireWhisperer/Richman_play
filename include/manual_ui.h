/**
 * @file manual_ui.h
 * @brief 手动交互对局模式 —— 规范 v1.1 第 1 节
 *
 * 规范不限制界面形式；本模块提供终端命令行交互式对局，
 * 供人工体验与调试，不参与自动化测试状态比较（规范 2.6）。
 */
#ifndef RICH_MANUAL_UI_H
#define RICH_MANUAL_UI_H

#include "cJSON.h"
#include "game.h"

/** Formats the current turn prompt, for example "钱夫人> ". */
int manual_ui_format_turn_prompt(
    const Game *g,
    char *buffer,
    size_t buffer_size
);

/**
 * 交互式手动对局：
 *   输入 ROLL/STEP n/SELL n/BLOCK n/BOMB n/ROBOT/QUERY/HELP/QUIT，
 *   ANSWER 阶段输入 Y/N/1/2/3/F。命令不区分大小写。
 * @param g 已加载地图、完成资金和角色设置的游戏
 * @return 0 正常退出
 */
int manual_ui_run(Game *g);

#endif /* RICH_MANUAL_UI_H */
