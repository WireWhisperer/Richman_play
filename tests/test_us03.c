/**
 * @file test_us03.c
 * @brief Automated acceptance and boundary tests for US03.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "manual_ui.h"
#include "player_setup.h"

static void assert_game_unchanged(const Game *expected, const Game *actual)
{
    assert(memcmp(expected, actual, sizeof(*expected)) == 0);
}

static void read_output(FILE *stream, char *buffer, size_t buffer_size)
{
    size_t bytes_read;

    assert(stream != NULL);
    assert(buffer != NULL);
    assert(buffer_size > 0U);
    assert(fflush(stream) == 0);
    rewind(stream);

    bytes_read = fread(buffer, 1U, buffer_size - 1U, stream);
    buffer[bytes_read] = '\0';
}

static void test_character_metadata(void)
{
    const PlayerSetupCharacter *character;

    character = player_setup_character(1);
    assert(character != NULL);
    assert(character->id == 'Q');
    assert(strcmp(character->name, "钱夫人") == 0);
    assert(strcmp(character->color, "红色") == 0);

    character = player_setup_character(4);
    assert(character != NULL);
    assert(character->id == 'J');
    assert(strcmp(character->name, "金贝贝") == 0);
    assert(strcmp(character->color, "黄色") == 0);

    assert(player_setup_character(0) == NULL);
    assert(player_setup_character(5) == NULL);
    assert(player_setup_character_by_id('A') != NULL);
    assert(strcmp(player_setup_character_by_id('A')->name, "阿土伯") == 0);
    assert(player_setup_character_by_id('?') == NULL);
}

static void test_game_init_preserves_empty_state_contract(void)
{
    Game game;

    (void)memset(&game, 0x7f, sizeof(game));
    game_init(&game);

    assert(game.user_count == 0);
    assert(game.current_index == -1);
    assert(game.players[0].id == '?');
    assert(game.players[MAX_PLAYERS - 1].id == '?');
    assert(game.properties[0].position == -1);
    assert(game.properties[0].owner_index == -1);
    assert(game.board_items[0].position == -1);
    assert(game.phase == PHASE_COMMAND);
    assert(game.status == GAME_RUNNING);
    assert(game.prompt == PROMPT_NONE);
    assert(game.winner_index == -1);
    assert(!game.quit);
}

static void test_sequence_12_uses_selection_order(void)
{
    Game game;

    game_init(&game);
    assert(player_setup_apply_sequence(&game, "12") == PLAYER_SETUP_OK);
    assert(game.user_count == 2);
    assert(game.current_index == 0);
    assert(game.players[0].id == 'Q');
    assert(game.players[1].id == 'A');
}

static void test_sequence_31_uses_selection_order(void)
{
    Game game;

    game_init(&game);
    assert(player_setup_apply_sequence(&game, "31") == PLAYER_SETUP_OK);
    assert(game.user_count == 2);
    assert(game.players[0].id == 'S');
    assert(game.players[1].id == 'Q');
}

static void test_four_players_supported(void)
{
    Game game;

    game_init(&game);
    assert(player_setup_apply_sequence(&game, "1234") == PLAYER_SETUP_OK);
    assert(game.user_count == 4);
    assert(game.players[0].id == 'Q');
    assert(game.players[1].id == 'A');
    assert(game.players[2].id == 'S');
    assert(game.players[3].id == 'J');
}

static void test_invalid_count_does_not_modify_game(void)
{
    Game game;
    Game before;

    game_init(&game);
    before = game;
    assert(player_setup_apply_sequence(&game, "1") ==
           PLAYER_SETUP_INVALID_COUNT);
    assert_game_unchanged(&before, &game);

    assert(player_setup_apply_sequence(&game, "12341") ==
           PLAYER_SETUP_INVALID_COUNT);
    assert_game_unchanged(&before, &game);
}

static void test_duplicate_character_does_not_modify_game(void)
{
    Game game;
    Game before;

    game_init(&game);
    before = game;
    assert(player_setup_apply_sequence(&game, "11") ==
           PLAYER_SETUP_DUPLICATE_CHARACTER);
    assert_game_unchanged(&before, &game);
}

static void test_invalid_character_does_not_modify_game(void)
{
    Game game;
    Game before;

    game_init(&game);
    before = game;
    assert(player_setup_apply_sequence(&game, "1235") ==
           PLAYER_SETUP_INVALID_CHARACTER);
    assert_game_unchanged(&before, &game);

    assert(player_setup_apply_sequence(&game, "1#2") ==
           PLAYER_SETUP_INVALID_CHARACTER);
    assert_game_unchanged(&before, &game);
}

static void test_cli_selects_fund_then_character_sequence(void)
{
    char output_text[4096];
    const char *fund_prompt;
    const char *character_prompt;
    FILE *input;
    FILE *output;
    Game game;

    input = tmpfile();
    output = tmpfile();
    assert(input != NULL);
    assert(output != NULL);

    assert(fputs("999\n5000\n1\n11\n15\n321\n", input) >= 0);
    rewind(input);

    game_init(&game);
    assert(player_setup_run(&game, input, output) == PLAYER_SETUP_OK);
    assert(game.user_count == 3);
    assert(game.players[0].id == 'S');
    assert(game.players[1].id == 'A');
    assert(game.players[2].id == 'Q');
    assert(game.players[0].fund == 5000);
    assert(game.players[1].fund == 5000);
    assert(game.players[2].fund == 5000);

    read_output(output, output_text, sizeof(output_text));
    fund_prompt = strstr(output_text, "请输入初始资金");
    character_prompt = strstr(output_text, "请输入角色编号序列");
    assert(fund_prompt != NULL);
    assert(character_prompt != NULL);
    assert(fund_prompt < character_prompt);
    assert(strstr(output_text, "初始资金必须为 1000 到 50000") != NULL);
    assert(strstr(output_text, "请输入 2 到 4 个角色编号") != NULL);
    assert(strstr(output_text, "角色不能重复选择") != NULL);
    assert(strstr(output_text, "角色编号只能是 1 到 4") != NULL);
    assert(strstr(output_text, "初始资金已设为 5000") != NULL);
    assert(strstr(output_text, "玩家 1：孙小美（蓝色/S）") != NULL);
    assert(strstr(output_text, "玩家 2：阿土伯（绿色/A）") != NULL);
    assert(strstr(output_text, "玩家 3：钱夫人（红色/Q）") != NULL);

    assert(fclose(input) == 0);
    assert(fclose(output) == 0);
}

static void test_cli_empty_fund_uses_default(void)
{
    FILE *input = tmpfile();
    FILE *output = tmpfile();
    Game game;

    assert(input != NULL);
    assert(output != NULL);
    assert(fputs("\n24\n", input) >= 0);
    rewind(input);

    game_init(&game);
    assert(player_setup_run(&game, input, output) == PLAYER_SETUP_OK);
    assert(game.user_count == 2);
    assert(game.players[0].id == 'A');
    assert(game.players[1].id == 'J');
    assert(game.players[0].fund == MANUAL_INITIAL_FUND_DEFAULT);
    assert(game.players[1].fund == MANUAL_INITIAL_FUND_DEFAULT);

    assert(fclose(input) == 0);
    assert(fclose(output) == 0);
}

static void test_turn_prompt_uses_character_name(void)
{
    char prompt[64];
    Game game;

    game_init(&game);
    assert(player_setup_apply_sequence(&game, "21") == PLAYER_SETUP_OK);
    game.current_index = 0;
    game.phase = PHASE_COMMAND;
    assert(manual_ui_format_turn_prompt(&game, prompt, sizeof(prompt)) == RC_OK);
    assert(strcmp(prompt, "阿土伯> ") == 0);

    game_next_turn(&game);
    assert(game.current_index == 1);
    assert(manual_ui_format_turn_prompt(&game, prompt, sizeof(prompt)) == RC_OK);
    assert(strcmp(prompt, "钱夫人> ") == 0);

    game.phase = PHASE_PROMPT;
    assert(manual_ui_format_turn_prompt(&game, prompt, sizeof(prompt)) == RC_OK);
    assert(strcmp(prompt, "钱夫人(ANSWER)> ") == 0);
}

static void test_cli_eof_does_not_modify_game(void)
{
    FILE *input;
    FILE *output;
    Game game;
    Game before;

    input = tmpfile();
    output = tmpfile();
    assert(input != NULL);
    assert(output != NULL);

    game_init(&game);
    before = game;
    assert(player_setup_run(&game, input, output) == PLAYER_SETUP_IO_ERROR);
    assert_game_unchanged(&before, &game);

    assert(fclose(input) == 0);
    assert(fclose(output) == 0);
}

int main(void)
{
    test_character_metadata();
    test_game_init_preserves_empty_state_contract();
    test_sequence_12_uses_selection_order();
    test_sequence_31_uses_selection_order();
    test_four_players_supported();
    test_invalid_count_does_not_modify_game();
    test_duplicate_character_does_not_modify_game();
    test_invalid_character_does_not_modify_game();
    test_cli_selects_fund_then_character_sequence();
    test_cli_empty_fund_uses_default();
    test_turn_prompt_uses_character_name();
    test_cli_eof_does_not_modify_game();
    return 0;
}
