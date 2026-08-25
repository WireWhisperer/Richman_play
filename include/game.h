/**
 * @file game.h
 * @brief 大富翁游戏核心状态与规则 —— 规范 v1.1 第 3/4/5/9 节
 *
 * 只定义"外部可观察行为"所需的数据结构与接口，
 * 不限制内部实现细节（规范 2 跨语言兼容原则）。
 */
#ifndef RICH_GAME_H
#define RICH_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct cJSON;

/* ===== 基础常量（规范 3.x / 4.x） ===== */
#define MAP_SIZE            70    /* 地图合法编号 0~69，69 的下一格为 0 */
#define MAX_PLAYERS          4    /* 每局 2~4 名玩家 */
#define MAX_ITEM_TOTAL      10    /* 每名玩家背包道具总数上限 */
#define LAND_MAX_LEVEL       3    /* 地产最高等级：摩天楼 */
#define HOSPITAL_ROUNDS      3    /* 住院轮空初始值（规范 3.5） */
#define JAIL_ROUNDS          2    /* 监狱轮空初始值（规范 3.5） */
#define BLOCK_OFFSET_LIMIT  10    /* BLOCK/BOMB 的 offset 范围 -10~10 */
#define ROBOT_CLEAR_RANGE   10    /* 机器娃娃清除前方 1~10 格 */
#define DICE_MIN             1
#define DICE_MAX             6
#define USER_ID_MAX         16    /* 玩家标识字符串上限 */
#define MAX_DICE_SEQ      1024    /* 预置骰子序列上限 */
#define MAX_BOARD_ITEMS    100    /* 地图道具/地产动态数组容量上限 */
#define HOSPITAL_POS        14    /*医院位置*/
#define JAIL_POS            49    /*监狱位置*/
#define TOOL_POS            28    /*道具屋位置*/
#define GIFT_POS            35    /*礼品屋位置*/
#define MANUAL_INITIAL_FUND_DEFAULT 10000
#define MANUAL_INITIAL_FUND_MIN      1000
#define MANUAL_INITIAL_FUND_MAX     50000

/* ===== 统一错误码（规范 13） ===== */
typedef enum {
    RC_OK = 0,                    /* 无错误 */
    RC_INVALID_JSON,              /* JSON 无法解析 */
    RC_UNSUPPORTED_VERSION,       /* 不支持 schema_version */
    RC_INVALID_PRESET,            /* Preset 字段缺失或状态冲突 */
    RC_INVALID_MAP,               /* 地图文件错误 */
    RC_INVALID_COMMAND,           /* 不支持的 command */
    RC_INVALID_PARAMS,            /* Action 参数缺失、类型错误或越界 */
    RC_INVALID_PHASE,             /* 当前阶段不允许执行该 Action */
    RC_DICE_SEQUENCE_EMPTY,       /* ROLL 时预置骰子不足 */
    RC_ACTION_AFTER_END,          /* 游戏结束后仍有 Action */
    RC_ASSERT_NOT_EQUAL,          /* Actual 与 Expected 不相等 */
    RC_ASSERT_NOT_FOUND,          /* Expected 要求的对象不存在 */
    RC_ASSERT_NOT_ABSENT,         /* 应不存在的对象实际存在 */
    RC_IO_ERROR,                  /* 文件读写失败 */
    RC_INTERNAL                   /* 未实现/内部错误 */
} ResultCode;

const char *result_code_name(ResultCode rc);    /* 输出 "INVALID_PARAMS" 等规范错误码文本 */

/* ===== 枚举（与 JSON 字符串的固定映射见 game.c，规范 14.2） ===== */
typedef enum {
    CELL_START = 0,     /* 起点 */
    CELL_LAND_1,        /* 地段一普通地产 */
    CELL_LAND_2,        /* 地段二普通地产 */
    CELL_LAND_3,        /* 地段三普通地产 */
    CELL_TOOL_SHOP,     /* 道具屋 */
    CELL_GIFT_SHOP,     /* 礼品屋 */
    CELL_MAGIC_HOUSE,   /* 魔法屋 */
    CELL_HOSPITAL,      /* 医院 */
    CELL_JAIL,          /* 监狱 */
    CELL_MINE,          /* 矿地 */
    CELL_KIND_COUNT
} CellType;

typedef enum { ITEM_BLOCK, ITEM_BOMB, ITEM_ROBOT, ITEM_KIND_COUNT } ItemKind;
typedef enum { PHASE_COMMAND, PHASE_PROMPT, PHASE_ENDED } GamePhase;
typedef enum { GAME_RUNNING, GAME_FINISHED } GameStatus;
typedef enum {
    PROMPT_NONE, PROMPT_BUY, PROMPT_UPGRADE, PROMPT_TOOL_SHOP, PROMPT_GIFT_SHOP
} PromptType;

/**
 * 玩家状态（规范 3.5）。三语言统一命名：
 * IMPRISONED 即规范中的 JAIL（入狱轮空），JSON 输出字符串仍为 "JAIL"。
 */
typedef enum player_status {
    NORMAL,       /* 可以正常行动 */
    HOSPITAL,     /* 住院并轮空，初始 3 轮 */
    BANKRUPT,     /* 已经破产 */
    IMPRISONED    /* 监狱并轮空，初始 2 轮（JSON: "JAIL"） */
} PLAYER_STATUS;

/** 玩家背包道具（规范 3.4），三类合计不超过 MAX_ITEM_TOTAL */
typedef struct items {
    int8_t BLOCK;   /* 路障 */
    int8_t BOMB;    /* 炸弹 */
    int8_t ROBOT;   /* 机器娃娃 */
} ITEMS;

/* ===== 数据结构 ===== */

/** 地图格（由统一的 map.json 确定，规范 3.2） */
typedef struct {
    CellType type;
    int32_t  price;         /* 购买价格（LAND_1=200 LAND_2=500 LAND_3=300） */
    int32_t  upgrade_cost;  /* 每次升级费用 */
    int32_t  mine_points;   /* MINE 每次到达增加的点数（地图配置） */
} MapCell;

/**
 * 玩家（users 数组顺序即回合顺序，规范 3.1）。
 * fund/credit 为 int32_t：规范 2.1 要求整数限 int32 范围，
 * 解析时先过 int64 校验再存入，int16 会截断合法值。
 */
typedef struct player {
    char          id;                 /* 角色标识：Q 钱夫人 / A 阿土伯 / S 孙小美 / J 金贝贝 */
    int32_t       fund;
    int32_t       credit;             /* 点数 */
    int8_t        position;           /* 逻辑位置 0~69，允许重叠（规范 5） */
    PLAYER_STATUS status;
    int8_t        remaining_rounds;   /* HOSPITAL/IMPRISONED 剩余轮空次数 */
    ITEMS         items;
    int8_t        god_of_wealth_rounds; /* 财神剩余回合，>0 免租 */
} PLAYER;

/** 已购地产（规范 3.3） */
typedef struct {
    int32_t position;
    int32_t owner_index;    /* 玩家在 users 中的下标，-1 表示无人 */
    int32_t level;          /* 0 空地 / 1 茅屋 / 2 洋房 / 3 摩天楼 */
} Property;

/** 地图上的道具（只能是 BLOCK 或 BOMB，规范 3.4） */
typedef struct {
    int32_t position;
    ItemKind kind;
} BoardItem;

/** 游戏全局状态 */
typedef struct {
    char    map_file[256];
    MapCell cells[MAP_SIZE];

    PLAYER  players[MAX_PLAYERS];   /* 数组顺序 = 回合顺序 = users 顺序 */
    int32_t user_count;
    int32_t current_index;          /* 当前玩家在 players 中的下标，-1 表示无 */

    GamePhase  phase;
    GameStatus status;
    PromptType prompt;              /* phase==PROMPT 时等待回答的提示类型 */

    Property  properties[MAX_BOARD_ITEMS];   /* 已购地产，按 position 升序维护 */
    int32_t   property_count;
    BoardItem board_items[MAX_BOARD_ITEMS];  /* 地图道具，按 position 升序维护 */
    int32_t   board_item_count;

    int32_t dice_seq[MAX_DICE_SEQ]; /* 预置骰子序列（规范 7） */
    int32_t dice_count;
    int32_t dice_next;              /* 下一个要读取的骰子下标 */

    int32_t winner_index;           /* 游戏结束时获胜玩家下标，-1 无 */
    bool    quit;                   /* QUIT 强制结束 */
    bool    dice_preset_loaded;     /* 是否加载过 preset 骰子序列（空序列时 ROLL 报错） */
} Game;

/* ===== 生命周期（规范 7.1 / 14 步骤 3~4） ===== */
void game_init(Game *g);                                /* 初始化为空状态 */
int  game_load_map(Game *g, const char *map_file);      /* 读取 map.json，0 成功，否则 RC_INVALID_MAP */
void game_reset(Game *g);                               /* 完整重置：执行每个测试前必须调用 */
int  game_apply_initial_fund(Game *g, int32_t initial_fund); /* 为已选玩家设置初始资金 */
int  game_start_manual(Game *g, int32_t initial_fund);  /* 手动对局开局：四名玩家同额初始资金 */
int  game_apply_preset(Game *g, const struct cJSON *preset);   /* 加载 Preset，0 成功 */
const char *game_last_error(void);                      /* 最近一次游戏操作的错误描述 */

/* ===== 枚举 <-> JSON 字符串固定映射（规范 14.2） ===== */
const char *cell_type_to_str(CellType t);
int         cell_type_from_str(const char *s);      /* 找不到返回 -1 */
const char *player_status_to_str(PLAYER_STATUS s);
int         player_status_from_str(const char *s);
const char *item_kind_to_str(ItemKind k);
int         item_kind_from_str(const char *s);
const char *phase_to_str(GamePhase p);
const char *game_status_to_str(GameStatus s);
const char *prompt_to_str(PromptType p);

/* ===== 地产经济（规范 3.3） ===== */
int32_t property_total_invest(const Game *g, const Property *p); /* 购买价格 + level x 升级费用 */
int32_t property_rent(const Game *g, const Property *p);         /* 投资总成本 / 2 */
int32_t property_sell_price(const Game *g, const Property *p);   /* 投资总成本 x 2 */
void get_rent(Game *g, Property p);
void game_bankrupt_player(Game *g, int32_t player_index);
void game_finish_action_turn(Game *g);
void handle_land_landing(Game *g, int32_t position);
int land_answer_buy(Game *g, const char *value, char *message, size_t message_size);
int land_answer_upgrade(Game *g, const char *value, char *message, size_t message_size);
int game_sell_property(Game *g, int32_t position);
int gift_shop_enter(Game *g, char *message, size_t message_size);
int gift_shop_answer(Game *g, const char *input, char *message, size_t message_size);

/* ===== 查询 ===== */
PLAYER *game_current_player(Game *g);
const PLAYER *game_current_player_c(const Game *g);
const Property *game_property_at(const Game *g, int32_t position);  /* 无则 NULL */
const BoardItem *game_board_item_at(const Game *g, int32_t position);
int   game_active_count(const Game *g);                 /* 未破产玩家数 */
bool  game_has_finished(const Game *g);                 /* 只剩一名未破产玩家 */
int   game_player_index_by_id(const Game *g, const char *id);  /* 找不到返回 -1 */
int   game_next_player_index(const Game *g);            /* 按 users 顺序的下一位未破产玩家，无则 -1 */

/* ===== Action 入口（规范 8；返回 0=成功，负数=ResultCode） ===== */
int game_roll(Game *g);                                 /* ROLL：使用预置骰子移动 */
int game_step(Game *g, int32_t steps);                  /* STEP：按指定步数移动 */
int game_sell(Game *g, int32_t position);               /* SELL：出售地产 */
int game_block(Game *g, int32_t offset);                /* BLOCK：放置路障 */
int game_bomb(Game *g, int32_t offset);                 /* BOMB：放置炸弹 */
int game_robot(Game *g);                                /* ROBOT：清除前方十格道具 */
int game_answer(Game *g, const char *value);            /* ANSWER：回答购买/升级/商店提示 */
int game_query(const Game *g, char *buf, size_t bufsz); /* QUERY：查询当前玩家资产 */
int game_help(char *buf, size_t bufsz);                 /* HELP：命令帮助文本 */
int game_quit(Game *g);                                 /* QUIT：强制结束游戏 */


/* ===== 内部流程（规范 4 回合和游戏流程） ===== */
int game_move_to(Game *g, int32_t steps, int8_t last_position); /* 逐格移动+途中道具触发，返回最终落点 */
void game_settle_landing(Game *g);    /* 落点处理（规范 9） */
void game_next_turn(Game *g);                   /* 回合切换与轮空（规范 4.3） */
void game_check_finish(Game *g);                /* 破产/结束判定 */
void game_boarditem_suc(Game *g, BoardItem *b, int8_t index);               /*道具生效判定*/
void game_remove_board_item(Game *g, int index);                            /*清除道具*/

int tool_shop_enter(Game *g, char *message, size_t message_size);
int tool_shop_answer(Game *g, const char *input,
                     char *message, size_t message_size);

#endif /* RICH_GAME_H */
