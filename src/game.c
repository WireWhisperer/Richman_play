/**
 * @file game.c
 * @brief 大富翁游戏核心状态与规则（规范 v2.0 第 3/4/5/6/7/14/15/18 节）
 *
 * 实现：枚举映射、生命周期、地图加载、地产经济、查询、回合切换、
 * 确定性随机流、地图财神（生成/保留/领取/再生成）与当轮免租。
 * 移动/落点/Action 业务逻辑见 usr_action.c / usr_judge.c / game_property.c。
 */
#include "cJSON.h"
#include "game.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_utils.h"

/* ==================== 错误码与最近错误 ==================== */

static char g_last_error[256] = "";

/* 公开：供 UI/道具屋等模块写入玩家可读的错误提示（远端文案优化保留） */
void game_set_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, ap);
    va_end(ap);
}

#define set_error game_set_error

const char *game_last_error(void)
{
    return g_last_error;
}

/* 游戏内部统一输出入口：自动化测试模式下静默，保证控制台只有 PASS/FAIL 结果行 */
bool g_game_quiet = false;

void game_print(const char *fmt, ...)
{
    va_list ap;

    if (g_game_quiet || fmt == NULL) {
        return;
    }
    va_start(ap, fmt);
    (void)vprintf(fmt, ap);
    va_end(ap);
}

const char *result_code_name(ResultCode rc)
{
    switch (rc) {
    case RC_OK:                      return "OK";
    case RC_INVALID_JSON:            return "INVALID_JSON";
    case RC_UNSUPPORTED_VERSION:     return "UNSUPPORTED_VERSION";
    case RC_UNSUPPORTED_MODE:        return "UNSUPPORTED_MODE";
    case RC_INVALID_PRESET:          return "INVALID_PRESET";
    case RC_INVALID_MAP:             return "INVALID_MAP";
    case RC_INVALID_COMMAND:         return "INVALID_COMMAND";
    case RC_INVALID_PARAMS:          return "INVALID_PARAMS";
    case RC_INVALID_PHASE:           return "INVALID_PHASE";
    case RC_RANDOM_SEQUENCE_EMPTY:   return "RANDOM_SEQUENCE_EMPTY";
    case RC_RANDOM_VALUE_OUT_OF_RANGE: return "RANDOM_VALUE_OUT_OF_RANGE";
    case RC_ACTION_AFTER_END:        return "ACTION_AFTER_END";
    case RC_ASSERT_NOT_EQUAL:        return "ASSERT_NOT_EQUAL";
    case RC_ASSERT_NOT_FOUND:        return "ASSERT_NOT_FOUND";
    case RC_ASSERT_NOT_ABSENT:       return "ASSERT_NOT_ABSENT";
    case RC_IO_ERROR:                return "IO_ERROR";
    case RC_INTERNAL:                return "INTERNAL";
    }
    return "UNKNOWN";
}

/* ==================== 枚举 <-> JSON 字符串固定映射 ==================== */

static const char *const CELL_TYPE_NAMES[CELL_KIND_COUNT] = {
    "START", "LAND_1", "LAND_2", "LAND_3", "TOOL_SHOP", "GIFT_SHOP",
    "PARK", "MINE"
};
/* 与 PLAYER_STATUS 枚举顺序一致（规范 5.4）：NORMAL, BANKRUPT。
   HOSPITAL/JAIL 已在 v2.0 删除：player_status_from_str 对它们返回 -1（INVALID_PRESET）。 */
static const char *const PLAYER_STATUS_NAMES[] = {
    "NORMAL", "BANKRUPT"
};
static const char *const ITEM_KIND_NAMES[ITEM_KIND_COUNT] = {
    "BLOCK", "BOMB", "ROBOT"
};
static const char *const PHASE_NAMES[] = { "COMMAND", "PROMPT", "ENDED" };
static const char *const GAME_STATUS_NAMES[] = { "RUNNING", "FINISHED" };
static const char *const PROMPT_NAMES[] = {
    "NONE", "BUY", "UPGRADE", "TOOL_SHOP", "GIFT_SHOP"
};
static const char *const RANDOM_STREAM_NAMES[RSTREAM_COUNT] = {
    "DICE", "FORTUNE_POSITION", "FORTUNE_RESPAWN_DELAY", "GIFT"
};

const char *cell_type_to_str(CellType t)
{
    return (t >= 0 && t < CELL_KIND_COUNT) ? CELL_TYPE_NAMES[t] : "UNKNOWN";
}

int cell_type_from_str(const char *s)
{
    if (s == NULL) return -1;
    for (int i = 0; i < CELL_KIND_COUNT; i++) {
        if (strcmp(s, CELL_TYPE_NAMES[i]) == 0) return i;
    }
    return -1;
}

const char *player_status_to_str(PLAYER_STATUS s)
{
    return (s >= 0 && s < 2) ? PLAYER_STATUS_NAMES[s] : "UNKNOWN";
}

int player_status_from_str(const char *s)
{
    if (s == NULL) return -1;
    for (int i = 0; i < 2; i++) {
        if (strcmp(s, PLAYER_STATUS_NAMES[i]) == 0) return i;
    }
    return -1;
}

const char *item_kind_to_str(ItemKind k)
{
    return (k >= 0 && k < ITEM_KIND_COUNT) ? ITEM_KIND_NAMES[k] : "UNKNOWN";
}

int item_kind_from_str(const char *s)
{
    if (s == NULL) return -1;
    for (int i = 0; i < ITEM_KIND_COUNT; i++) {
        if (strcmp(s, ITEM_KIND_NAMES[i]) == 0) return i;
    }
    return -1;
}

const char *phase_to_str(GamePhase p)
{
    return (p >= 0 && p < 3) ? PHASE_NAMES[p] : "UNKNOWN";
}

const char *game_status_to_str(GameStatus s)
{
    return (s >= 0 && s < 2) ? GAME_STATUS_NAMES[s] : "UNKNOWN";
}

const char *prompt_to_str(PromptType p)
{
    return (p >= 0 && p < 5) ? PROMPT_NAMES[p] : "UNKNOWN";
}

const char *random_stream_name(RandomStreamKind k)
{
    return (k >= 0 && k < RSTREAM_COUNT) ? RANDOM_STREAM_NAMES[k] : "UNKNOWN";
}

static int random_stream_from_name(const char *s)
{
    if (s == NULL) return -1;
    for (int i = 0; i < RSTREAM_COUNT; i++) {
        if (strcmp(s, RANDOM_STREAM_NAMES[i]) == 0) return i;
    }
    return -1;
}

/* ==================== 确定性随机流（规范 7） ==================== */

static uint32_t xorshift32_next(uint32_t *x)
{
    uint32_t v = *x;
    v ^= (v << 13);
    v ^= (v >> 17);
    v ^= (v << 5);
    *x = v;
    return v;
}

/**
 * 从指定流取一个 [min,max] 内的值（规范 7）：
 *   未声明随机控制 -> 语言自带随机；
 *   SEQUENCE 耗尽 -> RC_RANDOM_SEQUENCE_EMPTY（1.0 骰子序列保留旧错误名）；
 *   值越界 -> RC_RANDOM_VALUE_OUT_OF_RANGE；
 *   PRNG -> XORSHIFT32，min + next % (max-min+1)。
 */
int random_next(Game *g, RandomStreamKind k, int32_t min, int32_t max, int32_t *out)
{
    RandomControl *rng;
    int32_t v;

    if (g == NULL || out == NULL || k < 0 || k >= RSTREAM_COUNT || max < min) {
        return -RC_INVALID_PARAMS;
    }
    rng = &g->rng;

    if (rng->mode == RANDOM_MODE_NONE) {
        *out = min + (int32_t)(rand() % (uint32_t)(max - min + 1));
        return RC_OK;
    }

    if (rng->mode == RANDOM_MODE_SEQUENCE) {
        if (rng->seq_next[k] >= rng->seq_count[k]) {
            set_error("%s 随机流已耗尽", random_stream_name(k));
            return -RC_RANDOM_SEQUENCE_EMPTY;
        }
        v = rng->seq[k][rng->seq_next[k]++];
    } else if (rng->mode == RANDOM_MODE_PRNG) {
        v = min + (int32_t)(xorshift32_next(&rng->prng_state[k]) %
                            (uint32_t)(max - min + 1));
    } else {
        set_error("未知随机控制模式");
        return -RC_INTERNAL;
    }

    if (v < min || v > max) {
        set_error("%s 随机流值 %d 越界（应为 %d~%d）",
                  random_stream_name(k), (int)v, (int)min, (int)max);
        return -RC_RANDOM_VALUE_OUT_OF_RANGE;
    }
    *out = v;
    return RC_OK;
}

/* ==================== 生命周期 ==================== */

void game_init(Game *g)
{
    int32_t index;

    if (g == NULL) {
        return;
    }

    memset(g, 0, sizeof(*g));

    for (index = 0; index < MAP_SIZE; ++index) {
        g->cells[index].type = CELL_START;
    }

    g->current_index = -1;
    g->turn_number = 1;
    g->fortune.position = -1;
    g->fortune.spawned_after_turn = 0;
    g->fortune.remaining_map_turns = 0;
    g->fortune.next_spawn_after_turn = 0;
    for (index = 0; index < MAX_PLAYERS; ++index) {
        g->players[index].id = '?';
        g->players[index].status = NORMAL;
    }

    for (index = 0; index < MAX_BOARD_ITEMS; ++index) {
        g->properties[index].position = -1;
        g->properties[index].owner_index = -1;
        g->board_items[index].position = -1;
        g->board_items[index].kind = ITEM_BLOCK;
    }

    g->phase = PHASE_COMMAND;
    g->status = GAME_RUNNING;
    g->prompt = PROMPT_NONE;
    g->winner_index = -1;
}

void game_reset(Game *g)
{
    /* 规范 19：执行每个测试前必须完整重置游戏。
       地图为游戏级配置，予以保留；其余全部清空。 */
    memset(g->players, 0, sizeof(g->players));
    g->user_count = 0;
    g->current_index = -1;
    g->phase = PHASE_COMMAND;
    g->status = GAME_RUNNING;
    g->prompt = PROMPT_NONE;
    memset(g->properties, 0, sizeof(g->properties));
    g->property_count = 0;
    memset(g->board_items, 0, sizeof(g->board_items));
    g->board_item_count = 0;
    memset(&g->rng, 0, sizeof(g->rng));
    g->turn_number = 1;
    g->fortune.position = -1;
    g->fortune.spawned_after_turn = 0;
    g->fortune.remaining_map_turns = 0;
    g->fortune.next_spawn_after_turn = 0;
    g->winner_index = -1;
    g->quit = false;
    g->god_acquired_this_turn = false;
}

int game_apply_initial_fund(Game *g, int32_t initial_fund)
{
    int32_t index;

    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (initial_fund < MANUAL_INITIAL_FUND_MIN ||
        initial_fund > MANUAL_INITIAL_FUND_MAX) {
        set_error(
            "初始资金必须在 %d~%d 之间",
            MANUAL_INITIAL_FUND_MIN,
            MANUAL_INITIAL_FUND_MAX
        );
        return RC_INVALID_PARAMS;
    }
    if (g->user_count < 1 || g->user_count > MAX_PLAYERS) {
        set_error("请先完成玩家设置");
        return RC_INVALID_PRESET;
    }

    for (index = 0; index < g->user_count; ++index) {
        PLAYER *player = &g->players[index];

        player->fund = initial_fund;
        player->credit = 0;
        player->position = 0;
        player->status = NORMAL;
        player->god_of_wealth_rounds = 0;
    }

    g->current_index = 0;
    g->phase = PHASE_COMMAND;
    g->status = GAME_RUNNING;
    g->prompt = PROMPT_NONE;
    g->turn_number = 1;
    /* 规范 14.1：完成第 10 个玩家回合后首次生成财神 */
    g->fortune.next_spawn_after_turn = FORTUNE_FIRST_SPAWN_TURN;
    return RC_OK;
}

int game_start_manual(Game *g, int32_t initial_fund)
{
    static const char default_ids[] = { 'Q', 'A', 'S', 'J' };
    int32_t index;

    if (initial_fund < MANUAL_INITIAL_FUND_MIN ||
        initial_fund > MANUAL_INITIAL_FUND_MAX) {
        set_error(
            "初始资金必须在 %d~%d 之间",
            MANUAL_INITIAL_FUND_MIN,
            MANUAL_INITIAL_FUND_MAX
        );
        return RC_INVALID_PARAMS;
    }

    game_reset(g);

    g->user_count = MAX_PLAYERS;
    for (index = 0; index < g->user_count; ++index) {
        g->players[index].id = default_ids[index];
    }

    return game_apply_initial_fund(g, initial_fund);
}

int game_load_map(Game *g, const char *map_file)
{
    /* map.json 结构（项目统一约定）：
       { "name": "...", "cells": [ { "position": 0, "type": "START",
         "price": 0, "upgrade_cost": 0, "mine_points": 0 }, ... ] }
       规范 5.2：地图共有 70 个位置，合法编号 0~69；类型由 map.json 唯一确定。
       v2.0：MAGIC_HOUSE/HOSPITAL/JAIL 为已删除类型（INVALID_MAP）；
       位置 14/49/63 必须为 PARK，其余位置不得为 PARK。 */
    char *text = fu_read_file(map_file, NULL);
    if (text == NULL) {
        set_error("无法读取地图文件: %s", map_file);
        return RC_INVALID_MAP;
    }
    cJSON *root = NULL;
    if (fu_parse_json(text, &root) != RC_OK || !cJSON_IsObject(root)) {
        free(text);
        set_error("地图文件不是合法 JSON 对象: %s", map_file);
        if (root) cJSON_Delete(root);
        return RC_INVALID_MAP;
    }
    free(text);

    cJSON *cells = cJSON_GetObjectItemCaseSensitive(root, "cells");
    if (!cJSON_IsArray(cells) || cJSON_GetArraySize(cells) != MAP_SIZE) {
        set_error("地图 cells 必须恰好包含 %d 个位置", MAP_SIZE);
        cJSON_Delete(root);
        return RC_INVALID_MAP;
    }

    memset(g->cells, 0, sizeof(g->cells));
    bool seen[MAP_SIZE];
    memset(seen, 0, sizeof(seen));

    cJSON *cell = NULL;
    cJSON_ArrayForEach(cell, cells) {
        if (!cJSON_IsObject(cell)) {
            set_error("地图 cell 必须是对象");
            cJSON_Delete(root);
            return RC_INVALID_MAP;
        }
        int32_t pos;
        const cJSON *pos_node = cJSON_GetObjectItemCaseSensitive(cell, "position");
        if (!fu_json_get_uint32(pos_node, &pos) || pos >= MAP_SIZE) {
            set_error("地图 cell.position 非法（需为 0~%d 的整数）", MAP_SIZE - 1);
            cJSON_Delete(root);
            return RC_INVALID_MAP;
        }
        if (seen[pos]) {
            set_error("地图 position 重复: %d", (int)pos);
            cJSON_Delete(root);
            return RC_INVALID_MAP;
        }
        seen[pos] = true;

        const cJSON *type_node = cJSON_GetObjectItemCaseSensitive(cell, "type");
        if (!cJSON_IsString(type_node) ||
            (g->cells[pos].type = (CellType)cell_type_from_str(type_node->valuestring)) < 0) {
            set_error("地图 position %d 的 type 非法", (int)pos);
            cJSON_Delete(root);
            return RC_INVALID_MAP;
        }
        /* 数值字段可选；缺省视为 0，再按地块类型补默认值 */
        const cJSON *p = cJSON_GetObjectItemCaseSensitive(cell, "price");
        const cJSON *u = cJSON_GetObjectItemCaseSensitive(cell, "upgrade_cost");
        const cJSON *m = cJSON_GetObjectItemCaseSensitive(cell, "mine_points");
        int32_t price = 0;
        int32_t upg = 0;
        int32_t mine = 0;

        if (p != NULL && !fu_json_get_uint32(p, &price)) {
            set_error("地图 position %d 的 price 非法（需为非负 int32）", (int)pos);
            cJSON_Delete(root);
            return RC_INVALID_MAP;
        }
        if (u != NULL && !fu_json_get_uint32(u, &upg)) {
            set_error("地图 position %d 的 upgrade_cost 非法（需为非负 int32）", (int)pos);
            cJSON_Delete(root);
            return RC_INVALID_MAP;
        }
        if (m != NULL && !fu_json_get_uint32(m, &mine)) {
            set_error("地图 position %d 的 mine_points 非法（需为非负 int32）", (int)pos);
            cJSON_Delete(root);
            return RC_INVALID_MAP;
        }
        switch (g->cells[pos].type) {
        case CELL_LAND_1: if (price == 0) price = 200; if (upg == 0) upg = 200; break;
        case CELL_LAND_2: if (price == 0) price = 500; if (upg == 0) upg = 500; break;
        case CELL_LAND_3: if (price == 0) price = 300; if (upg == 0) upg = 300; break;
        case CELL_MINE:   if (mine == 0) mine = 80; break;
        default: break;
        }
        g->cells[pos].price = price;
        g->cells[pos].upgrade_cost = upg;
        g->cells[pos].mine_points = mine;
    }

    cJSON_Delete(root);

    /* 规范 5.2：14/49/63 必须为 PARK；其余位置不得为 PARK */
    const int park_positions[3] = { PARK_POS_1, PARK_POS_2, PARK_POS_3 };
    for (int i = 0; i < 3; i++) {
        if (g->cells[park_positions[i]].type != CELL_PARK) {
            set_error("地图位置 %d 必须为 PARK（规范 5.2）", park_positions[i]);
            return RC_INVALID_MAP;
        }
    }
    for (int pos = 0; pos < MAP_SIZE; pos++) {
        bool is_fixed_park = (pos == PARK_POS_1 || pos == PARK_POS_2 || pos == PARK_POS_3);
        if (!is_fixed_park && g->cells[pos].type == CELL_PARK) {
            set_error("地图位置 %d 不得为 PARK（规范 5.2：仅 14/49/63 为公园）", pos);
            return RC_INVALID_MAP;
        }
    }

    snprintf(g->map_file, sizeof(g->map_file), "%s", map_file);
    return RC_OK;
}

/* ---- 排序辅助：properties / board_items 按 position 升序（规范 11） ---- */
static int cmp_property(const void *a, const void *b)
{
    const Property *pa = (const Property *)a;
    const Property *pb = (const Property *)b;
    return (pa->position > pb->position) - (pa->position < pb->position);
}

static int cmp_board_item(const void *a, const void *b)
{
    const BoardItem *ia = (const BoardItem *)a;
    const BoardItem *ib = (const BoardItem *)b;
    return (ia->position > ib->position) - (ia->position < ib->position);
}

/* ==================== Preset 加载 ==================== */

/** 解析 random_control（规范 7）。前置条件：case_validate_preset 已通过。 */
static int preset_load_random_control(Game *g, const cJSON *preset)
{
    RandomControl tmp;
    memset(&tmp, 0, sizeof(tmp));

    const cJSON *rc = cJSON_GetObjectItemCaseSensitive(preset, "random_control");
    if (rc == NULL || cJSON_IsNull(rc)) {
        /* 未声明随机控制：使用系统随机（规范 7 非强制） */
        g->rng = tmp;
        return RC_OK;
    }
    if (!cJSON_IsObject(rc)) {
        set_error("preset.random_control 必须为对象");
        return RC_INVALID_PRESET;
    }

    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(rc, "mode");
    if (!cJSON_IsString(mode)) {
        set_error("preset.random_control.mode 必须为字符串（SEQUENCE 或 PRNG）");
        return RC_INVALID_PRESET;
    }
    if (strcmp(mode->valuestring, "SEQUENCE") == 0) {
        tmp.mode = RANDOM_MODE_SEQUENCE;
        const cJSON *streams = cJSON_GetObjectItemCaseSensitive(rc, "streams");
        if (!cJSON_IsObject(streams)) {
            set_error("preset.random_control(SEQUENCE) 缺少 streams 对象");
            return RC_INVALID_PRESET;
        }
        cJSON *s = NULL;
        cJSON_ArrayForEach(s, streams) {
            int k = random_stream_from_name(s->string);
            if (k < 0 || !cJSON_IsArray(s)) {
                set_error("preset.random_control.streams 含未知流: %s", s->string);
                return RC_INVALID_PRESET;
            }
            int32_t n = cJSON_GetArraySize(s);
            if (n > MAX_DICE_SEQ) {
                set_error("preset.random_control.streams.%s 超限", s->string);
                return RC_INVALID_PRESET;
            }
            for (int32_t i = 0; i < n; i++) {
                int32_t v;
                if (!fu_json_get_int32(cJSON_GetArrayItem(s, i), &v)) {
                    set_error("preset.random_control.streams.%s[%d] 必须为 int32 整数",
                              s->string, (int)i);
                    return RC_INVALID_PRESET;
                }
                tmp.seq[k][i] = v;
            }
            tmp.seq_count[k] = n;
        }
        g->rng = tmp;
        return RC_OK;
    }
    if (strcmp(mode->valuestring, "PRNG") == 0) {
        const cJSON *alg = cJSON_GetObjectItemCaseSensitive(rc, "algorithm");
        if (!cJSON_IsString(alg) || strcmp(alg->valuestring, "XORSHIFT32") != 0) {
            set_error("preset.random_control(PRNG).algorithm 必须为 XORSHIFT32");
            return RC_INVALID_PRESET;
        }
        const cJSON *seeds = cJSON_GetObjectItemCaseSensitive(rc, "stream_seeds");
        if (!cJSON_IsObject(seeds)) {
            set_error("preset.random_control(PRNG) 缺少 stream_seeds 对象");
            return RC_INVALID_PRESET;
        }
        tmp.mode = RANDOM_MODE_PRNG;
        cJSON *s = NULL;
        cJSON_ArrayForEach(s, seeds) {
            int k = random_stream_from_name(s->string);
            int32_t v;
            if (k < 0 || !fu_json_get_uint32(s, &v) || v < 1) {
                set_error("preset.random_control.stream_seeds.%s 非法（seed 需为 1~4294967295）",
                          s->string);
                return RC_INVALID_PRESET;
            }
            tmp.prng_state[k] = (uint32_t)v;
        }
        g->rng = tmp;
        return RC_OK;
    }
    set_error("preset.random_control.mode 必须为 SEQUENCE 或 PRNG");
    return RC_INVALID_PRESET;
}

/** 解析 fortune 前置状态（规范 8）。前置条件：case_validate_preset 已通过。 */
static int preset_load_fortune(Game *g, const cJSON *preset)
{
    const cJSON *f = cJSON_GetObjectItemCaseSensitive(preset, "fortune");
    if (f == NULL) {
        /* 防御性缺省：无财神、无生成计划（2.0 校验器已强制要求该字段） */
        g->fortune.position = -1;
        g->fortune.spawned_after_turn = 0;
        g->fortune.remaining_map_turns = 0;
        g->fortune.next_spawn_after_turn = 0;
        return RC_OK;
    }
    if (!cJSON_IsObject(f)) {
        set_error("preset.fortune 必须为对象");
        return RC_INVALID_PRESET;
    }

    const cJSON *pos = cJSON_GetObjectItemCaseSensitive(f, "position");
    const cJSON *sat = cJSON_GetObjectItemCaseSensitive(f, "spawned_after_turn");
    const cJSON *rmt = cJSON_GetObjectItemCaseSensitive(f, "remaining_map_turns");
    const cJSON *nsat = cJSON_GetObjectItemCaseSensitive(f, "next_spawn_after_turn");
    int32_t p, s, r, n;

    if (cJSON_IsNull(pos)) {
        p = -1;
    } else if (!fu_json_get_int32(pos, &p) || p < 0 || p >= MAP_SIZE) {
        set_error("preset.fortune.position 非法（需为 null 或 0~69）");
        return RC_INVALID_PRESET;
    }
    if (cJSON_IsNull(sat)) {
        s = 0;
    } else if (!fu_json_get_int32(sat, &s) || s < 1) {
        set_error("preset.fortune.spawned_after_turn 非法（需为 null 或 >=1 的整数）");
        return RC_INVALID_PRESET;
    }
    if (!fu_json_get_int32(rmt, &r) || r < 0 || r > FORTUNE_MAP_TURNS) {
        set_error("preset.fortune.remaining_map_turns 非法（需为 0~5 的整数）");
        return RC_INVALID_PRESET;
    }
    if (cJSON_IsNull(nsat)) {
        n = 0;
    } else if (!fu_json_get_int32(nsat, &n) || n < 1) {
        set_error("preset.fortune.next_spawn_after_turn 非法（需为 null 或 >=1 的整数）");
        return RC_INVALID_PRESET;
    }

    g->fortune.position = p;
    g->fortune.spawned_after_turn = s;
    g->fortune.remaining_map_turns = r;
    g->fortune.next_spawn_after_turn = n;
    return RC_OK;
}

/**
 * 加载 Preset 前置状态（规范 8）。
 * 前置条件：case_validate_preset 已通过；此处仍做防御性检查，
 * 任何字段类型错误都返回 RC_INVALID_PRESET，不产生部分状态修改。
 */
int game_apply_preset(Game *g, const cJSON *preset)
{
    const cJSON *users = cJSON_GetObjectItemCaseSensitive(preset, "users");
    const cJSON *plist = cJSON_GetObjectItemCaseSensitive(preset, "players");
    if (!cJSON_IsArray(users) || !cJSON_IsArray(plist)) {
        set_error("preset 缺少 users/players 数组");
        return RC_INVALID_PRESET;
    }
    int32_t n = cJSON_GetArraySize(users);
    if (n < 2 || n > MAX_PLAYERS || cJSON_GetArraySize(plist) != n) {
        set_error("preset 玩家数量非法（需 2~%d 且 players 与 users 一一对应）", MAX_PLAYERS);
        return RC_INVALID_PRESET;
    }

    /* 先解析到本地临时数组，全部成功后再写回 Game，保证失败时状态不被部分修改 */
    PLAYER tmp[MAX_PLAYERS];
    memset(tmp, 0, sizeof(tmp));

    for (int32_t i = 0; i < n; i++) {
        const cJSON *u = cJSON_GetArrayItem(users, i);
        const cJSON *p = cJSON_GetArrayItem(plist, i);
        if (!cJSON_IsString(u) || u->valuestring[0] == '\0' ||
            u->valuestring[1] != '\0' || !cJSON_IsObject(p)) {
            set_error("preset users/players[%d] 非法", (int)i);
            return RC_INVALID_PRESET;
        }
        tmp[i].id = u->valuestring[0];

        /* id 需与 users 顺序一致 */
        const cJSON *id_node = cJSON_GetObjectItemCaseSensitive(p, "id");
        if (!cJSON_IsString(id_node) || id_node->valuestring[0] != tmp[i].id ||
            id_node->valuestring[1] != '\0') {
            set_error("preset players[%d].id 与 users 顺序不一致", (int)i);
            return RC_INVALID_PRESET;
        }
        /* fund/credit 只做 int32 契约校验，不做范围限制 */
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "fund"), &tmp[i].fund) ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "credit"), &tmp[i].credit)) {
            set_error("preset players[%d].fund/credit 非法（需为 int32 整数）", (int)i);
            return RC_INVALID_PRESET;
        }
        int32_t pos, gow;
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "position"), &pos) ||
            pos < 0 || pos >= MAP_SIZE ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "god_of_wealth_rounds"), &gow) ||
            gow < 0 || gow > GOD_OF_WEALTH_TURNS) {
            set_error("preset players[%d] 数值字段非法", (int)i);
            return RC_INVALID_PRESET;
        }
        tmp[i].position = (int8_t)pos;
        tmp[i].god_of_wealth_rounds = (int8_t)gow;

        const cJSON *st = cJSON_GetObjectItemCaseSensitive(p, "status");
        if (!cJSON_IsString(st) || (tmp[i].status = (PLAYER_STATUS)player_status_from_str(st->valuestring)) < 0) {
            set_error("preset players[%d].status 非法（只允许 NORMAL/BANKRUPT）", (int)i);
            return RC_INVALID_PRESET;
        }

        /* 背包道具（规范 5.1：仅 BLOCK/ROBOT，合计 <= 10） */
        const cJSON *items = cJSON_GetObjectItemCaseSensitive(p, "items");
        if (!cJSON_IsObject(items)) {
            set_error("preset players[%d].items 非法", (int)i);
            return RC_INVALID_PRESET;
        }
        static const char *const ITEM_FIELDS[] = { "BLOCK", "ROBOT" };
        int8_t counts[2] = { 0, 0 };
        int32_t total = 0;
        for (int k = 0; k < 2; k++) {
            int32_t v;
            if (!fu_json_get_uint32(cJSON_GetObjectItemCaseSensitive(items, ITEM_FIELDS[k]), &v) ||
                v > MAX_ITEM_TOTAL) {
                set_error("preset players[%d].items.%s 非法", (int)i, ITEM_FIELDS[k]);
                return RC_INVALID_PRESET;
            }
            counts[k] = (int8_t)v;
            total += v;
        }
        if (total > MAX_ITEM_TOTAL) {
            set_error("preset players[%d] 背包道具总数超过 %d", (int)i, MAX_ITEM_TOTAL);
            return RC_INVALID_PRESET;
        }
        tmp[i].items.BLOCK = counts[0];
        tmp[i].items.ROBOT = counts[1];
    }

    /* current_user：必须属于 users */
    const cJSON *cur = cJSON_GetObjectItemCaseSensitive(preset, "current_user");
    if (!cJSON_IsString(cur)) {
        set_error("preset 缺少 current_user");
        return RC_INVALID_PRESET;
    }
    int32_t cur_idx = -1;
    for (int32_t i = 0; i < n; i++) {
        if (tmp[i].id == cur->valuestring[0] && cur->valuestring[1] == '\0') {
            cur_idx = i;
            break;
        }
    }
    if (cur_idx < 0 || tmp[cur_idx].status == BANKRUPT) {
        set_error("preset current_user 非法（不属于 users 或已破产）");
        return RC_INVALID_PRESET;
    }

    /* properties：owner 属于 users 且未破产 */
    Property props[MAX_BOARD_ITEMS];
    int32_t prop_count = 0;
    const cJSON *props_json = cJSON_GetObjectItemCaseSensitive(preset, "properties");
    if (!cJSON_IsArray(props_json)) {
        set_error("preset 缺少 properties 数组");
        return RC_INVALID_PRESET;
    }
    prop_count = cJSON_GetArraySize(props_json);
    if (prop_count > MAX_BOARD_ITEMS) {
        set_error("preset properties 数量超限");
        return RC_INVALID_PRESET;
    }
    for (int32_t i = 0; i < prop_count; i++) {
        const cJSON *pp = cJSON_GetArrayItem(props_json, i);
        if (!cJSON_IsObject(pp)) {
            set_error("preset properties[%d] 非法", (int)i);
            return RC_INVALID_PRESET;
        }
        int32_t pos, level;
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(pp, "position"), &pos) ||
            pos < 0 || pos >= MAP_SIZE ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(pp, "level"), &level) ||
            level < 0 || level > LAND_MAX_LEVEL) {
            set_error("preset properties[%d] 数值非法", (int)i);
            return RC_INVALID_PRESET;
        }
        const cJSON *owner = cJSON_GetObjectItemCaseSensitive(pp, "owner");
        if (!cJSON_IsString(owner)) {
            set_error("preset properties[%d].owner 非法", (int)i);
            return RC_INVALID_PRESET;
        }
        int32_t oi = -1;
        for (int32_t j = 0; j < n; j++) {
            if (tmp[j].id == owner->valuestring[0] && owner->valuestring[1] == '\0') {
                oi = j;
                break;
            }
        }
        if (oi < 0 || tmp[oi].status == BANKRUPT) {
            set_error("preset properties[%d].owner 非法", (int)i);
            return RC_INVALID_PRESET;
        }
        props[i].position = pos;
        props[i].owner_index = oi;
        props[i].level = level;
    }

    /* map_items（规范 5.3：仅 BLOCK） */
    BoardItem bits[MAX_BOARD_ITEMS];
    int32_t bit_count = 0;
    const cJSON *bits_json = cJSON_GetObjectItemCaseSensitive(preset, "map_items");
    if (!cJSON_IsArray(bits_json)) {
        set_error("preset 缺少 map_items 数组");
        return RC_INVALID_PRESET;
    }
    bit_count = cJSON_GetArraySize(bits_json);
    if (bit_count > MAX_BOARD_ITEMS) {
        set_error("preset map_items 数量超限");
        return RC_INVALID_PRESET;
    }
    for (int32_t i = 0; i < bit_count; i++) {
        const cJSON *bi = cJSON_GetArrayItem(bits_json, i);
        if (!cJSON_IsObject(bi)) {
            set_error("preset map_items[%d] 非法", (int)i);
            return RC_INVALID_PRESET;
        }
        int32_t pos;
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(bi, "type");
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(bi, "position"), &pos) ||
            pos < 0 || pos >= MAP_SIZE || !cJSON_IsString(type)) {
            set_error("preset map_items[%d] 非法", (int)i);
            return RC_INVALID_PRESET;
        }
        int kind = item_kind_from_str(type->valuestring);
        if (kind != ITEM_BLOCK) {
            set_error("preset map_items[%d].type 只能是 BLOCK（v2.0 已删除 BOMB）", (int)i);
            return RC_INVALID_PRESET;
        }
        bits[i].position = pos;
        bits[i].kind = (ItemKind)kind;
    }

    /* turn_number（规范 8：从 1 开始） */
    const cJSON *tn = cJSON_GetObjectItemCaseSensitive(preset, "turn_number");
    int32_t turn_number = 1;
    if (tn != NULL && !cJSON_IsNull(tn)) {
        if (!fu_json_get_int32(tn, &turn_number) || turn_number < 1) {
            set_error("preset.turn_number 非法（需为 >=1 的整数）");
            return RC_INVALID_PRESET;
        }
    }

    /* ---- 全部解析成功，写回 Game ---- */
    memcpy(g->players, tmp, sizeof(tmp));
    g->user_count = n;
    g->current_index = cur_idx;
    g->phase = PHASE_COMMAND;
    g->status = GAME_RUNNING;
    g->prompt = PROMPT_NONE;
    g->winner_index = -1;
    g->quit = false;
    g->god_acquired_this_turn = false;
    g->turn_number = turn_number;

    memcpy(g->properties, props, sizeof(props));
    g->property_count = prop_count;
    memcpy(g->board_items, bits, sizeof(bits));
    g->board_item_count = bit_count;
    qsort(g->properties, (size_t)prop_count, sizeof(Property), cmp_property);
    qsort(g->board_items, (size_t)bit_count, sizeof(BoardItem), cmp_board_item);

    int rc = preset_load_random_control(g, preset);
    if (rc != RC_OK) {
        return rc;
    }
    return preset_load_fortune(g, preset);
}

/* ==================== 地产经济（规范 3.3） ==================== */

int32_t property_total_invest(const Game *g, const Property *p)
{
    const MapCell *c = &g->cells[p->position];
    return c->price + p->level * c->upgrade_cost;
}

int32_t property_rent(const Game *g, const Property *p)
{
    return property_total_invest(g, p) / 2;     /* 整数除法，规范 3.3 */
}

int32_t property_sell_price(const Game *g, const Property *p)
{
    return property_total_invest(g, p) * 2;
}

/* ==================== 查询 ==================== */

PLAYER *game_current_player(Game *g)
{
    if (g->current_index < 0 || g->current_index >= g->user_count) {
        return NULL;
    }
    return &g->players[g->current_index];
}

const PLAYER *game_current_player_c(const Game *g)
{
    if (g->current_index < 0 || g->current_index >= g->user_count) {
        return NULL;
    }
    return &g->players[g->current_index];
}

const Property *game_property_at(const Game *g, int32_t position)
{
    /* properties 按 position 升序维护，顺序查找即可 */
    for (int32_t i = 0; i < g->property_count; i++) {
        if (g->properties[i].position == position) {
            return &g->properties[i];
        }
    }
    return NULL;
}

const BoardItem *game_board_item_at(const Game *g, int32_t position)
{
    for (int32_t i = 0; i < g->board_item_count; i++) {
        if (g->board_items[i].position == position) {
            return &g->board_items[i];
        }
    }
    return NULL;
}

int game_active_count(const Game *g)
{
    int n = 0;
    for (int32_t i = 0; i < g->user_count; i++) {
        if (g->players[i].status != BANKRUPT) {
            n++;
        }
    }
    return n;
}

bool game_has_finished(const Game *g)
{
    return g->status == GAME_FINISHED;
}

int game_player_index_by_id(const Game *g, const char *id)
{
    /* 角色标识为单字符（规范 3.1：Q/A/S/J） */
    if (id == NULL || id[0] == '\0' || id[1] != '\0') return -1;
    for (int32_t i = 0; i < g->user_count; i++) {
        if (g->players[i].id == id[0]) {
            return i;
        }
    }
    return -1;
}

int game_next_player_index(const Game *g)
{
    /* 按 users 数组顺序选择下一名未破产玩家（规范 4.3） */
    if (g->user_count <= 0) return -1;
    int32_t start = (g->current_index < 0) ? 0 : (g->current_index + 1) % g->user_count;
    for (int32_t i = 0; i < g->user_count; i++) {
        int32_t idx = (start + i) % g->user_count;
        if (g->players[idx].status != BANKRUPT) {
            return idx;
        }
    }
    return -1;
}

/* ==================== 地图财神（规范 14） ==================== */

static bool fortune_position_qualified(const Game *g, int32_t pos)
{
    /* 候选格：0~69、无未破产玩家、无 map_item，且非 TOOL_SHOP/GIFT_SHOP */
    CellType t;

    if (pos < 0 || pos >= MAP_SIZE) {
        return false;
    }
    for (int32_t i = 0; i < g->user_count; i++) {
        const PLAYER *p = &g->players[i];
        if (p->status != BANKRUPT && p->position == pos) {
            return false;
        }
    }
    if (game_board_item_at(g, pos) != NULL) {
        return false;
    }
    t = g->cells[pos].type;
    if (t == CELL_TOOL_SHOP || t == CELL_GIFT_SHOP) {
        return false;
    }
    return true;
}

/** 尝试生成财神：按 FORTUNE_POSITION 流逐个取候选直到合格（规范 14.1/14.2） */
static int fortune_spawn(Game *g)
{
    int32_t pos;

    if (g->fortune.position >= 0) {
        /* 同一时刻最多一个地图财神（规范 14.2） */
        return RC_OK;
    }
    for (;;) {
        int rc = random_next(g, RSTREAM_FORTUNE_POSITION, 0, MAP_SIZE - 1, &pos);
        if (rc != RC_OK) {
            return rc;
        }
        if (fortune_position_qualified(g, pos)) {
            break;
        }
    }

    g->fortune.position = pos;
    g->fortune.spawned_after_turn = g->turn_number;
    g->fortune.remaining_map_turns = FORTUNE_MAP_TURNS;
    g->fortune.next_spawn_after_turn = 0;
    (void)game_print("财神出现在地图 %d 号位置，停留 %d 回合！\n",
                     (int)pos, FORTUNE_MAP_TURNS);
    return RC_OK;
}

/** 领取或自然失效后调度再生成：读取 FORTUNE_RESPAWN_DELAY 1~10（规范 14.4） */
static int fortune_schedule_respawn(Game *g)
{
    int32_t delay;
    int rc = random_next(g, RSTREAM_FORTUNE_RESPAWN_DELAY,
                         FORTUNE_RESPAWN_MIN, FORTUNE_RESPAWN_MAX, &delay);
    if (rc != RC_OK) {
        return rc;
    }
    g->fortune.next_spawn_after_turn = g->turn_number + delay;
    return RC_OK;
}

/**
 * 移动中经过财神格：第一个进入者立即领取（规范 14.4/14.5）。
 * 领取后 god_of_wealth_rounds=5，当轮免租、领取回合不递减。
 */
int game_process_fortune_pickup(Game *g)
{
    PLAYER *p = game_current_player(g);

    g->fortune.position = -1;
    g->fortune.spawned_after_turn = 0;
    g->fortune.remaining_map_turns = 0;
    if (p != NULL) {
        p->god_of_wealth_rounds = GOD_OF_WEALTH_TURNS;
    }
    g->god_acquired_this_turn = true;
    (void)game_print("恭喜玩家拾取财神！%d 回合内免过路费，本回合立即生效。\n",
                     GOD_OF_WEALTH_TURNS);
    return fortune_schedule_respawn(g);
}

/**
 * 回合结束时的财神处理（规范 14.3/14.4）：
 *   1) 财神仍在图上：remaining_map_turns-1；减到 0 立即移除并调度再生成；
 *   2) 无财神且生成计划到期（next_spawn_after_turn == 当前回合号）：生成财神。
 * 在 turn_number 递增前调用，使用"刚完成的回合号"。
 */
int game_process_fortune_turn_end(Game *g)
{
    int rc;

    if (g->fortune.position >= 0) {
        g->fortune.remaining_map_turns--;
        if (g->fortune.remaining_map_turns <= 0) {
            g->fortune.position = -1;
            g->fortune.spawned_after_turn = 0;
            g->fortune.remaining_map_turns = 0;
            (void)game_print("财神已自然消失。\n");
            rc = fortune_schedule_respawn(g);
            if (rc != RC_OK) {
                return rc;
            }
        }
    }

    if (g->fortune.position < 0 &&
        g->fortune.next_spawn_after_turn != 0 &&
        g->fortune.next_spawn_after_turn == g->turn_number) {
        g->fortune.next_spawn_after_turn = 0;
        rc = fortune_spawn(g);
        if (rc != RC_OK) {
            return rc;
        }
    }
    return RC_OK;
}

/* ==================== Action 入口（规范 9） ==================== */

/* game_roll / game_step 实现在 src/usr_action.c */
/* game_move_to 实现在 src/usr_judge.c */

int game_sell(Game *g, int32_t position)
{
    /* game_sell_property 内部已写入玩家可读错误提示 */
    return game_sell_property(g, position);
}

static int handle_prompt_answer(Game *g, const char *value,
                                int (*handler)(Game *, const char *,
                                               char *, size_t))
{
    char message[1024];
    int rc;
    PromptType answered;

    if (g == NULL || handler == NULL) {
        return RC_INVALID_PARAMS;
    }

    answered = g->prompt;
    message[0] = '\0';
    rc = handler(g, value, message, sizeof(message));
    if (message[0] != '\0') {
        (void)game_print("%s\n", message);
    }

    if (rc < 0) {
        /* 道具屋/礼品屋的业务拒绝（点数不足、非法选项）仍算交互完成；
           购买/升级提示的非法回答必须返回参数错误。 */
        if (answered == PROMPT_TOOL_SHOP || answered == PROMPT_GIFT_SHOP) {
            return RC_OK;
        }
        set_error("回答无效");
        return rc;
    }

    if (answered == PROMPT_TOOL_SHOP) {
        if (g->phase == PHASE_COMMAND) {
            return game_finish_action_turn(g);
        }
    } else if (answered == PROMPT_BUY || answered == PROMPT_UPGRADE ||
               answered == PROMPT_GIFT_SHOP) {
        if (g->phase == PHASE_COMMAND) {
            return game_finish_action_turn(g);
        }
    }

    return RC_OK;
}

int game_answer(Game *g, const char *value)
{
    if (g == NULL || value == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        return RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_PROMPT) {
        return RC_INVALID_PHASE;
    }

    switch (g->prompt) {
    case PROMPT_TOOL_SHOP:
        return handle_prompt_answer(g, value, tool_shop_answer);
    case PROMPT_BUY:
        return handle_prompt_answer(g, value, land_answer_buy);
    case PROMPT_UPGRADE:
        return handle_prompt_answer(g, value, land_answer_upgrade);
    case PROMPT_GIFT_SHOP:
        return handle_prompt_answer(g, value, gift_shop_answer);
    default:
        set_error("未知的提示类型");
        return RC_INTERNAL;
    }
}

int game_advance_turn(Game *g)
{
    /* ADVANCE_TURN（规范 6/9）：STATE 测试专用，原地结束当前玩家回合并推进计时器，
       不触发移动或落点。 */
    if (g == NULL) {
        return RC_INVALID_PARAMS;
    }
    if (g->status != GAME_RUNNING) {
        return RC_ACTION_AFTER_END;
    }
    if (g->phase != PHASE_COMMAND) {
        return RC_INVALID_PHASE;
    }
    return game_finish_action_turn(g);
}

int game_query(const Game *g, char *buf, size_t bufsz)
{
    /* QUERY：查询当前玩家资产（规范 9 表），文本不参与状态比较 */
    const PLAYER *p = game_current_player_c(g);
    size_t offset = 0;
    int32_t index;
    int owned_count = 0;

    if (p == NULL) {
        snprintf(buf, bufsz, "当前无行动玩家。\n");
        return RC_OK;
    }

    offset += (size_t)snprintf(
        buf + offset,
        bufsz > offset ? bufsz - offset : 0,
        "玩家 %c：资金 %d，点数 %d，位置 %d，状态 %s\n"
        "道具：路障 %d，机器娃娃 %d；财神剩余 %d 回合\n",
        p->id, (int)p->fund, (int)p->credit, (int)p->position,
        player_status_to_str(p->status),
        (int)p->items.BLOCK, (int)p->items.ROBOT,
        (int)p->god_of_wealth_rounds
    );

    offset += (size_t)snprintf(
        buf + offset,
        bufsz > offset ? bufsz - offset : 0,
        "房产：\n"
    );

    for (index = 0; index < g->property_count; ++index) {
        const Property *prop = &g->properties[index];
        const char *level_name;
        int32_t house_price;
        int32_t rent_price;
        int32_t sell_price;

        if (prop->owner_index != g->current_index) {
            continue;
        }

        switch (prop->level) {
        case 1: level_name = "茅屋"; break;
        case 2: level_name = "洋房"; break;
        case 3: level_name = "摩天楼"; break;
        default: level_name = "空地"; break;
        }

        house_price = property_total_invest(g, prop);
        rent_price = property_rent(g, prop);
        sell_price = property_sell_price(g, prop);
        offset += (size_t)snprintf(
            buf + offset,
            bufsz > offset ? bufsz - offset : 0,
            "  位置 %d，%s，房价 %d 元，收租 %d 元，出售价 %d 元\n",
            prop->position,
            level_name,
            house_price,
            rent_price,
            sell_price
        );
        ++owned_count;
    }

    if (owned_count == 0) {
        offset += (size_t)snprintf(
            buf + offset,
            bufsz > offset ? bufsz - offset : 0,
            "  （暂无房产）\n"
        );
    }

    return RC_OK;
}

int game_help(char *buf, size_t bufsz)
{
    /* HELP：命令帮助文本（v2.0：不再包含 BOMB 等已删除命令） */
    snprintf(buf, bufsz,
             "命令：\n"
             "  ROLL           掷骰子移动（1~6 步）\n"
             "  STEP <n>       按指定步数移动（1~2147483647，超过 70 自动取余）\n"
             "  SELL <pos>     出售指定位置地产\n"
             "  BLOCK <off>    在偏移位置放置路障(-10~10)\n"
             "  ROBOT          清除前方十格路障\n"
             "  QUERY          查询当前玩家资产\n"
             "  HELP           查看本帮助\n"
             "  QUIT           强制结束游戏\n");
    return RC_OK;
}

int game_quit(Game *g)
{
    /* QUIT：强制结束游戏（规范 9：结束游戏；pending_prompt 清空） */
    g->quit = true;
    g->status = GAME_FINISHED;
    g->phase = PHASE_ENDED;
    g->prompt = PROMPT_NONE;
    return RC_OK;
}

/* ==================== 内部流程（规范 4） ==================== */

void handle_mine_landing(Game *g, int32_t position);

static int is_land_cell(CellType type)
{
    return type == CELL_LAND_1 || type == CELL_LAND_2 || type == CELL_LAND_3;
}

void game_settle_landing(Game *g)
{
    int32_t position;
    char message[1024];

    if (g == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count ||
        g->current_index >= MAX_PLAYERS) {
        return;
    }

    position = g->players[g->current_index].position;
    if (position < 0 || position >= MAP_SIZE) {
        return;
    }

    message[0] = '\0';

    switch (g->cells[position].type) {
    case CELL_MINE:
        handle_mine_landing(g, position);
        (void)game_print("到达矿地，获得 %d 点数！\n",
                     g->cells[position].mine_points);
        break;

    case CELL_TOOL_SHOP:
        (void)tool_shop_enter(g, message, sizeof(message));
        if (message[0] != '\0') {
            (void)game_print("%s\n", message);
        }
        break;

    case CELL_GIFT_SHOP:
        (void)gift_shop_enter(g, message, sizeof(message));
        if (message[0] != '\0') {
            (void)game_print("%s\n", message);
        }
        break;

    case CELL_PARK:
        /* 规范 15.1：到达公园不做任何处理，不产生提示 */
        (void)game_print("到达公园，休息一下。\n");
        break;

    case CELL_START:
        /* 起点无事件（经过与到达均无效果） */
        break;

    default:
        if (is_land_cell(g->cells[position].type)) {
            handle_land_landing(g, position);
        }
        break;
    }
}

void game_next_turn(Game *g)
{
    /* 规范 4.3/8：完成第 N 回合后进入 N+1；BANKRUPT 玩家被跳过。
       v2.0 已删除监狱/医院轮空。 */
    if (g->phase == PHASE_ENDED) {
        return;
    }
    g->phase = PHASE_COMMAND;
    g->prompt = PROMPT_NONE;
    g->turn_number++;

    int32_t idx = g->current_index;
    for (int32_t i = 0; i < g->user_count; i++) {
        idx = (idx + 1) % g->user_count;
        if (g->players[idx].status == BANKRUPT) {
            continue;
        }
        g->current_index = idx;
        return;
    }
    /* 所有玩家都无法行动：正常情况 game_check_finish 已先行触发结束 */
    game_check_finish(g);
}

void game_check_finish(Game *g)
{
    /* 规范 4.3 / 9.1：只剩一名未破产玩家时，游戏状态变为 FINISHED */
    int active = game_active_count(g);
    if (active > 1) {
        return;
    }
    g->status = GAME_FINISHED;
    g->phase = PHASE_ENDED;
    g->winner_index = -1;
    for (int32_t i = 0; i < g->user_count; i++) {
        if (g->players[i].status != BANKRUPT) {
            g->winner_index = i;
            break;
        }
    }
    if (g->winner_index >= 0) {
        (void)game_print(
            "\n游戏结束！获胜者：玩家 %c（资金 %d 元）\n",
            g->players[g->winner_index].id,
            g->players[g->winner_index].fund
        );
    } else {
        (void)game_print("\n游戏结束！\n");
    }
}
