#ifndef ACTUAL_WRITER_H
#define ACTUAL_WRITER_H

#include "game.h"

struct cJSON;

/**
 * 从游戏状态构建 Actual 对象（规范 10，不含 schema_version/case_id）。
 * 返回的 cJSON 由调用者负责 cJSON_Delete。
 */
struct cJSON *actual_writer_build(const Game *g);

#endif /* ACTUAL_WRITER_H */
