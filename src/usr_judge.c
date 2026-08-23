#include "game.h"

int game_move_to(Game *g, int32_t steps, int8_t last_position) /* 逐格移动+途中道具触发，返回最终落点 */
{
    int i;
    for (i = 0; i <= MAX_BOARD_ITEMS; i++)
    {
        if(g->board_items[i].position < last_position)
            continue;
        if (g->board_items[i].position > last_position)
        {
            if (g->board_items[i].position <= g->players[g->current_index].position)
                game_boarditem_suc(g, g->board_items[i], i);
            else
                game_settle_landing(g);
        }
    }
}

void game_boarditem_suc(Game *g, BoardItem b, int8_t index)   /*道具生效判定*/
{
    if(b.kind == ITEM_BOMB)
    {
        g->players[g->current_index].position = HOSPITAL_POS;  //玩家位置更新到医院
        g->players[g->current_index].status = HOSPITAL;        //玩家状态更新为住院
        game_remove_board_item(g, index);
        //此处缺少一个住院处理轮空函数
    }
    else if(b.kind == ITEM_BLOCK)
    {
        g->players[g->current_index].position = b.position;
        game_remove_board_item(g, index);
        printf("您已被路障阻隔在%d处！\n", b.position);
        game_settle_landing(g);
    }
}

void game_remove_board_item(Game *g, int index)         //清除道具
{
    int i;

    // 参数检查
    if (index < 0 || index >= g->board_item_count)
        return;

    // 后面的元素前移
    for (i = index; i < g->board_item_count - 1; i++)
    {
        g->board_items[i] = g->board_items[i + 1];
    }

    // 有效数量减少
    g->board_item_count--;
}