#include "game.h"

void ask_ROLL(Game *g)
{
    char command[20];
    int32_t move;
    int last_position;
    (void)g;

    scanf("%19s", command);

    // 转换为大写，方便判断
    for (int i = 0; command[i] != '\0'; i++)
    {
        command[i] = toupper(command[i]);
    }

    // 判断是否输入ROLL
    if (strcmp(command, "ROLL") == 0)
    {
        move = game_roll(g);
        last_position = game_step(g, move);
        game_move_to(g, move, last_position);
    }
}

void ask_STEP(Game *g)
{
    char command[20];
    int steps;
    int last_position;

    scanf("%19s", command);

    // 转换为大写
    for (int i = 0; command[i] != '\0'; i++)
    {
        command[i] = toupper(command[i]);
    }

    // 判断 STEP
    if (strcmp(command, "STEP") == 0)
    {
        scanf("%d", &steps);

        last_position = game_step(g, steps);

        game_move_to(g, steps, last_position);
    }
}