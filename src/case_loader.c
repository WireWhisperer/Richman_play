/**
 * @file case_loader.c
 * @brief 测试文件读取与校验（规范 v2.0 第 4/5/7/8/17 节）
 *
 * 仅支持 schema_version 2.0：
 *   - mode：STATE 缺省；PROCESS/INTERACTIVE 尚未实现 -> UNSUPPORTED_MODE；
 *   - expected_outcome / expected_error（code/action_index/path）；
 *   - 删除项校验：BOMB / HOSPITAL / JAIL / remaining_rounds / dice_sequence
 *     一律返回 INVALID_PRESET，MAGIC_HOUSE/HOSPITAL/JAIL 地图类型返回 INVALID_MAP；
 *   - 新增 turn_number、fortune、random_control 校验。
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
    tc->expected_error_action_index = -1;
}

static void load_optional_error_expect(TestCase *tc, const cJSON *obj)
{
    /* 规范 17：expected_outcome=SUCCESS/ERROR；expected_error={code,action_index,path} */
    const cJSON *eo = cJSON_GetObjectItemCaseSensitive(obj, "expected_outcome");
    if (cJSON_IsString(eo)) {
        (void)fu_json_get_string(eo, tc->expected_outcome, sizeof(tc->expected_outcome));
    } else {
        snprintf(tc->expected_outcome, sizeof(tc->expected_outcome), "SUCCESS");
    }
    const cJSON *ee = cJSON_GetObjectItemCaseSensitive(obj, "expected_error");
    if (cJSON_IsObject(ee)) {
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(ee, "code");
        const cJSON *ai = cJSON_GetObjectItemCaseSensitive(ee, "action_index");
        const cJSON *path = cJSON_GetObjectItemCaseSensitive(ee, "path");
        if (cJSON_IsString(code)) {
            (void)fu_json_get_string(code, tc->expected_error_code,
                                     sizeof(tc->expected_error_code));
        }
        if (cJSON_IsNumber(ai)) {
            tc->expected_error_action_index = (int)ai->valuedouble;
        }
        if (cJSON_IsString(path)) {
            (void)fu_json_get_string(path, tc->expected_error_path,
                                     sizeof(tc->expected_error_path));
        }
    }
}

int case_load_from_object(TestCase *tc, cJSON *obj, const char *schema_version,
                          char *errbuf, size_t errsz)
{
    test_case_init(tc);

    if (!cJSON_IsObject(obj)) {
        snprintf(errbuf, errsz, "用例必须是 JSON 对象");
        return RC_INVALID_PRESET;
    }

    if (schema_version == NULL || schema_version[0] == '\0') {
        snprintf(errbuf, errsz, "缺少 schema_version");
        return RC_INVALID_PRESET;
    }
    snprintf(tc->schema_version, sizeof(tc->schema_version), "%s", schema_version);
    if (strcmp(tc->schema_version, "2.0") != 0) {
        snprintf(errbuf, errsz, "不支持的 schema_version: %s（仅支持 2.0）",
                 tc->schema_version);
        return RC_UNSUPPORTED_VERSION;
    }

    /* mode：STATE 缺省；PROCESS/INTERACTIVE 当前未实现 -> UNSUPPORTED_MODE */
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(obj, "mode");
    if (cJSON_IsString(mode) && strcmp(mode->valuestring, "STATE") != 0) {
        snprintf(errbuf, errsz, "不支持的 mode: %s（当前仅实现 STATE）",
                 mode->valuestring);
        return RC_UNSUPPORTED_MODE;
    }

    const cJSON *cid = cJSON_GetObjectItemCaseSensitive(obj, "case_id");
    const cJSON *cname = cJSON_GetObjectItemCaseSensitive(obj, "case_name");
    const cJSON *mf = cJSON_GetObjectItemCaseSensitive(obj, "map_file");
    if (!fu_json_get_string(cid, tc->case_id, sizeof(tc->case_id)) ||
        !fu_json_get_string(cname, tc->case_name, sizeof(tc->case_name)) ||
        !fu_json_get_string(mf, tc->map_file, sizeof(tc->map_file))) {
        snprintf(errbuf, errsz,
                 "缺少必填字段 case_id/case_name/map_file（规范 4）");
        return RC_INVALID_PRESET;
    }

    cJSON *preset = cJSON_DetachItemFromObjectCaseSensitive(obj, "preset");
    cJSON *actions = cJSON_DetachItemFromObjectCaseSensitive(obj, "actions");
    cJSON *expected = cJSON_DetachItemFromObjectCaseSensitive(obj, "expected");

    if (!cJSON_IsObject(preset) || !cJSON_IsArray(actions) ||
        !cJSON_IsObject(expected)) {
        cJSON_Delete(preset);
        cJSON_Delete(actions);
        cJSON_Delete(expected);
        snprintf(errbuf, errsz,
                 "preset 必须为对象、actions 必须为数组、expected 必须为对象");
        return RC_INVALID_PRESET;
    }

    tc->preset = preset;
    tc->actions = actions;
    tc->expected = expected;
    load_optional_error_expect(tc, obj);
    return RC_OK;
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

    /* 套件文件：{ schema_version, suite, tests: [...] } 不在此解析 */
    if (cJSON_GetObjectItemCaseSensitive(root, "tests") != NULL) {
        cJSON_Delete(root);
        snprintf(errbuf, errsz, "套件文件请使用 runner_run_file 解析 tests[]");
        return RC_INVALID_PRESET;
    }

    const cJSON *sv = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    char schema[SCHEMA_VERSION_MAX];
    if (!fu_json_get_string(sv, schema, sizeof(schema))) {
        cJSON_Delete(root);
        snprintf(errbuf, errsz, "缺少顶层必填字段 schema_version");
        return RC_INVALID_PRESET;
    }

    int rc = case_load_from_object(tc, root, schema, errbuf, errsz);
    cJSON_Delete(root);
    return rc;
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

/* ==================== Preset 规则校验（规范 7/8） ==================== */

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

/** 校验单个玩家的所有字段（规范 5.1/5.4 + 规范 2.1 类型契约） */
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
    const char *int_fields[] = { "fund", "credit", "position", "god_of_wealth_rounds" };
    int32_t vals[4];
    for (int i = 0; i < 4; i++) {
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
    if (vals[3] < 0 || vals[3] > GOD_OF_WEALTH_TURNS) {   /* 财神 0~5（规范 5.1） */
        snprintf(prefix, sizeof(prefix),
                 "players[%d].god_of_wealth_rounds 越界（0~%d）", idx, GOD_OF_WEALTH_TURNS);
        return set_err(errbuf, errsz, prefix);
    }

    const cJSON *st = cJSON_GetObjectItemCaseSensitive(p, "status");
    if (!cJSON_IsString(st) || player_status_from_str(st->valuestring) < 0) {
        snprintf(prefix, sizeof(prefix),
                 "players[%d].status 非法（v2.0 只允许 NORMAL/BANKRUPT）", idx);
        return set_err(errbuf, errsz, prefix);
    }

    /* 删除字段：remaining_rounds 必须不存在（规范 5.4/15.2） */
    if (cJSON_GetObjectItemCaseSensitive(p, "remaining_rounds") != NULL) {
        snprintf(prefix, sizeof(prefix),
                 "players[%d].remaining_rounds 已在 v2.0 删除（INVALID_PRESET）", idx);
        return set_err(errbuf, errsz, prefix);
    }

    /* 背包：仅 BLOCK/ROBOT（规范 5.1）；items.BOMB 必须不存在（规范 5.3/15.2） */
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(p, "items");
    if (!cJSON_IsObject(items)) {
        snprintf(prefix, sizeof(prefix), "players[%d].items 必须为对象", idx);
        return set_err(errbuf, errsz, prefix);
    }
    if (cJSON_GetObjectItemCaseSensitive(items, "BOMB") != NULL) {
        snprintf(prefix, sizeof(prefix),
                 "players[%d].items.BOMB 已在 v2.0 删除（INVALID_PRESET）", idx);
        return set_err(errbuf, errsz, prefix);
    }
    static const char *const kinds[] = { "BLOCK", "ROBOT" };
    int32_t total = 0;
    for (int k = 0; k < 2; k++) {
        int32_t v;
        if (!fu_json_get_uint32(cJSON_GetObjectItemCaseSensitive(items, kinds[k]), &v) ||
            v > MAX_ITEM_TOTAL) {
            snprintf(prefix, sizeof(prefix), "players[%d].items.%s 非法（需为 0~%d 整数）",
                     idx, kinds[k], MAX_ITEM_TOTAL);
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

static int validate_fortune(const cJSON *preset, char *errbuf, size_t errsz)
{
    const cJSON *f = cJSON_GetObjectItemCaseSensitive(preset, "fortune");
    if (!cJSON_IsObject(f)) {
        return set_err(errbuf, errsz, "preset.fortune 必须为对象（规范 8）");
    }
    const cJSON *pos = cJSON_GetObjectItemCaseSensitive(f, "position");
    const cJSON *sat = cJSON_GetObjectItemCaseSensitive(f, "spawned_after_turn");
    const cJSON *rmt = cJSON_GetObjectItemCaseSensitive(f, "remaining_map_turns");
    const cJSON *nsat = cJSON_GetObjectItemCaseSensitive(f, "next_spawn_after_turn");
    int32_t p, s, r, n;
    bool pos_null = (pos == NULL || cJSON_IsNull(pos));
    bool sat_null = (sat == NULL || cJSON_IsNull(sat));
    bool nsat_null = (nsat == NULL || cJSON_IsNull(nsat));

    if (!pos_null && (!fu_json_get_int32(pos, &p) || p < 0 || p >= MAP_SIZE)) {
        return set_err(errbuf, errsz, "preset.fortune.position 非法（需为 null 或 0~69）");
    }
    if (!sat_null && (!fu_json_get_int32(sat, &s) || s < 1)) {
        return set_err(errbuf, errsz, "preset.fortune.spawned_after_turn 非法（需为 null 或 >=1）");
    }
    if (!fu_json_get_int32(rmt, &r) || r < 0 || r > FORTUNE_MAP_TURNS) {
        return set_err(errbuf, errsz, "preset.fortune.remaining_map_turns 非法（需为 0~5）");
    }
    if (!nsat_null && (!fu_json_get_int32(nsat, &n) || n < 1)) {
        return set_err(errbuf, errsz, "preset.fortune.next_spawn_after_turn 非法（需为 null 或 >=1）");
    }
    /* 状态一致性（规范 14.2：同一时刻最多一个地图财神） */
    if (pos_null) {
        if (!sat_null || r != 0) {
            return set_err(errbuf, errsz,
                           "preset.fortune 冲突：无财神时 spawned_after_turn 须为 null 且 "
                           "remaining_map_turns 须为 0");
        }
    } else {
        if (sat_null || r < 1 || r > FORTUNE_MAP_TURNS || !nsat_null) {
            return set_err(errbuf, errsz,
                           "preset.fortune 冲突：财神在图上时 spawned_after_turn 须非 null、"
                           "remaining_map_turns 须为 1~5、next_spawn_after_turn 须为 null");
        }
    }
    return RC_OK;
}

static int validate_random_control(const cJSON *preset, char *errbuf, size_t errsz)
{
    const cJSON *rc = cJSON_GetObjectItemCaseSensitive(preset, "random_control");
    if (rc == NULL || cJSON_IsNull(rc)) {
        return RC_OK;   /* 未声明：使用系统随机（规范 7 非强制） */
    }
    if (!cJSON_IsObject(rc)) {
        return set_err(errbuf, errsz, "preset.random_control 必须为对象");
    }
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(rc, "mode");
    if (!cJSON_IsString(mode)) {
        return set_err(errbuf, errsz, "preset.random_control.mode 必须为字符串");
    }
    if (strcmp(mode->valuestring, "SEQUENCE") == 0) {
        const cJSON *streams = cJSON_GetObjectItemCaseSensitive(rc, "streams");
        if (!cJSON_IsObject(streams)) {
            return set_err(errbuf, errsz, "preset.random_control(SEQUENCE) 缺少 streams 对象");
        }
        cJSON *s = NULL;
        cJSON_ArrayForEach(s, streams) {
            int k = -1;
            static const char *const names[RSTREAM_COUNT] = {
                "DICE", "FORTUNE_POSITION", "FORTUNE_RESPAWN_DELAY", "GIFT"
            };
            for (int i = 0; i < RSTREAM_COUNT; i++) {
                if (strcmp(s->string, names[i]) == 0) { k = i; break; }
            }
            if (k < 0 || !cJSON_IsArray(s)) {
                return set_err(errbuf, errsz,
                               "preset.random_control.streams 含未知流（需为数组）");
            }
            for (int i = 0; i < cJSON_GetArraySize(s); i++) {
                int32_t v;
                if (!fu_json_get_int32(cJSON_GetArrayItem(s, i), &v)) {
                    return set_err(errbuf, errsz,
                                   "preset.random_control.streams 的值必须为 int32 整数");
                }
            }
        }
        return RC_OK;
    }
    if (strcmp(mode->valuestring, "PRNG") == 0) {
        const cJSON *alg = cJSON_GetObjectItemCaseSensitive(rc, "algorithm");
        const cJSON *seeds = cJSON_GetObjectItemCaseSensitive(rc, "stream_seeds");
        if (!cJSON_IsString(alg) || strcmp(alg->valuestring, "XORSHIFT32") != 0) {
            return set_err(errbuf, errsz,
                           "preset.random_control(PRNG).algorithm 必须为 XORSHIFT32");
        }
        if (!cJSON_IsObject(seeds)) {
            return set_err(errbuf, errsz,
                           "preset.random_control(PRNG) 缺少 stream_seeds 对象");
        }
        cJSON *s = NULL;
        cJSON_ArrayForEach(s, seeds) {
            int32_t v;
            if (!fu_json_get_uint32(s, &v) || v < 1) {
                return set_err(errbuf, errsz,
                               "preset.random_control.stream_seeds 的 seed 必须为 1~4294967295");
            }
        }
        return RC_OK;
    }
    return set_err(errbuf, errsz, "preset.random_control.mode 必须为 SEQUENCE 或 PRNG");
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

    /* players：与 users 一一对应且顺序一致 */
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

    /* current_user：属于 users 且未破产 */
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

    /* phase：Preset 中必须为 COMMAND */
    const cJSON *phase = cJSON_GetObjectItemCaseSensitive(preset, "phase");
    if (!cJSON_IsString(phase) || strcmp(phase->valuestring, "COMMAND") != 0) {
        return set_err(errbuf, errsz, "preset.phase 必须为 COMMAND");
    }

    /* properties：位置不重复、owner 属于 users 且未破产、level 0~3 */
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

    /* map_items：仅 BLOCK（规范 5.3/15.2），位置不重复 */
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
        if (!cJSON_IsString(type) || strcmp(type->valuestring, "BLOCK") != 0) {
            return set_err(errbuf, errsz,
                           "preset.map_items 的 type 只能是 BLOCK（BOMB 已删除）");
        }
    }

    /* dice_sequence 已删除（附录 A：由 random_control.streams.DICE 取代） */
    if (cJSON_GetObjectItemCaseSensitive(preset, "dice_sequence") != NULL) {
        return set_err(errbuf, errsz,
                       "preset.dice_sequence 已被 random_control.streams.DICE 取代（2.0 删除）");
    }

    /* turn_number（规范 8）：必填，>=1 */
    {
        const cJSON *tn = cJSON_GetObjectItemCaseSensitive(preset, "turn_number");
        int32_t v;
        if (!fu_json_get_int32(tn, &v) || v < 1) {
            return set_err(errbuf, errsz, "preset.turn_number 必须为 >=1 的整数（规范 8）");
        }
    }

    /* fortune（规范 8）：必填 */
    {
        int rc = validate_fortune(preset, errbuf, errsz);
        if (rc != RC_OK) {
            return rc;
        }
    }

    /* random_control（规范 7）：可选 */
    return validate_random_control(preset, errbuf, errsz);
}
