#include <assert.h>
#include <string.h>

#include "game.h"

static void prepare_player(Game *game, CellType cell_type)
{
    game_init(game);
    game->user_count = 1;
    game->current_index = 0;
    game->players[0].id = 'Q';
    game->players[0].status = NORMAL;
    game->players[0].position = 5;
    game->cells[5].type = cell_type;
}

static void test_mine_landing(void)
{
    Game game;
    prepare_player(&game, CELL_MINE);
    game.cells[5].mine_points = 80;

    game_settle_landing(&game, 5);

    assert(game.players[0].credit == 80);
}

static void test_jail_landing(void)
{
    Game game;
    prepare_player(&game, CELL_JAIL);

    game_settle_landing(&game, 5);

    assert(game.players[0].status == IMPRISONED);
    assert(game.players[0].remaining_rounds == JAIL_ROUNDS);
}

static void test_tool_shop_purchase(void)
{
    Game game;
    char message[1024];
    prepare_player(&game, CELL_TOOL_SHOP);
    game.players[0].credit = 100;

    game_settle_landing(&game, 5);
    assert(game.phase == PHASE_PROMPT);
    assert(game.prompt == PROMPT_TOOL_SHOP);

    (void)memset(message, 0, sizeof(message));
    assert(tool_shop_answer(&game, "2", message, sizeof(message)) == RC_OK);
    assert(game.players[0].credit == 70);
    assert(game.players[0].items.ROBOT == 1);
}

int main(void)
{
    test_mine_landing();
    test_jail_landing();
    test_tool_shop_purchase();
    return 0;
}
