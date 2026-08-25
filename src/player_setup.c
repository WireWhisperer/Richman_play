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

const PlayerSetupCharacter *player_setup_character_by_id(char id)
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

        character = player_setup_character_by_id(game->players[index].id);
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
    int initial_fund;

    if (game == NULL || input == NULL || output == NULL) {
        return PLAYER_SETUP_IO_ERROR;
    }

    for (;;) {
        LineReadStatus read_status;

        if (fprintf(
                output,
                "请输入初始资金（%d-%d，直接回车默认 %d）：",
                MANUAL_INITIAL_FUND_MIN,
                MANUAL_INITIAL_FUND_MAX,
                MANUAL_INITIAL_FUND_DEFAULT
            ) < 0 || fflush(output) != 0) {
            return PLAYER_SETUP_IO_ERROR;
        }

        read_status = read_line(input, buffer, sizeof(buffer));
        if (read_status == LINE_READ_EOF) {
            return PLAYER_SETUP_IO_ERROR;
        }
        if (read_status == LINE_READ_OK && buffer[0] == '\0') {
            initial_fund = MANUAL_INITIAL_FUND_DEFAULT;
            break;
        }
        if (read_status == LINE_READ_OK &&
            parse_number(buffer, MANUAL_INITIAL_FUND_MIN,
                         MANUAL_INITIAL_FUND_MAX, &initial_fund)) {
            break;
        }
        if (fprintf(
                output,
                "初始资金必须为 %d 到 %d，请重新输入。\n",
                MANUAL_INITIAL_FUND_MIN,
                MANUAL_INITIAL_FUND_MAX
            ) < 0) {
            return PLAYER_SETUP_IO_ERROR;
        }
    }

    if (fputs(
            "可选角色：\n"
            "1. 钱夫人（红色/Q）\n"
            "2. 阿土伯（绿色/A）\n"
            "3. 孙小美（蓝色/S）\n"
            "4. 金贝贝（黄色/J）\n"
            "请一次输入 2-4 个不重复的角色编号；输入顺序即游戏顺序。\n",
            output
        ) < 0) {
        return PLAYER_SETUP_IO_ERROR;
    }

    for (;;) {
        LineReadStatus read_status;
        PlayerSetupStatus status;

        if (!write_prompt(
                output,
                "请输入角色编号序列（例如 123、24、321、21）："
            )) {
            return PLAYER_SETUP_IO_ERROR;
        }
        read_status = read_line(input, buffer, sizeof(buffer));
        if (read_status == LINE_READ_EOF) {
            return PLAYER_SETUP_IO_ERROR;
        }
        status = read_status == LINE_READ_OK
            ? player_setup_apply_sequence(game, buffer)
            : PLAYER_SETUP_INVALID_COUNT;
        if (status == PLAYER_SETUP_OK) {
            break;
        }
        if (status == PLAYER_SETUP_INVALID_COUNT) {
            if (fputs("请输入 2 到 4 个角色编号。\n", output) < 0) {
                return PLAYER_SETUP_IO_ERROR;
            }
        } else if (status == PLAYER_SETUP_DUPLICATE_CHARACTER) {
            if (fputs("角色不能重复选择，请重新输入。\n", output) < 0) {
                return PLAYER_SETUP_IO_ERROR;
            }
        } else if (fputs("角色编号只能是 1 到 4，请重新输入。\n", output) < 0) {
            return PLAYER_SETUP_IO_ERROR;
        }
    }

    for (int32_t index = 0; index < game->user_count; ++index) {
        game->players[index].fund = initial_fund;
    }
    if (fprintf(output, "初始资金已设为 %d。\n", initial_fund) < 0) {
        return PLAYER_SETUP_IO_ERROR;
    }

    return player_setup_print_summary(game, output);
}
