/**
 * @file test_us03.c
 * @brief Automated acceptance and boundary tests for US03.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "player_setup.h"

static void assert_game_unchanged(const Game *expected, const Game *actual)
{
    if (expected == NULL || actual == NULL ||
        memcmp(expected, actual, sizeof(*expected)) != 0) {
        fputs("game state was modified unexpectedly\n", stderr);
        abort();
    }
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

static void test_sequence_21_uses_selection_order(void)
{
    Game game;

    game_init(&game);
    assert(player_setup_apply_sequence(&game, "21") == PLAYER_SETUP_OK);
    assert(game.user_count == 2);
    assert(game.players[0].id == 'A');
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

static void test_cli_reprompts_and_prints_summary(void)
{
    char output_text[4096];
    FILE *input;
    FILE *output;
    Game game;

    input = tmpfile();
    output = tmpfile();
    assert(input != NULL);
    assert(output != NULL);

    assert(fputs("1\n11\n12\n", input) >= 0);
    rewind(input);

    game_init(&game);
    assert(player_setup_run(&game, input, output) == PLAYER_SETUP_OK);
    assert(game.user_count == 2);
    assert(game.players[0].id == 'Q');
    assert(game.players[1].id == 'A');

    read_output(output, output_text, sizeof(output_text));
    assert(strstr(output_text, "请输入 2 到 4 位角色编号") != NULL);
    assert(strstr(output_text, "角色已选择，不能重复") != NULL);
    assert(strstr(output_text, "玩家 1：钱夫人（红色/Q）") != NULL);
    assert(strstr(output_text, "玩家 2：阿土伯（绿色/A）") != NULL);

    assert(fclose(input) == 0);
    assert(fclose(output) == 0);
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
    test_sequence_21_uses_selection_order();
    test_four_players_supported();
    test_invalid_count_does_not_modify_game();
    test_duplicate_character_does_not_modify_game();
    test_invalid_character_does_not_modify_game();
    test_cli_reprompts_and_prints_summary();
    test_cli_eof_does_not_modify_game();
    return 0;
}
