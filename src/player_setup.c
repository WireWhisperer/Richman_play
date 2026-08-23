/**
 * @file player_setup.c
 * @brief US03 player setup implementation.
 */

#include "player_setup.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define PLAYER_SETUP_INPUT_SIZE 64

typedef enum {
    LINE_READ_OK = 0,
    LINE_READ_EOF,
    LINE_READ_TOO_LONG
} LineReadStatus;

static const PlayerSetupCharacter CHARACTERS[MAX_PLAYERS] = {
    {1, 'Q', "钱夫人", "红色"},
    {2, 'A', "阿土伯", "绿色"},
    {3, 'S', "孙小美", "蓝色"},
    {4, 'J', "金贝贝", "黄色"}
};

static const PlayerSetupCharacter *character_by_id(char id)
{
    int index;

    for (index = 0; index < MAX_PLAYERS; ++index) {
        if (CHARACTERS[index].id == id) {
            return &CHARACTERS[index];
        }
    }

    return NULL;
}

static LineReadStatus read_line(FILE *input, char *buffer, size_t buffer_size)
{
    char *newline;
    int character;

    if (fgets(buffer, (int)buffer_size, input) == NULL) {
        return LINE_READ_EOF;
    }

    newline = strchr(buffer, '\n');
    if (newline != NULL) {
        *newline = '\0';
        if (newline > buffer && newline[-1] == '\r') {
            newline[-1] = '\0';
        }
        return LINE_READ_OK;
    }

    if (feof(input)) {
        return LINE_READ_OK;
    }

    do {
        character = fgetc(input);
    } while (character != '\n' && character != EOF);

    return LINE_READ_TOO_LONG;
}

static int parse_number(const char *text, long minimum, long maximum, int *value)
{
    char *end;
    long parsed;

    while (isspace((unsigned char)*text)) {
        ++text;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (text == end || errno == ERANGE) {
        return 0;
    }

    while (isspace((unsigned char)*end)) {
        ++end;
    }

    if (*end != '\0' || parsed < minimum || parsed > maximum) {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

static int choice_already_selected(const char *choices, int count, int choice)
{
    int index;

    for (index = 0; index < count; ++index) {
        if (choices[index] == (char)('0' + choice)) {
            return 1;
        }
    }

    return 0;
}

static int write_prompt(FILE *output, const char *prompt)
{
    return fputs(prompt, output) >= 0 && fflush(output) == 0;
}

const PlayerSetupCharacter *player_setup_character(int selection)
{
    if (selection < 1 || selection > MAX_PLAYERS) {
        return NULL;
    }

    return &CHARACTERS[selection - 1];
}

const char *player_setup_status_name(PlayerSetupStatus status)
{
    switch (status) {
        case PLAYER_SETUP_OK:
            return "OK";
        case PLAYER_SETUP_INVALID_COUNT:
            return "INVALID_COUNT";
        case PLAYER_SETUP_INVALID_CHARACTER:
            return "INVALID_CHARACTER";
        case PLAYER_SETUP_DUPLICATE_CHARACTER:
            return "DUPLICATE_CHARACTER";
        case PLAYER_SETUP_IO_ERROR:
            return "IO_ERROR";
        default:
            return "UNKNOWN";
    }
}

PlayerSetupStatus player_setup_apply_sequence(Game *game, const char *choices)
{
    size_t count;
    size_t index;
    int selected[MAX_PLAYERS] = {0, 0, 0, 0};

    if (game == NULL || choices == NULL) {
        return PLAYER_SETUP_INVALID_CHARACTER;
    }

    count = strlen(choices);
    if (count < (size_t)PLAYER_SETUP_MIN_PLAYERS ||
        count > (size_t)PLAYER_SETUP_MAX_PLAYERS) {
        return PLAYER_SETUP_INVALID_COUNT;
    }

    for (index = 0U; index < count; ++index) {
        int selection;

        if (choices[index] < '1' || choices[index] > '4') {
            return PLAYER_SETUP_INVALID_CHARACTER;
        }

        selection = choices[index] - '1';
        if (selected[selection] != 0) {
            return PLAYER_SETUP_DUPLICATE_CHARACTER;
        }
        selected[selection] = 1;
    }

    for (index = 0U; index < (size_t)MAX_PLAYERS; ++index) {
        game->players[index].id = '?';
    }
    for (index = 0U; index < count; ++index) {
        game->players[index].id = CHARACTERS[choices[index] - '1'].id;
    }

    game->user_count = (int32_t)count;
    game->current_index = 0;
    return PLAYER_SETUP_OK;
}

PlayerSetupStatus player_setup_print_summary(const Game *game, FILE *output)
{
    int32_t index;

    if (game == NULL || output == NULL) {
        return PLAYER_SETUP_IO_ERROR;
    }
    if (game->user_count < PLAYER_SETUP_MIN_PLAYERS ||
        game->user_count > PLAYER_SETUP_MAX_PLAYERS) {
        return PLAYER_SETUP_INVALID_COUNT;
    }

    if (fprintf(output, "\n玩家信息（游戏顺序）：\n") < 0) {
        return PLAYER_SETUP_IO_ERROR;
    }

    for (index = 0; index < game->user_count; ++index) {
        const PlayerSetupCharacter *character;

        character = character_by_id(game->players[index].id);
        if (character == NULL) {
            return PLAYER_SETUP_INVALID_CHARACTER;
        }

        if (fprintf(
                output,
                "玩家 %d：%s（%s/%c）\n",
                (int)(index + 1),
                character->name,
                character->color,
                character->id
            ) < 0) {
            return PLAYER_SETUP_IO_ERROR;
        }
    }

    return fflush(output) == 0 ? PLAYER_SETUP_OK : PLAYER_SETUP_IO_ERROR;
}

PlayerSetupStatus player_setup_run(Game *game, FILE *input, FILE *output)
{
    char buffer[PLAYER_SETUP_INPUT_SIZE];
    char choices[MAX_PLAYERS + 1] = {0};
    int player_count;
    int player_index;

    if (game == NULL || input == NULL || output == NULL) {
        return PLAYER_SETUP_IO_ERROR;
    }

    for (;;) {
        LineReadStatus read_status;

        if (!write_prompt(output, "请输入玩家数量（2-4）：")) {
            return PLAYER_SETUP_IO_ERROR;
        }

        read_status = read_line(input, buffer, sizeof(buffer));
        if (read_status == LINE_READ_EOF) {
            return PLAYER_SETUP_IO_ERROR;
        }
        if (read_status == LINE_READ_OK &&
            parse_number(buffer, PLAYER_SETUP_MIN_PLAYERS,
                         PLAYER_SETUP_MAX_PLAYERS, &player_count)) {
            break;
        }

        if (fputs("玩家数量必须为 2 到 4，请重新输入。\n", output) < 0) {
            return PLAYER_SETUP_IO_ERROR;
        }
    }

    if (fputs(
            "可选角色：\n"
            "1. 钱夫人（红色/Q）\n"
            "2. 阿土伯（绿色/A）\n"
            "3. 孙小美（蓝色/S）\n"
            "4. 金贝贝（黄色/J）\n",
            output
        ) < 0) {
        return PLAYER_SETUP_IO_ERROR;
    }

    for (player_index = 0; player_index < player_count; ++player_index) {
        for (;;) {
            LineReadStatus read_status;
            int choice;

            if (fprintf(
                    output,
                    "玩家 %d 请选择角色编号（1-4）：",
                    player_index + 1
                ) < 0 || fflush(output) != 0) {
                return PLAYER_SETUP_IO_ERROR;
            }

            read_status = read_line(input, buffer, sizeof(buffer));
            if (read_status == LINE_READ_EOF) {
                return PLAYER_SETUP_IO_ERROR;
            }
            if (read_status != LINE_READ_OK ||
                !parse_number(buffer, 1, MAX_PLAYERS, &choice)) {
                if (fputs("角色编号错误，请重新输入。\n", output) < 0) {
                    return PLAYER_SETUP_IO_ERROR;
                }
                continue;
            }
            if (choice_already_selected(choices, player_index, choice)) {
                if (fputs("角色已选择，请重新选择。\n", output) < 0) {
                    return PLAYER_SETUP_IO_ERROR;
                }
                continue;
            }

            choices[player_index] = (char)('0' + choice);
            break;
        }
    }

    choices[player_count] = '\0';
    {
        PlayerSetupStatus status = player_setup_apply_sequence(game, choices);
        if (status != PLAYER_SETUP_OK) {
            return status;
        }
    }

    return player_setup_print_summary(game, output);
}
