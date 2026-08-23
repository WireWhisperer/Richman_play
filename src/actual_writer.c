/**
 * @file actual_writer.c
 * @brief Actual JSON 导出实现（规范 10）。
 */

#include "actual_writer.h"

#include <string.h>

#include "cJSON.h"

static const char *id_str(char id, char buf[2])
{
    buf[0] = id;
    buf[1] = '\0';
    return buf;
}

cJSON *actual_writer_build(const Game *g)
{
    cJSON *actual;
    cJSON *users;
    cJSON *players;
    cJSON *properties;
    cJSON *map_items;
    cJSON *display;
    int32_t i;
    char idbuf[2];

    actual = cJSON_CreateObject();
    users = cJSON_CreateArray();
    players = cJSON_CreateArray();
    properties = cJSON_CreateArray();
    map_items = cJSON_CreateArray();
    display = cJSON_CreateArray();

    /* users */
    for (i = 0; i < g->user_count; ++i) {
        cJSON_AddItemToArray(users, cJSON_CreateString(id_str(g->players[i].id, idbuf)));
    }

    /* players */
    for (i = 0; i < g->user_count; ++i) {
        const PLAYER *p = &g->players[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON *items = cJSON_CreateObject();

        cJSON_AddStringToObject(obj, "id", id_str(p->id, idbuf));
        cJSON_AddNumberToObject(obj, "fund", p->fund);
        cJSON_AddNumberToObject(obj, "credit", p->credit);
        cJSON_AddNumberToObject(obj, "position", p->position);
        cJSON_AddStringToObject(obj, "status", player_status_to_str(p->status));
        cJSON_AddNumberToObject(obj, "remaining_rounds", p->remaining_rounds);
        cJSON_AddNumberToObject(items, "BLOCK", p->items.BLOCK);
        cJSON_AddNumberToObject(items, "BOMB", p->items.BOMB);
        cJSON_AddNumberToObject(items, "ROBOT", p->items.ROBOT);
        cJSON_AddItemToObject(obj, "items", items);
        cJSON_AddNumberToObject(obj, "god_of_wealth_rounds", p->god_of_wealth_rounds);

        cJSON_AddItemToArray(players, obj);
    }

    /* properties（按 position 升序） */
    for (i = 0; i < MAP_SIZE; ++i) {
        const Property *pr = game_property_at(g, i);
        if (pr != NULL) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "position", pr->position);
            cJSON_AddStringToObject(
                obj, "owner",
                (pr->owner_index >= 0 && pr->owner_index < g->user_count)
                    ? id_str(g->players[pr->owner_index].id, idbuf)
                    : "");
            cJSON_AddNumberToObject(obj, "level", pr->level);
            cJSON_AddItemToArray(properties, obj);
        }
    }

    /* map_items（按 position 升序） */
    for (i = 0; i < MAP_SIZE; ++i) {
        const BoardItem *item = game_board_item_at(g, i);
        if (item != NULL) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "position", item->position);
            cJSON_AddStringToObject(obj, "type", item_kind_to_str(item->kind));
            cJSON_AddItemToArray(map_items, obj);
        }
    }

    /* display_players（按 position 升序，规范 5.1） */
    for (i = 0; i < MAP_SIZE; ++i) {
        int32_t j;
        int visible = -1;
        for (j = 0; j < g->user_count; ++j) {
            if (g->players[j].status == BANKRUPT || g->players[j].position != i) {
                continue;
            }
            if (j == g->current_index) {
                visible = (int)j;
                break;
            }
            if (visible < 0) {
                visible = (int)j;
            }
        }
        if (visible >= 0) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "position", i);
            cJSON_AddStringToObject(obj, "visible_user", id_str(g->players[visible].id, idbuf));
            cJSON_AddItemToArray(display, obj);
        }
    }

    cJSON_AddItemToObject(actual, "users", users);
    cJSON_AddItemToObject(
        actual, "current_user",
        (g->current_index >= 0 && g->current_index < g->user_count)
            ? cJSON_CreateString(id_str(g->players[g->current_index].id, idbuf))
            : cJSON_CreateNull());
    cJSON_AddItemToObject(actual, "phase", cJSON_CreateString(phase_to_str(g->phase)));
    cJSON_AddItemToObject(
        actual, "pending_prompt",
        (g->phase == PHASE_PROMPT && g->prompt != PROMPT_NONE)
            ? cJSON_CreateString(prompt_to_str(g->prompt))
            : cJSON_CreateNull());
    cJSON_AddItemToObject(actual, "game_status", cJSON_CreateString(game_status_to_str(g->status)));
    cJSON_AddItemToObject(
        actual, "winner",
        (g->winner_index >= 0 && g->winner_index < g->user_count)
            ? cJSON_CreateString(id_str(g->players[g->winner_index].id, idbuf))
            : cJSON_CreateNull());
    cJSON_AddItemToObject(actual, "players", players);
    cJSON_AddItemToObject(actual, "properties", properties);
    cJSON_AddItemToObject(actual, "map_items", map_items);
    cJSON_AddItemToObject(actual, "display_players", display);

    return actual;
}
