/**
 * @file game.c
 * @brief 大富翁游戏核心状态与规则实现 —— 规范 v1.1
 *
 * 实现 game.h 中声明的全部函数。内部采用"逐格移动 + 落点处理 + 回合切换"
 * 的状态机，对外只暴露规范要求的可观察行为。
 */

#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "file_utils.h"

/* ===== 最近一次错误描述 ===== */
static char g_last_error[256];

const char *game_last_error(void)
{
    return g_last_error;
}

static void set_error(ResultCode rc)
{
    const char *name = result_code_name(rc);
    (void)snprintf(g_last_error, sizeof(g_last_error), "%s", name);
}

/* ===== 错误码文本（规范 13） ===== */
const char *result_code_name(ResultCode rc)
{
    switch (rc) {
        case RC_OK: return "OK";
        case RC_INVALID_JSON: return "INVALID_JSON";
        case RC_UNSUPPORTED_VERSION: return "UNSUPPORTED_VERSION";
        case RC_INVALID_PRESET: return "INVALID_PRESET";
        case RC_INVALID_MAP: return "INVALID_MAP";
        case RC_INVALID_COMMAND: return "INVALID_COMMAND";
        case RC_INVALID_PARAMS: return "INVALID_PARAMS";
        case RC_INVALID_PHASE: return "INVALID_PHASE";
        case RC_DICE_SEQUENCE_EMPTY: return "DICE_SEQUENCE_EMPTY";
        case RC_ACTION_AFTER_END: return "ACTION_AFTER_END";
        case RC_ASSERT_NOT_EQUAL: return "ASSERT_NOT_EQUAL";
        case RC_ASSERT_NOT_FOUND: return "ASSERT_NOT_FOUND";
        case RC_ASSERT_NOT_ABSENT: return "ASSERT_NOT_ABSENT";
        case RC_IO_ERROR: return "IO_ERROR";
        case RC_INTERNAL: return "INTERNAL";
        default: return "UNKNOWN";
    }
}

/* ===== 枚举 <-> JSON 字符串固定映射（规范 14.2） ===== */
const char *cell_type_to_str(CellType t)
{
    switch (t) {
        case CELL_START: return "START";
        case CELL_LAND_1: return "LAND_1";
        case CELL_LAND_2: return "LAND_2";
        case CELL_LAND_3: return "LAND_3";
        case CELL_TOOL_SHOP: return "TOOL_SHOP";
        case CELL_GIFT_SHOP: return "GIFT_SHOP";
        case CELL_MAGIC_HOUSE: return "MAGIC_HOUSE";
        case CELL_HOSPITAL: return "HOSPITAL";
        case CELL_JAIL: return "JAIL";
        case CELL_MINE: return "MINE";
        default: return "UNKNOWN";
    }
}

int cell_type_from_str(const char *s)
{
    int i;
    if (s == NULL) {
        return -1;
    }
    for (i = 0; i < CELL_KIND_COUNT; ++i) {
        if (strcmp(s, cell_type_to_str((CellType)i)) == 0) {
            return i;
        }
    }
    return -1;
}

const char *player_status_to_str(PLAYER_STATUS s)
{
    switch (s) {
        case NORMAL: return "NORMAL";
        case HOSPITAL: return "HOSPITAL";
        case BANKRUPT: return "BANKRUPT";
        case IMPRISONED: return "JAIL";   /* 规范 JSON 字符串为 "JAIL" */
        default: return "UNKNOWN";
    }
}

int player_status_from_str(const char *s)
{
    if (s == NULL) {
        return -1;
    }
    if (strcmp(s, "NORMAL") == 0) return NORMAL;
    if (strcmp(s, "HOSPITAL") == 0) return HOSPITAL;
    if (strcmp(s, "BANKRUPT") == 0) return BANKRUPT;
    if (strcmp(s, "JAIL") == 0) return IMPRISONED;
    return -1;
}

const char *item_kind_to_str(ItemKind k)
{
    switch (k) {
        case ITEM_BLOCK: return "BLOCK";
        case ITEM_BOMB: return "BOMB";
        case ITEM_ROBOT: return "ROBOT";
        default: return "UNKNOWN";
    }
}

int item_kind_from_str(const char *s)
{
    if (s == NULL) {
        return -1;
    }
    if (strcmp(s, "BLOCK") == 0) return ITEM_BLOCK;
    if (strcmp(s, "BOMB") == 0) return ITEM_BOMB;
    if (strcmp(s, "ROBOT") == 0) return ITEM_ROBOT;
    return -1;
}

const char *phase_to_str(GamePhase p)
{
    switch (p) {
        case PHASE_COMMAND: return "COMMAND";
        case PHASE_PROMPT: return "PROMPT";
        case PHASE_ENDED: return "ENDED";
        default: return "UNKNOWN";
    }
}

const char *game_status_to_str(GameStatus s)
{
    switch (s) {
        case GAME_RUNNING: return "RUNNING";
        case GAME_FINISHED: return "FINISHED";
        default: return "UNKNOWN";
    }
}

const char *prompt_to_str(PromptType p)
{
    switch (p) {
        case PROMPT_NONE: return "NONE";
        case PROMPT_BUY: return "BUY";
        case PROMPT_UPGRADE: return "UPGRADE";
        case PROMPT_TOOL_SHOP: return "TOOL_SHOP";
        case PROMPT_GIFT_SHOP: return "GIFT_SHOP";
        default: return "UNKNOWN";
    }
}

/* ===== 生命周期（规范 7.1 / 14 步骤 3~4） ===== */
void game_init(Game *game)
{
    int32_t index;

    if (game == NULL) {
        return;
    }

    (void)memset(game, 0, sizeof(*game));

    for (index = 0; index < MAP_SIZE; ++index) {
        game->cells[index].type = CELL_START;
    }

    game->current_index = -1;
    for (index = 0; index < MAX_PLAYERS; ++index) {
        game->players[index].id = '?';
        game->players[index].status = NORMAL;
    }

    for (index = 0; index < MAX_BOARD_ITEMS; ++index) {
        game->properties[index].position = -1;
        game->properties[index].owner_index = -1;
        game->board_items[index].position = -1;
        game->board_items[index].kind = ITEM_BLOCK;
    }

    game->phase = PHASE_COMMAND;
    game->status = GAME_RUNNING;
    game->prompt = PROMPT_NONE;
    game->winner_index = -1;
}

void game_reset(Game *game)
{
    game_init(game);
}

/* 重置状态但保留地图 cells（供 apply_preset 使用） */
static void reset_state(Game *g)
{
    int32_t i;

    g->user_count = 0;
    g->current_index = -1;
    for (i = 0; i < MAX_PLAYERS; ++i) {
        g->players[i].id = '?';
        g->players[i].fund = 0;
        g->players[i].credit = 0;
        g->players[i].position = 0;
        g->players[i].status = NORMAL;
        g->players[i].remaining_rounds = 0;
        g->players[i].items.BLOCK = 0;
        g->players[i].items.BOMB = 0;
        g->players[i].items.ROBOT = 0;
        g->players[i].god_of_wealth_rounds = 0;
    }

    g->property_count = 0;
    g->board_item_count = 0;
    for (i = 0; i < MAX_BOARD_ITEMS; ++i) {
        g->properties[i].position = -1;
        g->properties[i].owner_index = -1;
        g->properties[i].level = 0;
        g->board_items[i].position = -1;
        g->board_items[i].kind = ITEM_BLOCK;
    }

    g->dice_count = 0;
    g->dice_next = 0;
    g->phase = PHASE_COMMAND;
    g->status = GAME_RUNNING;
    g->prompt = PROMPT_NONE;
    g->winner_index = -1;
    g->quit = false;
}

/* ===== 地图加载（规范 3.2） ===== */
int game_load_map(Game *g, const char *map_file)
{
    char *text;
    size_t len;
    cJSON *root;
    cJSON *cells;
    cJSON *cell;
    int loaded;

    if (g == NULL || map_file == NULL) {
        set_error(RC_INVALID_MAP);
        return -RC_INVALID_MAP;
    }

    text = file_read_all(map_file, &len);
    if (text == NULL) {
        /* 回退：尝试在 spec/ 目录下查找（测试用例 map_file 通常为 "map.json"） */
        char spec_path[300];
        (void)snprintf(spec_path, sizeof(spec_path), "spec/%s", map_file);
        text = file_read_all(spec_path, &len);
        if (text == NULL) {
            set_error(RC_INVALID_MAP);
            return -RC_INVALID_MAP;
        }
    }

    root = cJSON_Parse(text);
    free(text);
    if (root == NULL) {
        set_error(RC_INVALID_MAP);
        return -RC_INVALID_MAP;
    }

    cells = cJSON_GetObjectItem(root, "cells");
    if (cells == NULL || !cJSON_IsArray(cells)) {
        cJSON_Delete(root);
        set_error(RC_INVALID_MAP);
        return -RC_INVALID_MAP;
    }

    loaded = 0;
    cJSON_ArrayForEach(cell, cells) {
        cJSON *position = cJSON_GetObjectItem(cell, "position");
        cJSON *type = cJSON_GetObjectItem(cell, "type");
        cJSON *price = cJSON_GetObjectItem(cell, "price");
        cJSON *upgrade = cJSON_GetObjectItem(cell, "upgrade_cost");
        cJSON *mine = cJSON_GetObjectItem(cell, "mine_points");
        int pos;
        int kind;

        if (position == NULL || !cJSON_IsNumber(position) ||
            type == NULL || !cJSON_IsString(type)) {
            continue;
        }

        pos = (int)position->valuedouble;
        kind = cell_type_from_str(type->valuestring);
        if (pos < 0 || pos >= MAP_SIZE || kind < 0) {
            continue;
        }

        g->cells[pos].type = (CellType)kind;
        g->cells[pos].price = (price != NULL && cJSON_IsNumber(price))
                                  ? (int32_t)price->valuedouble : 0;
        g->cells[pos].upgrade_cost = (upgrade != NULL && cJSON_IsNumber(upgrade))
                                         ? (int32_t)upgrade->valuedouble : 0;
        g->cells[pos].mine_points = (mine != NULL && cJSON_IsNumber(mine))
                                        ? (int32_t)mine->valuedouble : 0;
        ++loaded;
    }

    cJSON_Delete(root);

    if (loaded != MAP_SIZE) {
        set_error(RC_INVALID_MAP);
        return -RC_INVALID_MAP;
    }

    return 0;
}

/* ===== Preset 加载（规范 7） ===== */
int game_apply_preset(Game *g, const cJSON *preset)
{
    cJSON *users;
    cJSON *current_user;
    cJSON *phase;
    cJSON *game_status;
    cJSON *players;
    cJSON *properties;
    cJSON *map_items;
    cJSON *dice;
    int user_count;
    int i;

    if (g == NULL || preset == NULL || !cJSON_IsObject(preset)) {
        set_error(RC_INVALID_PRESET);
        return -RC_INVALID_PRESET;
    }

    reset_state(g);

    users = cJSON_GetObjectItem(preset, "users");
    if (users == NULL || !cJSON_IsArray(users)) {
        set_error(RC_INVALID_PRESET);
        return -RC_INVALID_PRESET;
    }

    user_count = cJSON_GetArraySize(users);
    if (user_count < 2 || user_count > MAX_PLAYERS) {
        set_error(RC_INVALID_PRESET);
        return -RC_INVALID_PRESET;
    }

    for (i = 0; i < user_count; ++i) {
        cJSON *u = cJSON_GetArrayItem(users, i);
        if (u == NULL || !cJSON_IsString(u) || u->valuestring[0] == '\0') {
            set_error(RC_INVALID_PRESET);
            return -RC_INVALID_PRESET;
        }
        g->players[i].id = u->valuestring[0];
    }
    g->user_count = user_count;

    /* 同一角色不能重复参加（规范 3.1） */
    for (i = 0; i < user_count; ++i) {
        int j;
        for (j = i + 1; j < user_count; ++j) {
            if (g->players[i].id == g->players[j].id) {
                set_error(RC_INVALID_PRESET);
                return -RC_INVALID_PRESET;
            }
        }
    }

    current_user = cJSON_GetObjectItem(preset, "current_user");
    g->current_index = (current_user != NULL && cJSON_IsString(current_user))
                           ? game_player_index_by_id(g, current_user->valuestring)
                           : 0;
    if (g->current_index < 0) {
        g->current_index = 0;
    }

    phase = cJSON_GetObjectItem(preset, "phase");
    if (phase != NULL && cJSON_IsString(phase)) {
        if (strcmp(phase->valuestring, "PROMPT") == 0) g->phase = PHASE_PROMPT;
        else if (strcmp(phase->valuestring, "ENDED") == 0) g->phase = PHASE_ENDED;
        else g->phase = PHASE_COMMAND;
    }

    game_status = cJSON_GetObjectItem(preset, "game_status");
    if (game_status != NULL && cJSON_IsString(game_status)) {
        g->status = (strcmp(game_status->valuestring, "FINISHED") == 0)
                        ? GAME_FINISHED : GAME_RUNNING;
    }

    players = cJSON_GetObjectItem(preset, "players");
    if (players != NULL && cJSON_IsArray(players)) {
        int n = cJSON_GetArraySize(players);
        for (i = 0; i < n && i < user_count; ++i) {
            cJSON *p = cJSON_GetArrayItem(players, i);
            cJSON *it;
            cJSON *id = cJSON_GetObjectItem(p, "id");
            cJSON *fund = cJSON_GetObjectItem(p, "fund");
            cJSON *credit = cJSON_GetObjectItem(p, "credit");
            cJSON *pos = cJSON_GetObjectItem(p, "position");
            cJSON *st = cJSON_GetObjectItem(p, "status");
            cJSON *rr = cJSON_GetObjectItem(p, "remaining_rounds");
            cJSON *gwr = cJSON_GetObjectItem(p, "god_of_wealth_rounds");
            PLAYER *pl = &g->players[i];

            if (id != NULL && cJSON_IsString(id) && id->valuestring[0] != '\0') {
                pl->id = id->valuestring[0];
            }
            if (fund != NULL && cJSON_IsNumber(fund)) pl->fund = (int32_t)fund->valuedouble;
            if (credit != NULL && cJSON_IsNumber(credit)) pl->credit = (int32_t)credit->valuedouble;
            if (pos != NULL && cJSON_IsNumber(pos)) pl->position = (int8_t)pos->valuedouble;
            if (st != NULL && cJSON_IsString(st)) {
                int s = player_status_from_str(st->valuestring);
                if (s >= 0) pl->status = (PLAYER_STATUS)s;
            }
            if (rr != NULL && cJSON_IsNumber(rr)) pl->remaining_rounds = (int8_t)rr->valuedouble;
            if (gwr != NULL && cJSON_IsNumber(gwr)) pl->god_of_wealth_rounds = (int8_t)gwr->valuedouble;

            it = cJSON_GetObjectItem(p, "items");
            if (it != NULL && cJSON_IsObject(it)) {
                cJSON *b = cJSON_GetObjectItem(it, "BLOCK");
                cJSON *m = cJSON_GetObjectItem(it, "BOMB");
                cJSON *r = cJSON_GetObjectItem(it, "ROBOT");
                if (b != NULL && cJSON_IsNumber(b)) pl->items.BLOCK = (int8_t)b->valuedouble;
                if (m != NULL && cJSON_IsNumber(m)) pl->items.BOMB = (int8_t)m->valuedouble;
                if (r != NULL && cJSON_IsNumber(r)) pl->items.ROBOT = (int8_t)r->valuedouble;
            }
        }
    }

    properties = cJSON_GetObjectItem(preset, "properties");
    if (properties != NULL && cJSON_IsArray(properties)) {
        int n = cJSON_GetArraySize(properties);
        g->property_count = 0;
        for (i = 0; i < n && g->property_count < MAX_BOARD_ITEMS; ++i) {
            cJSON *p = cJSON_GetArrayItem(properties, i);
            cJSON *pos = cJSON_GetObjectItem(p, "position");
            cJSON *owner = cJSON_GetObjectItem(p, "owner");
            cJSON *level = cJSON_GetObjectItem(p, "level");
            if (pos == NULL || !cJSON_IsNumber(pos) || owner == NULL || !cJSON_IsString(owner)) {
                continue;
            }
            g->properties[g->property_count].position = (int32_t)pos->valuedouble;
            g->properties[g->property_count].owner_index =
                game_player_index_by_id(g, owner->valuestring);
            g->properties[g->property_count].level =
                (level != NULL && cJSON_IsNumber(level)) ? (int32_t)level->valuedouble : 0;
            ++g->property_count;
        }
    }

    map_items = cJSON_GetObjectItem(preset, "map_items");
    if (map_items != NULL && cJSON_IsArray(map_items)) {
        int n = cJSON_GetArraySize(map_items);
        g->board_item_count = 0;
        for (i = 0; i < n && g->board_item_count < MAX_BOARD_ITEMS; ++i) {
            cJSON *it = cJSON_GetArrayItem(map_items, i);
            cJSON *pos = cJSON_GetObjectItem(it, "position");
            cJSON *type = cJSON_GetObjectItem(it, "type");
            if (pos == NULL || !cJSON_IsNumber(pos) || type == NULL || !cJSON_IsString(type)) {
                continue;
            }
            g->board_items[g->board_item_count].position = (int32_t)pos->valuedouble;
            g->board_items[g->board_item_count].kind = (ItemKind)item_kind_from_str(type->valuestring);
            ++g->board_item_count;
        }
    }

    dice = cJSON_GetObjectItem(preset, "dice_sequence");
    if (dice != NULL && cJSON_IsArray(dice)) {
        int n = cJSON_GetArraySize(dice);
        g->dice_count = 0;
        for (i = 0; i < n && g->dice_count < MAX_DICE_SEQ; ++i) {
            cJSON *d = cJSON_GetArrayItem(dice, i);
            if (d != NULL && cJSON_IsNumber(d)) {
                g->dice_seq[g->dice_count++] = (int32_t)d->valuedouble;
            }
        }
    }
    g->dice_next = 0;

    return 0;
}

/* ===== 地产经济（规范 3.3） ===== */
int32_t property_total_invest(const Game *g, const Property *p)
{
    if (g == NULL || p == NULL || p->position < 0 || p->position >= MAP_SIZE) {
        return 0;
    }
    return g->cells[p->position].price + p->level * g->cells[p->position].upgrade_cost;
}

int32_t property_rent(const Game *g, const Property *p)
{
    return property_total_invest(g, p) / 2;
}

int32_t property_sell_price(const Game *g, const Property *p)
{
    return property_total_invest(g, p) * 2;
}

void get_rent(Game *g, Property p)
{
    PLAYER *payer;
    PLAYER *owner;
    int32_t rent;

    if (g == NULL) {
        return;
    }

    payer = game_current_player(g);
    if (payer == NULL || p.owner_index < 0 || p.owner_index >= g->user_count) {
        return;
    }

    /* 地主在医院/监狱时不收租（规范 US10 验收） */
    owner = &g->players[p.owner_index];
    if (owner->status == HOSPITAL || owner->status == IMPRISONED) {
        return;
    }
    /* 过路者持有财神 buff 免租（规范 US10/US22） */
    if (payer->god_of_wealth_rounds > 0) {
        return;
    }

    rent = property_rent(g, &p);
    /* 面对破产玩家时收取房租为 0（规范 US10） */
    if (owner->status == BANKRUPT) {
        rent = 0;
    }

    payer->fund -= rent;
    owner->fund += rent;
    if (payer->fund < 0) {
        payer->status = BANKRUPT;
        payer->fund = 0;
    }
    game_check_finish(g);
}

/* ===== 查询 ===== */
PLAYER *game_current_player(Game *g)
{
    if (g == NULL || g->current_index < 0 || g->current_index >= g->user_count) {
        return NULL;
    }
    return &g->players[g->current_index];
}

const PLAYER *game_current_player_c(const Game *g)
{
    if (g == NULL || g->current_index < 0 || g->current_index >= g->user_count) {
        return NULL;
    }
    return &g->players[g->current_index];
}

const Property *game_property_at(const Game *g, int32_t position)
{
    int32_t i;
    if (g == NULL) {
        return NULL;
    }
    for (i = 0; i < g->property_count; ++i) {
        if (g->properties[i].position == position) {
            return &g->properties[i];
        }
    }
    return NULL;
}

const BoardItem *game_board_item_at(const Game *g, int32_t position)
{
    int32_t i;
    if (g == NULL) {
        return NULL;
    }
    for (i = 0; i < g->board_item_count; ++i) {
        if (g->board_items[i].position == position) {
            return &g->board_items[i];
        }
    }
    return NULL;
}

int game_active_count(const Game *g)
{
    int32_t i;
    int count = 0;
    if (g == NULL) {
        return 0;
    }
    for (i = 0; i < g->user_count; ++i) {
        if (g->players[i].status != BANKRUPT) {
            ++count;
        }
    }
    return count;
}

bool game_has_finished(const Game *g)
{
    return game_active_count(g) <= 1;
}

int game_player_index_by_id(const Game *g, const char *id)
{
    int32_t i;
    if (g == NULL || id == NULL || id[0] == '\0') {
        return -1;
    }
    for (i = 0; i < g->user_count; ++i) {
        if (g->players[i].id == id[0]) {
            return (int)i;
        }
    }
    return -1;
}

int game_next_player_index(const Game *g)
{
    int32_t i;
    int start;
    if (g == NULL || g->user_count <= 0) {
        return -1;
    }
    start = (g->current_index + 1) % g->user_count;
    for (i = 0; i < g->user_count; ++i) {
        int idx = (start + i) % g->user_count;
        if (g->players[idx].status != BANKRUPT) {
            return idx;
        }
    }
    return -1;
}

/* ===== 内部：道具生效 / 清除 ===== */
void game_remove_board_item(Game *g, int index)
{
    int i;
    if (g == NULL || index < 0 || index >= g->board_item_count) {
        return;
    }
    for (i = index; i < g->board_item_count - 1; ++i) {
        g->board_items[i] = g->board_items[i + 1];
    }
    --g->board_item_count;
}

void game_boarditem_suc(Game *g, BoardItem *b, int8_t index)
{
    if (g == NULL || b == NULL) {
        return;
    }

    if (b->kind == ITEM_BOMB) {
        PLAYER *p = game_current_player(g);
        p->position = HOSPITAL_POS;
        p->status = HOSPITAL;
        p->remaining_rounds = HOSPITAL_ROUNDS;
        game_remove_board_item(g, index);
        game_next_turn(g);
    } else if (b->kind == ITEM_BLOCK) {
        PLAYER *p = game_current_player(g);
        p->position = (int8_t)b->position;
        game_remove_board_item(g, index);
        game_settle_landing(g);
    }
}

/* ===== 内部：回合切换（规范 4.3） ===== */
void game_check_finish(Game *g)
{
    int32_t i;
    int active = 0;
    int last = -1;

    if (g == NULL) {
        return;
    }
    for (i = 0; i < g->user_count; ++i) {
        if (g->players[i].status != BANKRUPT) {
            ++active;
            last = i;
        }
    }
    if (active <= 1) {
        g->status = GAME_FINISHED;
        g->phase = PHASE_ENDED;
        g->winner_index = last;
    }
}

void game_next_turn(Game *g)
{
    int32_t i;
    if (g == NULL || g->user_count <= 0) {
        return;
    }

    game_check_finish(g);
    if (g->status == GAME_FINISHED) {
        g->current_index = -1;
        return;
    }

    for (i = 0; i < g->user_count; ++i) {
        PLAYER *p;
        g->current_index = (g->current_index + 1) % g->user_count;
        p = &g->players[g->current_index];

        if (p->status == BANKRUPT) {
            continue;
        }
        if (p->status == HOSPITAL || p->status == IMPRISONED) {
            if (p->remaining_rounds > 0) {
                --p->remaining_rounds;
                if (p->remaining_rounds == 0) {
                    p->status = NORMAL;
                }
                continue;
            }
        }
        /* 该玩家可以行动 */
        return;
    }

    /* 所有玩家都被跳过（理论上不会发生） */
    g->current_index = -1;
}

/* ===== 内部：逐格移动（规范 8.1 / 4.2） ===== */
int game_move_to(Game *g, int32_t steps, int8_t last_position)
{
    int32_t i;
    int8_t pos;
    PLAYER *p;

    if (g == NULL || steps <= 0) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    p = game_current_player(g);
    if (p == NULL) {
        set_error(RC_INVALID_PHASE);
        return -RC_INVALID_PHASE;
    }

    pos = last_position;
    for (i = 0; i < steps; ++i) {
        const BoardItem *item;
        pos = (int8_t)((pos + 1) % MAP_SIZE);
        item = game_board_item_at(g, pos);
        if (item != NULL) {
            int idx = (int)(item - g->board_items);
            if (item->kind == ITEM_BLOCK) {
                p->position = pos;
                game_remove_board_item(g, idx);
                game_settle_landing(g);
                return pos;
            } else {
                /* BOMB */
                p->position = HOSPITAL_POS;
                p->status = HOSPITAL;
                p->remaining_rounds = HOSPITAL_ROUNDS;
                game_remove_board_item(g, idx);
                game_next_turn(g);
                return HOSPITAL_POS;
            }
        }
    }

    p->position = pos;
    game_settle_landing(g);
    return pos;
}

/* ===== 内部：落点处理（规范 9） ===== */
void game_settle_landing(Game *g)
{
    PLAYER *p;
    CellType type;
    const Property *prop;

    if (g == NULL) {
        return;
    }
    p = game_current_player(g);
    if (p == NULL) {
        return;
    }

    type = g->cells[p->position].type;
    switch (type) {
        case CELL_LAND_1:
        case CELL_LAND_2:
        case CELL_LAND_3:
            prop = game_property_at(g, p->position);
            if (prop == NULL) {
                /* 无人地产：进入购买提示 */
                g->phase = PHASE_PROMPT;
                g->prompt = PROMPT_BUY;
            } else if (prop->owner_index == g->current_index) {
                if (prop->level < LAND_MAX_LEVEL) {
                    g->phase = PHASE_PROMPT;
                    g->prompt = PROMPT_UPGRADE;
                } else {
                    game_next_turn(g);
                }
            } else {
                get_rent(g, *prop);
                if (g->status != GAME_FINISHED) {
                    game_next_turn(g);
                }
            }
            return;

        case CELL_START:
        case CELL_HOSPITAL:
            game_next_turn(g);
            return;

        case CELL_JAIL:
            p->status = IMPRISONED;
            p->remaining_rounds = JAIL_ROUNDS;
            game_next_turn(g);
            return;

        case CELL_TOOL_SHOP:
            if (p->credit < 30) {
                /* 点数不足自动退出道具屋 */
                game_next_turn(g);
            } else {
                g->phase = PHASE_PROMPT;
                g->prompt = PROMPT_TOOL_SHOP;
            }
            return;

        case CELL_GIFT_SHOP:
            g->phase = PHASE_PROMPT;
            g->prompt = PROMPT_GIFT_SHOP;
            return;

        case CELL_MAGIC_HOUSE:
            /* 魔法屋暂不进入本次开发范围（约束 C10） */
            game_next_turn(g);
            return;

        case CELL_MINE:
            p->credit += g->cells[p->position].mine_points;
            game_next_turn(g);
            return;

        default:
            game_next_turn(g);
            return;
    }
}

/* ===== 辅助：当前玩家是否被跳过（住院/入狱轮空） ===== */
static bool current_player_skipped(const Game *g)
{
    const PLAYER *p = game_current_player_c(g);
    if (p == NULL) {
        return false;
    }
    if ((p->status == HOSPITAL || p->status == IMPRISONED) && p->remaining_rounds > 0) {
        return true;
    }
    return false;
}

/* 轮空当前玩家：remaining_rounds 减 1，归零则恢复 NORMAL，然后切换到下一玩家 */
static void skip_current_player(Game *g)
{
    PLAYER *p = game_current_player(g);
    if (p == NULL) {
        return;
    }
    if (p->remaining_rounds > 0) {
        --p->remaining_rounds;
        if (p->remaining_rounds == 0) {
            p->status = NORMAL;
        }
    }
    game_next_turn(g);
}

/* ===== Action 入口（规范 8） ===== */
int game_roll(Game *g)
{
    int32_t steps;
    int8_t last;

    if (g == NULL || g->phase != PHASE_COMMAND) {
        set_error(RC_INVALID_PHASE);
        return -RC_INVALID_PHASE;
    }
    /* 住院/入狱轮空：自动跳过当前玩家 */
    if (current_player_skipped(g)) {
        skip_current_player(g);
        return 0;
    }
    if (g->dice_next >= g->dice_count) {
        set_error(RC_DICE_SEQUENCE_EMPTY);
        return -RC_DICE_SEQUENCE_EMPTY;
    }

    steps = g->dice_seq[g->dice_next++];
    last = game_current_player(g)->position;
    {
        int rc = game_move_to(g, steps, last);
        return rc < 0 ? rc : 0;
    }
}

int game_step(Game *g, int32_t steps)
{
    int8_t last;

    if (g == NULL || g->phase != PHASE_COMMAND) {
        set_error(RC_INVALID_PHASE);
        return -RC_INVALID_PHASE;
    }
    if (current_player_skipped(g)) {
        skip_current_player(g);
        return 0;
    }
    if (steps <= 0) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    last = game_current_player(g)->position;
    {
        int rc = game_move_to(g, steps, last);
        return rc < 0 ? rc : 0;
    }
}

int game_sell(Game *g, int32_t position)
{
    const Property *prop;
    PLAYER *p;
    int32_t i;

    if (g == NULL || g->phase != PHASE_COMMAND) {
        set_error(RC_INVALID_PHASE);
        return -RC_INVALID_PHASE;
    }
    if (position < 0 || position >= MAP_SIZE) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    prop = game_property_at(g, position);
    if (prop == NULL || prop->owner_index != g->current_index) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    p = game_current_player(g);
    p->fund += property_sell_price(g, prop);

    /* 移除该地产 */
    for (i = 0; i < g->property_count; ++i) {
        if (g->properties[i].position == position) {
            int j;
            for (j = i; j < g->property_count - 1; ++j) {
                g->properties[j] = g->properties[j + 1];
            }
            --g->property_count;
            break;
        }
    }
    return 0;
}

static int item_total(const PLAYER *p)
{
    return (int)p->items.BLOCK + (int)p->items.BOMB + (int)p->items.ROBOT;
}

static int place_board_item(Game *g, int32_t position, ItemKind kind)
{
    if (game_board_item_at(g, position) != NULL) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }
    if (g->board_item_count >= MAX_BOARD_ITEMS) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }
    g->board_items[g->board_item_count].position = position;
    g->board_items[g->board_item_count].kind = kind;
    ++g->board_item_count;
    return 0;
}

int game_block(Game *g, int32_t offset)
{
    PLAYER *p;
    int32_t target;

    if (g == NULL || g->phase != PHASE_COMMAND) {
        set_error(RC_INVALID_PHASE);
        return -RC_INVALID_PHASE;
    }
    if (offset < -BLOCK_OFFSET_LIMIT || offset > BLOCK_OFFSET_LIMIT || offset == 0) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    p = game_current_player(g);
    if (p->items.BLOCK <= 0) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    target = ((int32_t)p->position + offset + MAP_SIZE) % MAP_SIZE;
    if (place_board_item(g, target, ITEM_BLOCK) != 0) {
        return -RC_INVALID_PARAMS;
    }
    --p->items.BLOCK;
    return 0;
}

int game_bomb(Game *g, int32_t offset)
{
    PLAYER *p;
    int32_t target;

    if (g == NULL || g->phase != PHASE_COMMAND) {
        set_error(RC_INVALID_PHASE);
        return -RC_INVALID_PHASE;
    }
    if (offset < -BLOCK_OFFSET_LIMIT || offset > BLOCK_OFFSET_LIMIT || offset == 0) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    p = game_current_player(g);
    if (p->items.BOMB <= 0) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    target = ((int32_t)p->position + offset + MAP_SIZE) % MAP_SIZE;
    if (place_board_item(g, target, ITEM_BOMB) != 0) {
        return -RC_INVALID_PARAMS;
    }
    --p->items.BOMB;
    return 0;
}

int game_robot(Game *g)
{
    PLAYER *p;
    int32_t i;

    if (g == NULL || g->phase != PHASE_COMMAND) {
        set_error(RC_INVALID_PHASE);
        return -RC_INVALID_PHASE;
    }
    p = game_current_player(g);
    if (p->items.ROBOT <= 0) {
        set_error(RC_INVALID_PARAMS);
        return -RC_INVALID_PARAMS;
    }

    for (i = 1; i <= ROBOT_CLEAR_RANGE; ++i) {
        int32_t pos = ((int32_t)p->position + i) % MAP_SIZE;
        const BoardItem *item = game_board_item_at(g, pos);
        if (item != NULL) {
            game_remove_board_item(g, (int)(item - g->board_items));
        }
    }
    --p->items.ROBOT;
    return 0;
}

int game_answer(Game *g, const char *value)
{
    PLAYER *p;

    if (g == NULL || g->phase != PHASE_PROMPT) {
        set_error(RC_INVALID_PHASE);
        return -RC_INVALID_PHASE;
    }
    if (value == NULL) {
        value = "";
    }
    p = game_current_player(g);

    switch (g->prompt) {
        case PROMPT_BUY: {
            const Property *prop = game_property_at(g, p->position);
            if (prop == NULL && (value[0] == 'Y' || value[0] == 'y')) {
                int32_t price = g->cells[p->position].price;
                if (p->fund >= price) {
                    if (g->property_count < MAX_BOARD_ITEMS) {
                        g->properties[g->property_count].position = p->position;
                        g->properties[g->property_count].owner_index = g->current_index;
                        g->properties[g->property_count].level = 0;
                        ++g->property_count;
                        p->fund -= price;
                    }
                }
            }
            break;
        }
        case PROMPT_UPGRADE: {
            Property *prop = NULL;
            int32_t j;
            for (j = 0; j < g->property_count; ++j) {
                if (g->properties[j].position == p->position &&
                    g->properties[j].owner_index == g->current_index) {
                    prop = &g->properties[j];
                    break;
                }
            }
            if (prop != NULL && (value[0] == 'Y' || value[0] == 'y') && prop->level < LAND_MAX_LEVEL) {
                int32_t cost = g->cells[p->position].upgrade_cost;
                if (p->fund >= cost) {
                    p->fund -= cost;
                    ++prop->level;
                }
            }
            break;
        }
        case PROMPT_TOOL_SHOP: {
            int choice = (value[0] >= '0' && value[0] <= '9') ? (value[0] - '0') : -1;
            if (choice == 1) {
                if (p->credit >= 50 && item_total(p) < MAX_ITEM_TOTAL) {
                    p->credit -= 50;
                    ++p->items.BLOCK;
                }
            } else if (choice == 2) {
                if (p->credit >= 30 && item_total(p) < MAX_ITEM_TOTAL) {
                    p->credit -= 30;
                    ++p->items.ROBOT;
                }
            } else if (choice == 3) {
                if (p->credit >= 50 && item_total(p) < MAX_ITEM_TOTAL) {
                    p->credit -= 50;
                    ++p->items.BOMB;
                }
            }
            /* F/f 或非法输入：直接退出 */
            break;
        }
        case PROMPT_GIFT_SHOP: {
            int choice = (value[0] >= '0' && value[0] <= '9') ? (value[0] - '0') : -1;
            if (choice == 1) {
                p->fund += 2000;
            } else if (choice == 2) {
                p->credit += 200;
            } else if (choice == 3) {
                p->god_of_wealth_rounds = 5;
            }
            break;
        }
        default:
            break;
    }

    g->phase = PHASE_COMMAND;
    g->prompt = PROMPT_NONE;
    game_next_turn(g);
    return 0;
}

int game_query(const Game *g, char *buf, size_t bufsz)
{
    const PLAYER *p = game_current_player_c(g);
    int written;

    if (buf == NULL || bufsz == 0) {
        return -RC_INVALID_PARAMS;
    }
    if (p == NULL) {
        (void)snprintf(buf, bufsz, "无当前玩家");
        return 0;
    }

    written = snprintf(
        buf, bufsz,
        "玩家：%c 资金：%d 点数：%d 位置：%d 状态：%s 道具[路障%d 炸弹%d 娃娃%d]",
        p->id, (int)p->fund, (int)p->credit, (int)p->position,
        player_status_to_str(p->status),
        (int)p->items.BLOCK, (int)p->items.BOMB, (int)p->items.ROBOT);
    (void)written;
    return 0;
}

int game_help(char *buf, size_t bufsz)
{
    if (buf == NULL || bufsz == 0) {
        return -RC_INVALID_PARAMS;
    }
    (void)snprintf(
        buf, bufsz,
        "命令：ROLL 掷骰子 / STEP n 移动 n 步 / SELL n 出售地产 / "
        "BLOCK n 放路障 / BOMB n 放炸弹 / ROBOT 清除道具 / "
        "QUERY 查询资产 / HELP 帮助 / QUIT 退出");
    return 0;
}

int game_quit(Game *g)
{
    if (g == NULL) {
        return -RC_INVALID_PARAMS;
    }
    g->phase = PHASE_ENDED;
    g->status = GAME_FINISHED;
    g->quit = true;
    return 0;
}
