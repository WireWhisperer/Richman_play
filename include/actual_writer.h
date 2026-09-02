/**
 * @file actual_writer.h
 * @brief Actual 实际结果导出 —— 规范 v2.0 第 12/13 节
 */
#ifndef RICH_ACTUAL_WRITER_H
#define RICH_ACTUAL_WRITER_H

#include "cJSON.h"
#include "game.h"

/** display_players 数组（规范 5/13） */
cJSON *display_players_build(const Game *g);

/** display_cells 数组：70 格基础符号与覆盖符号（规范 13） */
cJSON *display_cells_build(const Game *g);

/**
 * 构造完整 Actual JSON：
 * { schema_version, case_id, actual: { users, current_user, phase,
 *   pending_prompt, game_status, winner, turn_number, players, properties,
 *   map_items, fortune, display_players, display_cells } }
 */
cJSON *actual_build(const Game *g, const char *case_id, const char *schema_version);

/** 序列化为单行 JSON 字符串（调用者 free） */
char *actual_serialize(const Game *g, const char *case_id, const char *schema_version);

/** 导出 Actual 到文件 */
int actual_write_file(const Game *g, const char *case_id, const char *schema_version,
                      const char *path);

#endif /* RICH_ACTUAL_WRITER_H */
