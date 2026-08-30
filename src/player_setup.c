/**
 * @file player_setup.c
 * @brief US03 player setup implementation.
 */

#include "player_setup.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
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

static int write_prompt(FILE *output, const char *prompt)
{
    return fputs(prompt, output) >= 0 && fflush(output) == 0;
}

static void trim_outer_whitespace(char *text)
{
    char *start = text;
    char *end;

    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1U);
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
}

static int trim_choices(char *text)
{
    size_t length;
    size_t index;
    size_t write_index = 0;

    trim_outer_whitespace(text);

    length = strlen(text);
    for (index = 0U; index < length; ++index) {
        if (!isspace((unsigned char)text[index])) {
            text[write_index++] = text[index];
        }
    }
    text[write_index] = '\0';
    return (int)write_index;
}

static int is_quit_command(const char *text)
{
    char cmd[16];
    size_t index = 0;
    size_t length = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[index] == ' ' || text[index] == '\t') {
        ++index;
    }

    while (text[index] != '\0' && text[index] != ' ' && text[index] != '\t' &&
           length + 1U < sizeof(cmd)) {
        cmd[length++] = (char)toupper((unsigned char)text[index++]);
    }
    cmd[length] = '\0';
    return strcmp(cmd, "QUIT") == 0;
}

static int parse_initial_fund(const char *text, int32_t *initial_fund)
{
    char *end;
    long value;

    if (text == NULL || initial_fund == NULL || text[0] == '\0') {
        return 0;
    }

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value < MANUAL_INITIAL_FUND_MIN ||
        value > MANUAL_INITIAL_FUND_MAX) {
        return 0;
    }

    *initial_fund = (int32_t)value;
    return 1;
}

static PlayerSetupStatus prompt_initial_fund(
    Game *game,
    FILE *input,
    FILE *output,
    int32_t *initial_fund
)
{
    char buffer[PLAYER_SETUP_INPUT_SIZE];

    for (;;) {
        LineReadStatus read_status;

        if (fprintf(
                output,
                "请输入初始资金（%d~%d，直接回车默认 %d，输入 QUIT 退出）：",
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
        if (read_status == LINE_READ_TOO_LONG) {
            if (fputs("输入过长，请重新输入。\n", output) < 0) {
                return PLAYER_SETUP_IO_ERROR;
            }
            continue;
        }

        trim_outer_whitespace(buffer);
        if (is_quit_command(buffer)) {
            game_quit(game);
            return PLAYER_SETUP_QUIT;
        }
        if (buffer[0] == '\0') {
            *initial_fund = MANUAL_INITIAL_FUND_DEFAULT;
        } else if (!parse_initial_fund(buffer, initial_fund)) {
            if (fprintf(
                    output,
                    "输入无效，请输入 %d~%d 之间的整数。\n",
                    MANUAL_INITIAL_FUND_MIN,
                    MANUAL_INITIAL_FUND_MAX
                ) < 0) {
                return PLAYER_SETUP_IO_ERROR;
            }
            continue;
        }

        if (fprintf(output, "初始资金已设为 %d。\n", (int)*initial_fund) < 0) {
            return PLAYER_SETUP_IO_ERROR;
        }
        return PLAYER_SETUP_OK;
    }
}

static const char *status_message(PlayerSetupStatus status)
{
    switch (status) {
        case PLAYER_SETUP_INVALID_COUNT:
            return "请输入 2 到 4 位角色编号（如 21 表示两名玩家：阿土伯、钱夫人）。";
        case PLAYER_SETUP_INVALID_CHARACTER:
            return "角色编号错误，每位须为 1~4。";
        case PLAYER_SETUP_DUPLICATE_CHARACTER:
            return "角色已选择，不能重复。";
        default:
            return "输入无效，请重新输入。";
    }
}

const PlayerSetupCharacter *player_setup_character(int selection)
{
    if (selection < 1 || selection > MAX_PLAYERS) {
        return NULL;
    }

    return &CHARACTERS[selection - 1];
}

const PlayerSetupCharacter *player_setup_character_by_id(char id)
{
    return character_by_id(id);
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
        case PLAYER_SETUP_QUIT:
            return "QUIT";
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
    int32_t initial_fund;
    PlayerSetupStatus fund_status;

    if (game == NULL || input == NULL || output == NULL) {
        return PLAYER_SETUP_IO_ERROR;
    }

    fund_status = prompt_initial_fund(
        game,
        input,
        output,
        &initial_fund
    );
    if (fund_status != PLAYER_SETUP_OK) {
        return fund_status;
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

    for (;;) {
        LineReadStatus read_status;
        PlayerSetupStatus status;

        if (!write_prompt(
                output,
                "请输入玩家及角色编号（2-4 位，如 21 表示阿土伯+钱夫人，输入 QUIT 退出）：")) {
            return PLAYER_SETUP_IO_ERROR;
        }

        read_status = read_line(input, buffer, sizeof(buffer));
        if (read_status == LINE_READ_EOF) {
            return PLAYER_SETUP_IO_ERROR;
        }
        if (read_status == LINE_READ_TOO_LONG) {
            if (fputs("输入过长，请重新输入。\n", output) < 0) {
                return PLAYER_SETUP_IO_ERROR;
            }
            continue;
        }

        (void)trim_choices(buffer);
        if (is_quit_command(buffer)) {
            game_quit(game);
            return PLAYER_SETUP_QUIT;
        }

        status = player_setup_apply_sequence(game, buffer);
        if (status == PLAYER_SETUP_OK) {
            if (game_apply_initial_fund(game, initial_fund) != RC_OK) {
                return PLAYER_SETUP_IO_ERROR;
            }
            return player_setup_print_summary(game, output);
        }

        if (fputs(status_message(status), output) < 0 ||
            fputc('\n', output) == EOF) {
            return PLAYER_SETUP_IO_ERROR;
        }
    }
}
