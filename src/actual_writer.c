/**
 * @file actual_writer.c
 * @brief Actual 实际结果导出（规范 v1.1 第 10 节）
 */
#include "actual_writer.h"
#include "file_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *display_players_build(const Game *g)
{
    /* 规范 5.1：
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

cJSON *actual_build(const Game *g, const char *case_id)
{
    /* 规范 10：完整输出 { schema_version, case_id, actual: {...} } */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "schema_version", "1.0");
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
    cJSON_AddStringToObject(actual, "current_user",
                            cur != NULL ? (char[]){ cur->id, '\0' } : NULL);
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

    /* players：按 users 数组顺序输出（规范 10.1） */
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
        cJSON_AddNumberToObject(pj, "remaining_rounds", p->remaining_rounds);
        cJSON *items = cJSON_CreateObject();
        cJSON_AddNumberToObject(items, "BLOCK", p->items.BLOCK);
        cJSON_AddNumberToObject(items, "BOMB", p->items.BOMB);
        cJSON_AddNumberToObject(items, "ROBOT", p->items.ROBOT);
        cJSON_AddItemToObject(pj, "items", items);
        cJSON_AddNumberToObject(pj, "god_of_wealth_rounds", p->god_of_wealth_rounds);
        cJSON_AddItemToArray(players, pj);
    }
    cJSON_AddItemToObject(actual, "players", players);

    /* properties / map_items：按 position 升序（规范 10.1；Game 内已维护该不变量） */
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

    /* display_players（规范 5.1 可选标准显示状态；本项目统一输出） */
    cJSON_AddItemToObject(actual, "display_players", display_players_build(g));

    return root;
}

char *actual_serialize(const Game *g, const char *case_id)
{
    cJSON *root = actual_build(g, case_id);
    /* cJSON_PrintUnformatted 原样输出 UTF-8 字节（等价 ensure_ascii=False，规范 2.3） */
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

int actual_write_file(const Game *g, const char *case_id, const char *path)
{
    char *json = actual_serialize(g, case_id);
    if (json == NULL) {
        return RC_IO_ERROR;
    }
    int rc = fu_write_file(path, json);
    free(json);
    return rc;
}
