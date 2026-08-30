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

/**
 * 交互式手动对局：
 *   输入 ROLL/STEP n/SELL n/BLOCK n/BOMB n/ROBOT/QUERY/HELP/QUIT，
 *   ANSWER 阶段输入 Y/N/1/2/3/F。命令不区分大小写。
 * @param g 已加载地图并初始化的游戏
 * @return 0 正常退出
 */
int manual_ui_run(Game *g);

/**
 * 提示输入初始资金：空行使用默认值，越界则重新输入；输入 QUIT 返回 1。
 * 本函数不修改游戏状态，资金由调用方在玩家选择完成后应用。
 * @param g             游戏对象（仅用于 QUIT 时结束状态）
 * @param initial_fund  输出选定的初始资金
 * @return 0 已选定；1 用户输入 QUIT
 */
int manual_ui_prompt_initial_fund(Game *g, int32_t *initial_fund);

#endif /* RICH_MANUAL_UI_H */
