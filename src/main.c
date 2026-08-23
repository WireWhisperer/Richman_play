#include <string.h>
#include "game.h"
#include "manual_ui.h"

/**
 * main.c —— 程序入口
 *
 * 在调用 game.h 中的各函数之前，先对其所依赖的全部结构体进行初始化：
 *   - Game      ：全局游戏状态
 *   - MapCell   ：地图格（Game.cells[]）
 *   - PLAYER    ：玩家（Game.players[]），其内含 ITEMS 背包
 *   - ITEMS     ：背包道具（BLOCK / BOMB / ROBOT）
 *   - Property  ：已购地产（Game.properties[]）
 *   - BoardItem ：地图道具（Game.board_items[]）
 */
int main(void)
{
    /* ===== Game：全局游戏状态 ===== */
    Game game;

    /* 先整体清零，保证所有成员（含嵌套数组）初始为 0 */
    memset(&game, 0, sizeof(game));

    /* ===== MapCell：地图格（共 MAP_SIZE 格） ===== */
    game.map_file[0] = '\0';          /* 尚未加载地图 */
    for (int32_t i = 0; i < MAP_SIZE; i++) {
        MapCell *cell = &game.cells[i];
        cell->type         = CELL_START;
        cell->price        = 0;
        cell->upgrade_cost = 0;
        cell->mine_points  = 0;
    }

    /* ===== PLAYER：玩家（含 ITEMS 背包），未加载 Preset 前无玩家 ===== */
    game.user_count    = 0;
    game.current_index = -1;          /* 无当前玩家 */
    for (int32_t i = 0; i < MAX_PLAYERS; i++) {
        PLAYER *p = &game.players[i];
        p->id                   = '?';
        p->fund                 = 0;
        p->credit               = 0;
        p->position             = 0;
        p->status               = NORMAL;
        p->remaining_rounds     = 0;
        p->items.BLOCK          = 0;
        p->items.BOMB           = 0;
        p->items.ROBOT          = 0;
        p->god_of_wealth_rounds = 0;
    }

    /* ===== Property：已购地产（动态数组） ===== */
    game.property_count = 0;
    for (int32_t i = 0; i < MAX_BOARD_ITEMS; i++) {
        game.properties[i].position    = -1;   /* 无效位置 */
        game.properties[i].owner_index = -1;   /* 无人拥有 */
        game.properties[i].level       = 0;    /* 空地 */
    }

    /* ===== BoardItem：地图道具（动态数组） ===== */
    game.board_item_count = 0;
    for (int32_t i = 0; i < MAX_BOARD_ITEMS; i++) {
        game.board_items[i].position = -1;
        game.board_items[i].kind     = ITEM_BLOCK;
    }

    /* ===== 预置骰子序列 ===== */
    game.dice_count = 0;
    game.dice_next  = 0;
    for (int32_t i = 0; i < MAX_DICE_SEQ; i++) {
        game.dice_seq[i] = 0;
    }

    /* ===== 阶段 / 状态 / 结果 ===== */
    game.phase        = PHASE_COMMAND;
    game.status       = GAME_RUNNING;
    game.prompt       = PROMPT_NONE;
    game.winner_index = -1;
    game.quit         = false;

    /* 初始化完成后，即可调用 game.h 中声明的游戏函数（如 game_load_map / game_reset 等） */
    return 0;
}
