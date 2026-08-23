/**
 * @file game.c
 * @brief 大富翁游戏核心状态与规则（规范 v1.1 第 3/4/5/9 节）
 *
 * 框架阶段：枚举映射、生命周期、地图加载、地产经济、查询、
 * 回合切换与结束判定已实现；移动/落点/Action 业务逻辑为桩，
 * 留待下一迭代按 TODO 实现。
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

static void set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, ap);
    va_end(ap);
}

const char *game_last_error(void)
{
    return g_last_error;
}

const char *result_code_name(ResultCode rc)
{
    switch (rc) {
    case RC_OK:                  return "OK";
    case RC_INVALID_JSON:        return "INVALID_JSON";
    case RC_UNSUPPORTED_VERSION: return "UNSUPPORTED_VERSION";
    case RC_INVALID_PRESET:      return "INVALID_PRESET";
    case RC_INVALID_MAP:         return "INVALID_MAP";
    case RC_INVALID_COMMAND:     return "INVALID_COMMAND";
    case RC_INVALID_PARAMS:      return "INVALID_PARAMS";
    case RC_INVALID_PHASE:       return "INVALID_PHASE";
    case RC_DICE_SEQUENCE_EMPTY: return "DICE_SEQUENCE_EMPTY";
    case RC_ACTION_AFTER_END:    return "ACTION_AFTER_END";
    case RC_ASSERT_NOT_EQUAL:    return "ASSERT_NOT_EQUAL";
    case RC_ASSERT_NOT_FOUND:    return "ASSERT_NOT_FOUND";
    case RC_ASSERT_NOT_ABSENT:   return "ASSERT_NOT_ABSENT";
    case RC_IO_ERROR:            return "IO_ERROR";
    case RC_INTERNAL:            return "INTERNAL";
    }
    return "UNKNOWN";
}

/* ==================== 枚举 <-> JSON 字符串固定映射（规范 14.2） ==================== */

static const char *const CELL_TYPE_NAMES[CELL_KIND_COUNT] = {
    "START", "LAND_1", "LAND_2", "LAND_3", "TOOL_SHOP", "GIFT_SHOP",
    "MAGIC_HOUSE", "HOSPITAL", "JAIL", "MINE"
};
/* 与 PLAYER_STATUS 枚举顺序一致：NORMAL, HOSPITAL, BANKRUPT, IMPRISONED；
   IMPRISONED 对外输出规范字符串 "JAIL"（规范 3.5） */
static const char *const PLAYER_STATUS_NAMES[] = {
    "NORMAL", "HOSPITAL", "BANKRUPT", "JAIL"
};
static const char *const ITEM_KIND_NAMES[ITEM_KIND_COUNT] = {
    "BLOCK", "BOMB", "ROBOT"
};
static const char *const PHASE_NAMES[] = { "COMMAND", "PROMPT", "ENDED" };
static const char *const GAME_STATUS_NAMES[] = { "RUNNING", "FINISHED" };
static const char *const PROMPT_NAMES[] = {
    "NONE", "BUY", "UPGRADE", "TOOL_SHOP", "GIFT_SHOP"
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
    return (s >= 0 && s < 4) ? PLAYER_STATUS_NAMES[s] : "UNKNOWN";
}

int player_status_from_str(const char *s)
{
    if (s == NULL) return -1;
    for (int i = 0; i < 4; i++) {
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
    /* 规范 7.1：执行每个测试前必须完整重置游戏。
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
    memset(g->dice_seq, 0, sizeof(g->dice_seq));
    g->dice_count = 0;
    g->dice_next = 0;
    g->winner_index = -1;
    g->quit = false;
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
        player->remaining_rounds = 0;
        player->god_of_wealth_rounds = 0;
    }

    g->current_index = 0;
    g->phase = PHASE_COMMAND;
    g->status = GAME_RUNNING;
    g->prompt = PROMPT_NONE;
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
       规范 3.2：地图共有 70 个位置，合法编号 0~69，各语言必须使用内容相同的 map.json。
       校验：恰好 70 格、position 覆盖 0~69 不重复、type 合法、数值为 int32。 */
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
    snprintf(g->map_file, sizeof(g->map_file), "%s", map_file);
    return RC_OK;
}

/* ---- 排序辅助：properties / board_items 按 position 升序（规范 10.1） ---- */
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

/**
 * 加载 Preset 前置状态（规范 7）。
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

    /* 先解析到本地临时数组，全部成功后再写回 Game，保证失败时状态不被部分修改（规则 13） */
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

        /* id 需与 users 顺序一致（规范 7.1） */
        const cJSON *id_node = cJSON_GetObjectItemCaseSensitive(p, "id");
        if (!cJSON_IsString(id_node) || id_node->valuestring[0] != tmp[i].id ||
            id_node->valuestring[1] != '\0') {
            set_error("preset players[%d].id 与 users 顺序不一致", (int)i);
            return RC_INVALID_PRESET;
        }
        /* fund/credit 只做 int32 契约校验，不做 1000~50000 范围限制（团队已确认） */
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "fund"), &tmp[i].fund) ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "credit"), &tmp[i].credit)) {
            set_error("preset players[%d].fund/credit 非法（需为 int32 整数）", (int)i);
            return RC_INVALID_PRESET;
        }
        int32_t pos, rr, gow;
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "position"), &pos) ||
            pos < 0 || pos >= MAP_SIZE ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "remaining_rounds"), &rr) || rr < 0 ||
            !fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, "god_of_wealth_rounds"), &gow) || gow < 0) {
            set_error("preset players[%d] 数值字段非法", (int)i);
            return RC_INVALID_PRESET;
        }
        tmp[i].position = (int8_t)pos;
        tmp[i].remaining_rounds = (int8_t)rr;
        tmp[i].god_of_wealth_rounds = (int8_t)gow;

        const cJSON *st = cJSON_GetObjectItemCaseSensitive(p, "status");
        if (!cJSON_IsString(st) || (tmp[i].status = (PLAYER_STATUS)player_status_from_str(st->valuestring)) < 0) {
            set_error("preset players[%d].status 非法", (int)i);
            return RC_INVALID_PRESET;
        }

        /* 背包道具（规范 3.4：三类合计 <= 10，此处防御性检查） */
        const cJSON *items = cJSON_GetObjectItemCaseSensitive(p, "items");
        if (!cJSON_IsObject(items)) {
            set_error("preset players[%d].items 非法", (int)i);
            return RC_INVALID_PRESET;
        }
        static const char *const ITEM_FIELDS[ITEM_KIND_COUNT] = { "BLOCK", "BOMB", "ROBOT" };
        int8_t counts[ITEM_KIND_COUNT] = { 0, 0, 0 };
        int32_t total = 0;
        for (int k = 0; k < ITEM_KIND_COUNT; k++) {
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
        tmp[i].items.BOMB = counts[1];
        tmp[i].items.ROBOT = counts[2];
    }

    /* current_user（规范 7.1：必须属于 users） */
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

    /* properties（规范 7.1：owner 属于 users 且未破产） */
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

    /* map_items（规范 7.1：仅 BLOCK/BOMB） */
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
        if (kind != ITEM_BLOCK && kind != ITEM_BOMB) {
            set_error("preset map_items[%d].type 只能是 BLOCK/BOMB", (int)i);
            return RC_INVALID_PRESET;
        }
        bits[i].position = pos;
        bits[i].kind = (ItemKind)kind;
    }

    /* dice_sequence（规范 7.1：每值 1~6；可为空数组） */
    int32_t dice[MAX_DICE_SEQ];
    int32_t dice_count = 0;
    const cJSON *dice_json = cJSON_GetObjectItemCaseSensitive(preset, "dice_sequence");
    if (!cJSON_IsArray(dice_json)) {
        set_error("preset 缺少 dice_sequence 数组");
        return RC_INVALID_PRESET;
    }
    dice_count = cJSON_GetArraySize(dice_json);
    if (dice_count > MAX_DICE_SEQ) {
        set_error("preset dice_sequence 超限");
        return RC_INVALID_PRESET;
    }
    for (int32_t i = 0; i < dice_count; i++) {
        int32_t v;
        if (!fu_json_get_int32(cJSON_GetArrayItem(dice_json, i), &v) ||
            v < DICE_MIN || v > DICE_MAX) {
            set_error("preset dice_sequence[%d] 必须为 1~6", (int)i);
            return RC_INVALID_PRESET;
        }
        dice[i] = v;
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

    memcpy(g->properties, props, sizeof(props));
    g->property_count = prop_count;
    memcpy(g->board_items, bits, sizeof(bits));
    g->board_item_count = bit_count;
    qsort(g->properties, (size_t)prop_count, sizeof(Property), cmp_property);
    qsort(g->board_items, (size_t)bit_count, sizeof(BoardItem), cmp_board_item);

    memcpy(g->dice_seq, dice, sizeof(dice));
    g->dice_count = dice_count;
    g->dice_next = 0;
    return RC_OK;
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
    /* properties 按 position 升序维护（规范 10.1），顺序查找即可 */
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

/* ==================== Action 入口（规范 8） ==================== */

/* game_roll / game_step 实现在 src/usr_action.c */
/* game_move_to 实现在 src/usr_judge.c */

int game_sell(Game *g, int32_t position)
{
    (void)g; (void)position;
    set_error("TODO: game_sell 尚未实现（规范 8.2 SELL）");
    return RC_INTERNAL;
}

int game_answer(Game *g, const char *value)
{
    char message[1024];
    int rc;

    if (g == NULL || value == NULL) {
        return RC_INVALID_PARAMS;
    }

    if (g->prompt == PROMPT_TOOL_SHOP) {
        rc = tool_shop_answer(g, value, message, sizeof(message));
        if (message[0] != '\0') {
            (void)printf("%s\n", message);
        }
        return rc < 0 ? rc : RC_OK;
    }

    set_error("TODO: game_answer 尚未实现（规范 8.5 ANSWER）");
    return RC_INTERNAL;
}

int game_query(const Game *g, char *buf, size_t bufsz)
{
    /* QUERY：查询当前玩家资产（规范 8 表），文本不参与状态比较 */
    const PLAYER *p = game_current_player_c(g);
    if (p == NULL) {
        snprintf(buf, bufsz, "当前无行动玩家。\n");
        return RC_OK;
    }
    snprintf(buf, bufsz,
             "玩家 %c：资金 %d，点数 %d，位置 %d，状态 %s\n"
             "道具：路障 %d，炸弹 %d，机器娃娃 %d；财神剩余 %d 回合\n",
             p->id, (int)p->fund, (int)p->credit, (int)p->position,
             player_status_to_str(p->status),
             (int)p->items.BLOCK, (int)p->items.BOMB,
             (int)p->items.ROBOT, (int)p->god_of_wealth_rounds);
    return RC_OK;
}

int game_help(char *buf, size_t bufsz)
{
    /* HELP：命令帮助文本，逐字比较不进入公共测试集（规范 16 可选内容） */
    snprintf(buf, bufsz,
             "命令：\n"
             "  ROLL           使用预置骰子移动\n"
             "  STEP <n>       按指定步数移动\n"
             "  SELL <pos>     出售指定位置地产\n"
             "  BLOCK <off>    在偏移位置放置路障(-10~10)\n"
             "  BOMB <off>     在偏移位置放置炸弹(-10~10)\n"
             "  ROBOT          清除前方十格道具\n"
             "  QUERY          查询当前玩家资产\n"
             "  HELP           查看本帮助\n"
             "  QUIT           强制结束游戏\n");
    return RC_OK;
}

int game_quit(Game *g)
{
    /* QUIT：强制结束游戏（规范 8 表：结束游戏） */
    g->quit = true;
    g->status = GAME_FINISHED;
    g->phase = PHASE_ENDED;
    return RC_OK;
}

/* ==================== 内部流程（规范 4） ==================== */

#include "game.h"

void handle_mine_landing(Game *g, int32_t position);
void handle_jail_landing(Game *g);

void game_settle_landing(Game *g)
{
    int32_t position;

    if (g == NULL || g->current_index < 0 ||
        g->current_index >= g->user_count ||
        g->current_index >= MAX_PLAYERS) {
        return;
    }

    position = g->players[g->current_index].position;
    if (position < 0 || position >= MAP_SIZE) {
        return;
    }

    switch (g->cells[position].type) {
    case CELL_MINE:
        handle_mine_landing(g, position);
        break;

    case CELL_JAIL:
        handle_jail_landing(g);
        break;

    case CELL_TOOL_SHOP:
        (void)tool_shop_enter(g, NULL, 0);
        break;

    default:
        break;
    }
}

void game_next_turn(Game *g)
{
    /* 规范 4.3 回合切换：
       BANKRUPT 不再获得回合；HOSPITAL/JAIL 轮空一次并减 remaining_rounds，
       最后一次轮空后恢复 NORMAL，下次轮到才能行动。 */
    if (g->phase == PHASE_ENDED) {
        return;
    }
    g->phase = PHASE_COMMAND;
    g->prompt = PROMPT_NONE;

    int32_t idx = g->current_index;
    for (int32_t i = 0; i < g->user_count; i++) {
        idx = (idx + 1) % g->user_count;
        PLAYER *p = &g->players[idx];
        if (p->status == BANKRUPT) {
            continue;
        }
        if (p->status == HOSPITAL || p->status == IMPRISONED) {
            p->remaining_rounds--;
            if (p->remaining_rounds <= 0) {
                p->status = NORMAL;
                p->remaining_rounds = 0;
            }
            continue;   /* 本轮空 */
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
}
