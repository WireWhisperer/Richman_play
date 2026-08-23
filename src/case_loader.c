/**
 * @file case_loader.c
 * @brief 测试文件读取与校验（规范 v1.1 第 6/7 节）
 */
#include "case_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_utils.h"
#include "game.h"

void test_case_init(TestCase *tc)
{
    memset(tc, 0, sizeof(*tc));
}

int case_load(TestCase *tc, const char *json_text, char *errbuf, size_t errsz)
{
    test_case_init(tc);

    cJSON *root = NULL;
    if (fu_parse_json(json_text, &root) != RC_OK) {
        snprintf(errbuf, errsz, "JSON 无法解析（规范 2.1：标准 JSON，无注释/尾随逗号/NaN）");
        return RC_INVALID_JSON;
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        snprintf(errbuf, errsz, "测试文件顶层必须是 JSON 对象");
        return RC_INVALID_PRESET;
    }

    /* 规范 6：六个顶层字段全部必填 */
    const cJSON *sv = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    const cJSON *cid = cJSON_GetObjectItemCaseSensitive(root, "case_id");
    const cJSON *cname = cJSON_GetObjectItemCaseSensitive(root, "case_name");
    const cJSON *mf = cJSON_GetObjectItemCaseSensitive(root, "map_file");
    if (!fu_json_get_string(sv, tc->schema_version, sizeof(tc->schema_version)) ||
        !fu_json_get_string(cid, tc->case_id, sizeof(tc->case_id)) ||
        !fu_json_get_string(cname, tc->case_name, sizeof(tc->case_name)) ||
        !fu_json_get_string(mf, tc->map_file, sizeof(tc->map_file))) {
        cJSON_Delete(root);
        snprintf(errbuf, errsz,
                 "缺少顶层必填字段 schema_version/case_id/case_name/map_file（规范 6）");
        return RC_INVALID_PRESET;
    }
    /* 不支持其他 schema 版本（规范 13 UNSUPPORTED_VERSION） */
    if (strcmp(tc->schema_version, "1.0") != 0) {
        cJSON_Delete(root);
        snprintf(errbuf, errsz, "不支持的 schema_version: %s（当前为 1.0）", tc->schema_version);
        return RC_UNSUPPORTED_VERSION;
    }

    /* preset/actions/expected 保留原始 JSON 树（Detach 后独立管理，规范 6 必填） */
    cJSON *preset = cJSON_DetachItemFromObjectCaseSensitive(root, "preset");
    cJSON *actions = cJSON_DetachItemFromObjectCaseSensitive(root, "actions");
    cJSON *expected = cJSON_DetachItemFromObjectCaseSensitive(root, "expected");
    cJSON_Delete(root);

    if (!cJSON_IsObject(preset) || !cJSON_IsArray(actions) || !cJSON_IsObject(expected)) {
        cJSON_Delete(preset);
        cJSON_Delete(actions);
        cJSON_Delete(expected);
        snprintf(errbuf, errsz, "preset 必须为对象、actions 必须为数组、expected 必须为对象");
        return RC_INVALID_PRESET;
    }
    tc->preset = preset;
    tc->actions = actions;
    tc->expected = expected;
    return RC_OK;
}

int case_load_file(TestCase *tc, const char *path, char *errbuf, size_t errsz)
{
    size_t len = 0;
    char *text = fu_read_file(path, &len);
    if (text == NULL) {
        snprintf(errbuf, errsz, "无法读取测试文件: %s", path);
        return RC_IO_ERROR;
    }
    int rc = case_load(tc, text, errbuf, errsz);
    free(text);
    return rc;
}

void case_free(TestCase *tc)
{
    if (tc->preset)    cJSON_Delete(tc->preset);
    if (tc->actions)   cJSON_Delete(tc->actions);
    if (tc->expected)  cJSON_Delete(tc->expected);
    test_case_init(tc);
}

/* ==================== Preset 规则校验（规范 7.1） ==================== */

static int set_err(char *errbuf, size_t errsz, const char *msg)
{
    snprintf(errbuf, errsz, "%s", msg);
    return RC_INVALID_PRESET;
}

static bool users_contains(const cJSON *users, const char *id)
{
    cJSON *u = NULL;
    cJSON_ArrayForEach(u, users) {
        if (cJSON_IsString(u) && strcmp(u->valuestring, id) == 0) {
            return true;
        }
    }
    return false;
}

/** 按 id 在 players 数组中查找记录，返回下标，找不到返回 -1 */
static int players_find(const cJSON *players, const char *id)
{
    for (int i = 0; i < cJSON_GetArraySize(players); i++) {
        const cJSON *p = cJSON_GetArrayItem(players, i);
        const cJSON *pid = cJSON_GetObjectItemCaseSensitive(p, "id");
        if (cJSON_IsString(pid) && strcmp(pid->valuestring, id) == 0) {
            return i;
        }
    }
    return -1;
}

/** 校验单个玩家的所有字段（规范 7.1 + 规范 2.1 类型契约） */
static int validate_player(const cJSON *p, const cJSON *users, int idx,
                           char *errbuf, size_t errsz)
{
    char prefix[96];
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(p, "id");
    if (!cJSON_IsString(id) || !users_contains(users, id->valuestring)) {
        snprintf(prefix, sizeof(prefix), "players[%d].id 非法或不在 users 中", idx);
        return set_err(errbuf, errsz, prefix);
    }
    snprintf(prefix, sizeof(prefix), "players[%d](%s)", idx, id->valuestring);

    /* 数值字段：int32 契约，bool/浮点/字符串一律拒绝（规范 2.1） */
    const char *int_fields[] = { "fund", "credit", "position", "remaining_rounds",
                                 "god_of_wealth_rounds" };
    int32_t vals[5];
    for (int i = 0; i < 5; i++) {
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(p, int_fields[i]), &vals[i])) {
            snprintf(prefix, sizeof(prefix), "players[%d].%s 非法（需为 int32 整数）",
                     idx, int_fields[i]);
            return set_err(errbuf, errsz, prefix);
        }
    }
    if (vals[2] < 0 || vals[2] >= MAP_SIZE) {       /* position 0~69 */
        snprintf(prefix, sizeof(prefix), "players[%d].position 越界（0~%d）", idx, MAP_SIZE - 1);
        return set_err(errbuf, errsz, prefix);
    }
    if (vals[3] < 0 || vals[4] < 0) {               /* remaining_rounds / 财神 >= 0 */
        snprintf(prefix, sizeof(prefix), "players[%d] 的 remaining_rounds/god_of_wealth_rounds 不能为负", idx);
        return set_err(errbuf, errsz, prefix);
    }

    const cJSON *st = cJSON_GetObjectItemCaseSensitive(p, "status");
    if (!cJSON_IsString(st) || player_status_from_str(st->valuestring) < 0) {
        snprintf(prefix, sizeof(prefix), "players[%d].status 非法", idx);
        return set_err(errbuf, errsz, prefix);
    }

    /* 背包：三类道具合计 <= 10（规范 3.4） */
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(p, "items");
    if (!cJSON_IsObject(items)) {
        snprintf(prefix, sizeof(prefix), "players[%d].items 必须为对象", idx);
        return set_err(errbuf, errsz, prefix);
    }
    static const char *const kinds[] = { "BLOCK", "BOMB", "ROBOT" };
    int32_t total = 0;
    for (int k = 0; k < 3; k++) {
        int32_t v;
        if (!fu_json_get_uint32(cJSON_GetObjectItemCaseSensitive(items, kinds[k]), &v)) {
            snprintf(prefix, sizeof(prefix), "players[%d].items.%s 非法（需为非负整数）", idx, kinds[k]);
            return set_err(errbuf, errsz, prefix);
        }
        total += v;
    }
    if (total > MAX_ITEM_TOTAL) {
        snprintf(prefix, sizeof(prefix), "players[%d] 背包道具总数 %d 超过 %d",
                 idx, (int)total, MAX_ITEM_TOTAL);
        return set_err(errbuf, errsz, prefix);
    }
    return RC_OK;
}

int case_validate_preset(const cJSON *preset, char *errbuf, size_t errsz)
{
    if (!cJSON_IsObject(preset)) {
        return set_err(errbuf, errsz, "preset 必须为对象");
    }

    /* users：2~4 名，标识不重复，单字符（规范 3.1） */
    const cJSON *users = cJSON_GetObjectItemCaseSensitive(preset, "users");
    if (!cJSON_IsArray(users)) {
        return set_err(errbuf, errsz, "preset.users 必须为数组");
    }
    int n = cJSON_GetArraySize(users);
    if (n < 2 || n > MAX_PLAYERS) {
        return set_err(errbuf, errsz, "preset.users 必须包含 2~4 名玩家");
    }
    for (int i = 0; i < n; i++) {
        const cJSON *u = cJSON_GetArrayItem(users, i);
        if (!cJSON_IsString(u) || u->valuestring[0] == '\0' || u->valuestring[1] != '\0') {
            return set_err(errbuf, errsz, "preset.users 每项必须为单字符标识（Q/A/S/J）");
        }
        for (int j = 0; j < i; j++) {
            if (strcmp(cJSON_GetArrayItem(users, j)->valuestring, u->valuestring) == 0) {
                return set_err(errbuf, errsz, "preset.users 存在重复角色");
            }
        }
    }

    /* players：与 users 一一对应且顺序一致（规范 7.1） */
    const cJSON *players = cJSON_GetObjectItemCaseSensitive(preset, "players");
    if (!cJSON_IsArray(players)) {
        return set_err(errbuf, errsz, "preset.players 必须为数组");
    }
    if (cJSON_GetArraySize(players) != n) {
        return set_err(errbuf, errsz, "preset.players 必须为 users 中的每名玩家各提供一条记录");
    }
    for (int i = 0; i < n; i++) {
        const cJSON *p = cJSON_GetArrayItem(players, i);
        if (!cJSON_IsObject(p)) {
            return set_err(errbuf, errsz, "preset.players 每项必须为对象");
        }
        int rc = validate_player(p, users, i, errbuf, errsz);
        if (rc != RC_OK) {
            return rc;
        }
        /* 顺序一致：players[i].id == users[i] */
        const cJSON *pid = cJSON_GetObjectItemCaseSensitive(p, "id");
        const cJSON *u = cJSON_GetArrayItem(users, i);
        if (strcmp(pid->valuestring, u->valuestring) != 0) {
            snprintf(errbuf, errsz, "preset.players 必须按 users 的顺序排列（第 %d 项不一致）", i);
            return RC_INVALID_PRESET;
        }
    }

    /* current_user：属于 users 且未破产（规范 7.1） */
    const cJSON *cur = cJSON_GetObjectItemCaseSensitive(preset, "current_user");
    if (!cJSON_IsString(cur) || !users_contains(users, cur->valuestring)) {
        return set_err(errbuf, errsz, "preset.current_user 必须属于 users");
    }
    int cur_idx = players_find(players, cur->valuestring);
    if (cur_idx < 0) {
        return set_err(errbuf, errsz, "preset.current_user 在 players 中不存在");
    }
    const cJSON *cur_player = cJSON_GetArrayItem(players, cur_idx);
    const cJSON *cur_status = cJSON_GetObjectItemCaseSensitive(cur_player, "status");
    if (cJSON_IsString(cur_status) && strcmp(cur_status->valuestring, "BANKRUPT") == 0) {
        return set_err(errbuf, errsz, "preset.current_user 不能为 BANKRUPT");
    }

    /* phase：Preset 中必须为 COMMAND（规范 7.1） */
    const cJSON *phase = cJSON_GetObjectItemCaseSensitive(preset, "phase");
    if (!cJSON_IsString(phase) || strcmp(phase->valuestring, "COMMAND") != 0) {
        return set_err(errbuf, errsz, "preset.phase 必须为 COMMAND");
    }

    /* properties：位置不重复、owner 属于 users 且未破产、level 0~3（规范 7.1） */
    const cJSON *props = cJSON_GetObjectItemCaseSensitive(preset, "properties");
    if (!cJSON_IsArray(props)) {
        return set_err(errbuf, errsz, "preset.properties 必须为数组");
    }
    bool used_pos[MAP_SIZE];
    memset(used_pos, 0, sizeof(used_pos));
    for (int i = 0; i < cJSON_GetArraySize(props); i++) {
        const cJSON *pp = cJSON_GetArrayItem(props, i);
        if (!cJSON_IsObject(pp)) {
            return set_err(errbuf, errsz, "preset.properties 每项必须为对象");
        }
        int32_t pos, level;
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(pp, "position"), &pos) ||
            pos < 0 || pos >= MAP_SIZE) {
            return set_err(errbuf, errsz, "preset.properties 的 position 非法（0~69）");
        }
        if (used_pos[pos]) {
            snprintf(errbuf, errsz, "preset.properties 地产位置重复: %d", (int)pos);
            return RC_INVALID_PRESET;
        }
        used_pos[pos] = true;
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(pp, "level"), &level) ||
            level < 0 || level > LAND_MAX_LEVEL) {
            return set_err(errbuf, errsz, "preset.properties 的 level 非法（0~3）");
        }
        const cJSON *owner = cJSON_GetObjectItemCaseSensitive(pp, "owner");
        if (!cJSON_IsString(owner) || !users_contains(users, owner->valuestring)) {
            return set_err(errbuf, errsz, "preset.properties 的 owner 必须属于 users");
        }
        int oi = players_find(players, owner->valuestring);
        if (oi >= 0) {
            const cJSON *op = cJSON_GetArrayItem(players, oi);
            const cJSON *os = cJSON_GetObjectItemCaseSensitive(op, "status");
            if (cJSON_IsString(os) && strcmp(os->valuestring, "BANKRUPT") == 0) {
                return set_err(errbuf, errsz, "preset.properties 的 owner 不能为 BANKRUPT");
            }
        }
    }

    /* map_items：仅 BLOCK/BOMB，位置不重复（规范 7.1） */
    const cJSON *bits = cJSON_GetObjectItemCaseSensitive(preset, "map_items");
    if (!cJSON_IsArray(bits)) {
        return set_err(errbuf, errsz, "preset.map_items 必须为数组");
    }
    bool used_bit[MAP_SIZE];
    memset(used_bit, 0, sizeof(used_bit));
    for (int i = 0; i < cJSON_GetArraySize(bits); i++) {
        const cJSON *bi = cJSON_GetArrayItem(bits, i);
        if (!cJSON_IsObject(bi)) {
            return set_err(errbuf, errsz, "preset.map_items 每项必须为对象");
        }
        int32_t pos;
        if (!fu_json_get_int32(cJSON_GetObjectItemCaseSensitive(bi, "position"), &pos) ||
            pos < 0 || pos >= MAP_SIZE) {
            return set_err(errbuf, errsz, "preset.map_items 的 position 非法（0~69）");
        }
        if (used_bit[pos]) {
            snprintf(errbuf, errsz, "preset.map_items 位置重复: %d", (int)pos);
            return RC_INVALID_PRESET;
        }
        used_bit[pos] = true;
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(bi, "type");
        if (!cJSON_IsString(type) ||
            (strcmp(type->valuestring, "BLOCK") != 0 && strcmp(type->valuestring, "BOMB") != 0)) {
            return set_err(errbuf, errsz, "preset.map_items 的 type 只能是 BLOCK 或 BOMB");
        }
    }

    /* dice_sequence：每值 1~6（规范 7.1；允许空数组） */
    const cJSON *dice = cJSON_GetObjectItemCaseSensitive(preset, "dice_sequence");
    if (!cJSON_IsArray(dice)) {
        return set_err(errbuf, errsz, "preset.dice_sequence 必须为数组");
    }
    for (int i = 0; i < cJSON_GetArraySize(dice); i++) {
        int32_t v;
        if (!fu_json_get_int32(cJSON_GetArrayItem(dice, i), &v) || v < DICE_MIN || v > DICE_MAX) {
            snprintf(errbuf, errsz, "preset.dice_sequence[%d] 必须为 1~6", i);
            return RC_INVALID_PRESET;
        }
    }
    return RC_OK;
}
