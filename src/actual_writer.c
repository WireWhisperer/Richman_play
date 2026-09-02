/**
 * @file actual_writer.c
 * @brief Actual 实际结果导出（规范 v2.0 第 12/13 节）
 */
#include "actual_writer.h"
#include "file_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *display_players_build(const Game *g)
{
    /* 规范 5/13：
       每个被至少一名未破产玩家占用的位置输出一条记录，按 position 升序；
       若当前玩家位于该位置，visible_user 必须是当前玩家；
       若当前玩家不在该位置，则显示 users 数组中顺序最靠前的玩家。 */
    cJSON *arr = cJSON_CreateArray();
    const PLAYER *cur = game_current_player_c(g);

    for (int32_t pos = 0; pos < MAP_SIZE; pos++) {
        /* 找该位置 users 顺序最靠前的未破产玩家 */
        const PLAYER *first = NULL;
        for (int32_t i = 0; i < g->user_count; i++) {
            const PLAYER *p = &g->players[i];
            if (p->status != BANKRUPT && p->position == pos) {
                first = p;
                break;
            }
        }
        if (first == NULL) {
            continue;
        }
        const PLAYER *visible = first;
        if (cur != NULL && cur->status != BANKRUPT && cur->position == pos) {
            visible = cur;
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "position", pos);
        char idbuf[2] = { visible->id, '\0' };
        cJSON_AddStringToObject(item, "visible_user", idbuf);
        cJSON_AddItemToArray(arr, item);
    }
    return arr;
}

/** 地块底层符号（与手动界面一致；普通地产未购/已购均显示等级数字） */
static char base_symbol_of(const Game *g, int32_t pos)
{
    const Property *pr = game_property_at(g, pos);

    if (pr != NULL) {
        return (char)('0' + pr->level);
    }
    switch (g->cells[pos].type) {
    case CELL_START:     return 'S';
    case CELL_TOOL_SHOP: return 'T';
    case CELL_GIFT_SHOP: return 'G';
    case CELL_PARK:      return 'P';
    case CELL_MINE:      return '$';
    default:             return '0';
    }
}

/**
 * display_cells（规范 13）：每格输出
 * { position, base_type, base_symbol, visible_symbol, visible_entity }。
 * 覆盖优先级：PLAYER > FORTUNE > MAP_ITEM > BASE（规范 13 visible_entity 表）。
 */
cJSON *display_cells_build(const Game *g)
{
    cJSON *arr = cJSON_CreateArray();

    for (int32_t pos = 0; pos < MAP_SIZE; pos++) {
        cJSON *item = cJSON_CreateObject();
        const char *entity = "BASE";
        char symbol = base_symbol_of(g, pos);

        /* 覆盖优先级：PLAYER > FORTUNE > MAP_ITEM > BASE（与手动界面一致） */
        const BoardItem *bi = game_board_item_at(g, pos);
        if (bi != NULL) {
            symbol = '#';
            entity = "MAP_ITEM";
        }
        /* 财神覆盖（规范 13：F；玩家与财神理论上不同格——进入即领取） */
        if (g->fortune.position == pos) {
            symbol = 'F';
            entity = "FORTUNE";
        }
        /* 玩家覆盖 */
        const PLAYER *cur = game_current_player_c(g);
        const PLAYER *vis = NULL;
        for (int32_t i = 0; i < g->user_count; i++) {
            const PLAYER *p = &g->players[i];
            if (p->status != BANKRUPT && p->position == pos) {
                vis = p;
                break;
            }
        }
        if (vis != NULL) {
            if (cur != NULL && cur->status != BANKRUPT && cur->position == pos) {
                vis = cur;
            }
            symbol = vis->id;
            entity = "PLAYER";
        }

        cJSON_AddNumberToObject(item, "position", pos);
        cJSON_AddStringToObject(item, "base_type",
                                cell_type_to_str(g->cells[pos].type));
        char basebuf[2] = { base_symbol_of(g, pos), '\0' };
        cJSON_AddStringToObject(item, "base_symbol", basebuf);
        char symbuf[2] = { symbol, '\0' };
        cJSON_AddStringToObject(item, "visible_symbol", symbuf);
        cJSON_AddStringToObject(item, "visible_entity", entity);
        cJSON_AddItemToArray(arr, item);
    }
    return arr;
}

cJSON *actual_build(const Game *g, const char *case_id, const char *schema_version)
{
    /* 规范 12：完整输出 { schema_version, case_id, actual: {...} } */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "schema_version",
                            schema_version != NULL ? schema_version : "2.0");
    cJSON_AddStringToObject(root, "case_id", case_id);

    cJSON *actual = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "actual", actual);

    /* users：数组顺序 = 回合顺序（规范 3.1） */
    cJSON *users = cJSON_CreateArray();
    for (int32_t i = 0; i < g->user_count; i++) {
        char idbuf[2] = { g->players[i].id, '\0' };
        cJSON_AddItemToArray(users, cJSON_CreateString(idbuf));
    }
    cJSON_AddItemToObject(actual, "users", users);

    const PLAYER *cur = game_current_player_c(g);
    if (cur != NULL) {
        char idbuf[2] = { cur->id, '\0' };
        cJSON_AddStringToObject(actual, "current_user", idbuf);
    } else {
        cJSON_AddNullToObject(actual, "current_user");
    }
    cJSON_AddStringToObject(actual, "phase", phase_to_str(g->phase));
    cJSON_AddItemToObject(actual, "pending_prompt",
                          g->prompt == PROMPT_NONE
                              ? cJSON_CreateNull()
                              : cJSON_CreateString(prompt_to_str(g->prompt)));
    cJSON_AddStringToObject(actual, "game_status", game_status_to_str(g->status));
    if (g->winner_index >= 0 && g->winner_index < g->user_count) {
        char wbuf[2] = { g->players[g->winner_index].id, '\0' };
        cJSON_AddStringToObject(actual, "winner", wbuf);
    } else {
        cJSON_AddNullToObject(actual, "winner");
    }
    cJSON_AddNumberToObject(actual, "turn_number", g->turn_number);

    /* players：按 users 数组顺序输出（规范 12）；v2.0 无 remaining_rounds/BOMB */
    cJSON *players = cJSON_CreateArray();
    for (int32_t i = 0; i < g->user_count; i++) {
        const PLAYER *p = &g->players[i];
        cJSON *pj = cJSON_CreateObject();
        char idbuf[2] = { p->id, '\0' };
        cJSON_AddStringToObject(pj, "id", idbuf);
        cJSON_AddNumberToObject(pj, "fund", p->fund);
        cJSON_AddNumberToObject(pj, "credit", p->credit);
        cJSON_AddNumberToObject(pj, "position", p->position);
        cJSON_AddStringToObject(pj, "status", player_status_to_str(p->status));
        cJSON *items = cJSON_CreateObject();
        cJSON_AddNumberToObject(items, "BLOCK", p->items.BLOCK);
        cJSON_AddNumberToObject(items, "ROBOT", p->items.ROBOT);
        cJSON_AddItemToObject(pj, "items", items);
        cJSON_AddNumberToObject(pj, "god_of_wealth_rounds", p->god_of_wealth_rounds);
        cJSON_AddItemToArray(players, pj);
    }
    cJSON_AddItemToObject(actual, "players", players);

    /* properties / map_items：按 position 升序（规范 12；Game 内已维护该不变量） */
    cJSON *props = cJSON_CreateArray();
    for (int32_t i = 0; i < g->property_count; i++) {
        const Property *p = &g->properties[i];
        cJSON *pj = cJSON_CreateObject();
        cJSON_AddNumberToObject(pj, "position", p->position);
        char obuf[2] = { g->players[p->owner_index].id, '\0' };
        cJSON_AddStringToObject(pj, "owner", obuf);
        cJSON_AddNumberToObject(pj, "level", p->level);
        cJSON_AddItemToArray(props, pj);
    }
    cJSON_AddItemToObject(actual, "properties", props);

    cJSON *bits = cJSON_CreateArray();
    for (int32_t i = 0; i < g->board_item_count; i++) {
        const BoardItem *b = &g->board_items[i];
        cJSON *bj = cJSON_CreateObject();
        cJSON_AddNumberToObject(bj, "position", b->position);
        cJSON_AddStringToObject(bj, "type", item_kind_to_str(b->kind));
        cJSON_AddItemToArray(bits, bj);
    }
    cJSON_AddItemToObject(actual, "map_items", bits);

    /* fortune（规范 12）：完整生命周期 */
    {
        cJSON *f = cJSON_CreateObject();
        if (g->fortune.position >= 0) {
            cJSON_AddNumberToObject(f, "position", g->fortune.position);
            cJSON_AddStringToObject(f, "symbol", "F");
        } else {
            cJSON_AddNullToObject(f, "position");
            cJSON_AddNullToObject(f, "symbol");
        }
        if (g->fortune.spawned_after_turn > 0) {
            cJSON_AddNumberToObject(f, "spawned_after_turn",
                                    g->fortune.spawned_after_turn);
        } else {
            cJSON_AddNullToObject(f, "spawned_after_turn");
        }
        cJSON_AddNumberToObject(f, "remaining_map_turns",
                                g->fortune.remaining_map_turns);
        if (g->fortune.next_spawn_after_turn > 0) {
            cJSON_AddNumberToObject(f, "next_spawn_after_turn",
                                    g->fortune.next_spawn_after_turn);
        } else {
            cJSON_AddNullToObject(f, "next_spawn_after_turn");
        }
        cJSON_AddItemToObject(actual, "fortune", f);
    }

    /* display_players / display_cells（规范 13） */
    cJSON_AddItemToObject(actual, "display_players", display_players_build(g));
    cJSON_AddItemToObject(actual, "display_cells", display_cells_build(g));

    return root;
}

char *actual_serialize(const Game *g, const char *case_id, const char *schema_version)
{
    cJSON *root = actual_build(g, case_id, schema_version);
    /* cJSON_PrintUnformatted 原样输出 UTF-8 字节 */
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

int actual_write_file(const Game *g, const char *case_id, const char *schema_version,
                      const char *path)
{
    char *json = actual_serialize(g, case_id, schema_version);
    if (json == NULL) {
        return RC_IO_ERROR;
    }
    int rc = fu_write_file(path, json);
    free(json);
    return rc;
}
