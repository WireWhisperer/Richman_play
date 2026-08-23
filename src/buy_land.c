#include "game.h"

/*
    判断当前位置属于哪个地段
    返回:
    1 地段一
    2 地段二
    3 地段三
    -1 非地产区域
*/
int get_land_type(int position)
{
    if (position >= 0 && position <= 27)
    {
        return 1;
    }

    else if (position >= 29 && position <= 34)
    {
        return 2;
    }

    else if (position >= 36 && position <= 62)
    {
        return 3;
    }

    return -1;
}

/*
    根据地段返回价格
*/
int get_land_price(int land_type)
{
    switch (land_type)
    {
    case 1:
        return 200;

    case 2:
        return 500;

    case 3:
        return 300;

    default:
        return 0;
    }
}

/*
    玩家确认购买地产

    返回:
    0 成功
    -1 资金不足
*/
int buy_land(Game *g)
{
    PLAYER *player;

    int position;
    int land_type;
    int price;

    // 获取当前玩家
    player = &g->players[g->current_index];

    position = player->position;

    land_type = get_land_type(position);

    if (land_type == -1)
    {
        return -1;
    }

    price = get_land_price(land_type);

    // 判断资金
    if (player->fund < price)
    {
        return -1;
    }

    /*
        扣钱
    */
    player->fund -= price;

    /*
        创建地产信息
    */

    Property new_property;

    new_property.position = position;

    // 当前玩家编号
    new_property.owner_index = g->current_index;

    // 初始等级
    new_property.level = 0;

    /*
        添加到地产数组
    */

    g->properties[g->property_count] = new_property;

    g->property_count++;

    return 0;
}

/*
    询问玩家是否购买
*/
void ask_buy_land(Game *g)
{
    PLAYER *player;

    int land_type;
    int price;

    char input;

    player = &g->players[g->current_index];

    land_type = get_land_type(player->position);

    // 不是地产
    if (land_type == -1)
    {
        return;
    }

    price = get_land_price(land_type);

    printf(
        "您位于空地，该空地价格为%d元，是否购买？(Y/N): ",
        price);

    scanf(" %c", &input);

    if (input == 'Y' || input == 'y')
    {

        if (buy_land(g) == 0)
        {
            printf("购买成功！\n");
        }

        else
        {
            printf("资金不足，购买失败！\n");
        }
    }

    else
    {
        printf("您放弃购买。\n");
    }
}