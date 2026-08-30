/**
 * @file actual_writer.h
 * @brief Actual 实际结果导出 —— 规范 v1.1 第 10 节
 *
 * 游戏执行完全部 Actions 后输出完整状态 JSON：
 *   { schema_version, case_id, actual: { users, current_user, phase,
 *     pending_prompt, game_status, winner, players, properties,
 *     map_items, display_players } }
 *
 * 稳定输出顺序（规范 10.1）：
 *   players 按 users 数组顺序；
 *   properties / map_items / display_players 按 position 从小到大；
 *   JSON 对象字段顺序不参与比较，但序列化时保持固定以便人工比对。
 */
#ifndef RICH_ACTUAL_WRITER_H
#define RICH_ACTUAL_WRITER_H

#include "cJSON.h"
#include "game.h"

/** 构建 Actual 完整状态 JSON（规范 10 结构），返回新 cJSON 对象（调用者 cJSON_Delete） */
cJSON *actual_build(const Game *g, const char *case_id);

/**
 * 序列化为 UTF-8 JSON 字符串（ensure_ascii 行为等价：不转义中文）。
 * 返回 malloc 字符串，调用者 free。
 */
char *actual_serialize(const Game *g, const char *case_id);

/** 导出 Actual 到文件，0 成功 */
int actual_write_file(const Game *g, const char *case_id, const char *path);

/**
 * 计算 display_players（规范 5.1 可选标准显示状态）：
 *   每个被至少一名未破产玩家占用的位置输出一条，按 position 升序；
 *   当前玩家在该位置则显示当前玩家，否则显示 users 中顺序最靠前的玩家。
 * 返回新 cJSON 数组（调用者 cJSON_Delete）。
 */
cJSON *display_players_build(const Game *g);

#endif /* RICH_ACTUAL_WRITER_H */
