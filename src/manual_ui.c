/**
 * @file manual_ui.c
 * @brief 手动交互对局模式（规范 v1.1 第 1 节：不限制界面形式）
 *
 * 规则 8：manual_ui 只负责输入输出；业务规则在 game 模块。
 *
 * 界面始终显示完整地图（29×8 边缘布局，规范 3.2 顺时针 0~69）：
 *   - 地块编号是数据属性（cells[].position / map.json 的 position），界面不显示，
 *     后续资产显示模块按 position 查询即可
 *   - 特殊地块符号：S 起点 / H 医院 / T 道具屋 / G 礼品屋 / P 监狱 / M 魔法屋 / $ 矿地
 *   - 普通地产：未购显示 0；已购显示等级 0~3，颜色与业主一致
 *   - 玩家符号 Q/A/S/J 按角色颜色显示；同位多玩家时优先当前玩家（规范 5）
 *   - 地图道具：# 路障 / @ 炸弹（规范 3.4）
 * 每次操作后重绘整张地图。
 */
#include "manual_ui.h"

#include "console.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 终端颜色 ==================== */

enum { COL_DEF, COL_RED, COL_GREEN, COL_BLUE, COL_YELLOW };

static int player_color(char id)
{
    switch (id) {
    case 'Q': return COL_RED;
    case 'A': return COL_GREEN;
    case 'S': return COL_BLUE;
    case 'J': return COL_YELLOW;
    default:  return COL_DEF;
    }
}

static const char *ansi_of(int color)
{
    switch (color) {
    case COL_RED:    return "\033[31m";
    case COL_GREEN:  return "\033[32m";
    case COL_BLUE:   return "\033[34m";
    case COL_YELLOW: return "\033[33m";
    default:         return "\033[0m";
    }
}

/* ==================== 地图渲染 ==================== */

/**
 * 计算一个地块的显示内容：
 *   优先级：玩家符号 > 地图道具 > 已购地产等级 > 地块底色符号（规范 5 重叠规则）。
 */
static void cell_content(const Game *g, int32_t pos, char *main_ch, int *color)
{
    *main_ch = ' ';
    *color = COL_DEF;

    /* 玩家（规范 5）：同位多玩家时优先当前玩家，否则显示 users 顺序最靠前的未破产玩家 */
    const PLAYER *cur = game_current_player_c(g);
    const PLAYER *vis = NULL;
    for (int32_t i = 0; i < g->user_count; i++) {
        const PLAYER *p = &g->players[i];
        if (p->status == BANKRUPT || p->position != pos) {
            continue;
        }
        if (cur != NULL && p == cur) {
            vis = p;
            break;
        }
        if (vis == NULL) {
            vis = p;
        }
    }
    if (vis != NULL) {
        *main_ch = vis->id;
        *color = player_color(vis->id);
        return;
    }

    /* 地图道具（规范 3.4 地图标记：# 路障 / @ 炸弹） */
    const BoardItem *bi = game_board_item_at(g, pos);
    if (bi != NULL) {
        *main_ch = (bi->kind == ITEM_BLOCK) ? '#' : '@';
        return;
    }

    /* 已购地产：等级数字 0~3，颜色与业主一致 */
    const Property *pr = game_property_at(g, pos);
    if (pr != NULL && pr->owner_index >= 0 && pr->owner_index < g->user_count) {
        *color = player_color(g->players[pr->owner_index].id);
        *main_ch = (char)('0' + pr->level);
        return;
    }

    /* 地块底色符号；普通地产未购统一显示 0 */
    switch (g->cells[pos].type) {
    case CELL_START:       *main_ch = 'S'; break;
    case CELL_HOSPITAL:    *main_ch = 'H'; break;
    case CELL_TOOL_SHOP:   *main_ch = 'T'; break;
    case CELL_GIFT_SHOP:   *main_ch = 'G'; break;
    case CELL_JAIL:        *main_ch = 'P'; break;
    case CELL_MAGIC_HOUSE: *main_ch = 'M'; break;
    case CELL_MINE:        *main_ch = '$'; break;
    default:               *main_ch = '0'; break;
    }
}

/** 地图格：1 字符一格，所有格紧密相连、统一宽度 */
static void print_band_cell(const Game *g, int32_t pos)
{
    char m;
    int c;
    cell_content(g, pos, &m, &c);
    printf("%s%c\033[0m", ansi_of(c), m);
}

/**
 * 渲染完整地图（29×8 矩阵边缘，共 8 行 × 29 格，规范 3.2 顺时针）：
 *   第 0 行：0~28（左端 S 起点，右端 T 道具屋）
 *   第 1~6 行：左列 69~64、右列 29~34
 *   第 7 行：63~35（左端 M 魔法屋，右端 G 礼品屋）
 */
static void render_map(const Game *g)
{
    printf("\n");
    /* 第 0 行：上边 0~28 */
    for (int32_t p = 0; p <= 28; p++) {
        print_band_cell(g, p);
    }
    printf("\n");
    /* 第 1~6 行：左列 69~64，右列 29~34（中间 27 个空格对齐上下边） */
    for (int i = 0; i < 6; i++) {
        print_band_cell(g, 69 - i);
        for (int j = 1; j <= 27; j++) {
            printf(" ");
        }
        print_band_cell(g, 29 + i);
        printf("\n");
    }
    /* 第 7 行：下边 63~35（顺时针） */
    for (int32_t p = 63; p >= 35; p--) {
        print_band_cell(g, p);
    }
    printf("\n");
}

/* ==================== 输入辅助 ==================== */

/** 去掉首尾空白（空格/制表/换行/回车） */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    return s;
}

/** 严格整数解析：整串全为数字（可带负号），拒绝溢出和带尾巴（规则 13） */
static bool parse_int_arg(const char *s, int32_t *out)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        return false;
    }
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0') {
        return false;
    }
    if (v < INT32_MIN || v > INT32_MAX) {
        return false;
    }
    *out = (int32_t)v;
    return true;
}

/** 启动时提示输入初始资金：空行使用默认值，越界则重新输入 */
static int32_t prompt_initial_fund(void)
{
    char line[256];

    for (;;) {
        printf("请输入初始资金（%d~%d，直接回车默认 %d）：",
               MANUAL_INITIAL_FUND_MIN, MANUAL_INITIAL_FUND_MAX, MANUAL_INITIAL_FUND_DEFAULT);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            return MANUAL_INITIAL_FUND_DEFAULT;
        }

        char *s = trim(line);
        if (*s == '\0') {
            return MANUAL_INITIAL_FUND_DEFAULT;
        }

        int32_t fund = 0;
        if (!parse_int_arg(s, &fund) ||
            fund < MANUAL_INITIAL_FUND_MIN || fund > MANUAL_INITIAL_FUND_MAX) {
            printf("输入无效，请输入 %d~%d 之间的整数。\n",
                   MANUAL_INITIAL_FUND_MIN, MANUAL_INITIAL_FUND_MAX);
            continue;
        }
        return fund;
    }
}

/* ==================== 命令分发 ==================== */

/** 返回 0 成功；负数 = ResultCode；1 = QUIT 请求退出 */
static int dispatch(Game *g, const char *s)
{
    char cmd[16];
    size_t i = 0;
    while (s[i] == ' ' || s[i] == '\t') {
        i++;
    }
    size_t j = 0;
    while (s[i] != '\0' && s[i] != ' ' && s[i] != '\t' && j < sizeof(cmd) - 1) {
        cmd[j++] = (char)toupper((unsigned char)s[i++]);
    }
    cmd[j] = '\0';
    if (cmd[0] == '\0') {
        return RC_OK;
    }

    if (strcmp(cmd, "ROLL") == 0) {
        return game_roll(g);
    }
    if (strcmp(cmd, "STEP") == 0) {
        int32_t v;
        return parse_int_arg(s + i, &v) ? game_step(g, v) : RC_INVALID_PARAMS;
    }
    if (strcmp(cmd, "SELL") == 0) {
        int32_t v;
        return parse_int_arg(s + i, &v) ? game_sell(g, v) : RC_INVALID_PARAMS;
    }
    if (strcmp(cmd, "BLOCK") == 0) {
        int32_t v;
        return parse_int_arg(s + i, &v) ? game_block(g, v) : RC_INVALID_PARAMS;
    }
    if (strcmp(cmd, "BOMB") == 0) {
        int32_t v;
        return parse_int_arg(s + i, &v) ? game_bomb(g, v) : RC_INVALID_PARAMS;
    }
    if (strcmp(cmd, "ROBOT") == 0) {
        return game_robot(g);
    }
    if (strcmp(cmd, "QUERY") == 0) {
        char buf[1024];
        int rc = game_query(g, buf, sizeof(buf));
        if (rc == RC_OK) {
            printf("%s\n", buf);
        }
        return rc;
    }
    if (strcmp(cmd, "HELP") == 0) {
        char buf[2048];
        int rc = game_help(buf, sizeof(buf));
        if (rc == RC_OK) {
            printf("%s\n", buf);
        }
        return rc;
    }
    if (strcmp(cmd, "QUIT") == 0) {
        game_quit(g);
        return 1;
    }
    return RC_INVALID_COMMAND;
}

/* ==================== 主循环 ==================== */

int manual_ui_run(Game *g)
{
    console_init();

    int32_t initial_fund = prompt_initial_fund();
    int setup_rc = game_apply_initial_fund(g, initial_fund);
    if (setup_rc != RC_OK) {
        fprintf(stderr, "游戏初始化失败: %s\n", game_last_error());
        return setup_rc;
    }
    printf("初始资金已设为 %d。\n", initial_fund);

    char line[256];
    for (;;) {
        render_map(g);
        printf("%s> ", (g->phase == PHASE_PROMPT) ? "ANSWER" : "");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n（输入结束，退出）\n");
            return RC_OK;   /* EOF：不崩溃、不死循环（规则 13） */
        }

        char *s = trim(line);
        if (*s == '\0') {
            continue;
        }

        int rc = (g->phase == PHASE_PROMPT) ? game_answer(g, s) : dispatch(g, s);
        if (rc == 1) {
            break;   /* QUIT */
        }
        if (rc != RC_OK) {
            ResultCode code = (ResultCode)(-rc);
            printf("错误(%s): %s\n", result_code_name(code), game_last_error());
        }
    }
    return RC_OK;
}
