/**
 * @file game.h
 * @brief 大富翁游戏核心状态与规则 —— 规范 v2.0 第 3/4/5/6/7/14/15 节
 *
 * 只定义"外部可观察行为"所需的数据结构与接口，
 * 不限制内部实现细节（规范 2 跨语言兼容原则）。
 *
 * v2.0 相对 v1.1 的变更：
 *   - 删除监狱/医院/魔法屋/炸弹（BOMB）相关功能；地图 14/49/63 为公园 PARK；
 *   - 新增地图财神（10 回合后首次生成、5 回合自然消失、领取/失效后 1~10 回合再生成）；
 *   - 新增 turn_number、确定性随机流（SEQUENCE / XORSHIFT32 PRNG）、ADVANCE_TURN；
 *   - STEP 允许 1~2147483647，steps>70 时对 70 取余；
 *   - 输入 command 不区分 ASCII 大小写。
 */
#ifndef RICH_GAME_H
#define RICH_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct cJSON;

/* ===== 基础常量（规范 3.x / 4.x / 14.x） ===== */
#define MAP_SIZE            70    /* 地图合法编号 0~69，69 的下一格为 0 */
#define MAX_PLAYERS          4    /* 每局 2~4 名玩家 */
#define MAX_ITEM_TOTAL      10    /* 每名玩家背包道具总数上限 */
#define LAND_MAX_LEVEL       3    /* 地产最高等级：摩天楼 */
#define BLOCK_OFFSET_LIMIT  10    /* BLOCK 的 offset 范围 -10~10 */
#define ROBOT_CLEAR_RANGE   10    /* 机器娃娃清除前方 1~10 格 */
#define DICE_MIN             1
#define DICE_MAX             6
#define STEP_MAX      2147483647  /* STEP steps 上限（int32 最大值，规范 6） */
#define USER_ID_MAX         16    /* 玩家标识字符串上限 */
#define MAX_DICE_SEQ      1024    /* 每个随机流的序列长度上限 */
#define MAX_BOARD_ITEMS    100    /* 地图道具/地产动态数组容量上限 */
#define PARK_POS_1          14    /* 规范 5.2：统一地图 14/49/63 必须为 PARK */
#define PARK_POS_2          49
#define PARK_POS_3          63
#define TOOL_POS            28    /* 道具屋位置 */
#define GIFT_POS            35    /* 礼品屋位置 */

/* 财神规则（规范 14） */
#define FORTUNE_FIRST_SPAWN_TURN  10   /* 完成第 10 个玩家回合后首次生成 */
#define FORTUNE_MAP_TURNS          5   /* 财神在地图保留 5 回合 */
#define FORTUNE_RESPAWN_MIN        1   /* 再生成延迟 1~10 回合 */
#define FORTUNE_RESPAWN_MAX       10
#define GOD_OF_WEALTH_TURNS        5   /* 领取财神后免租回合数 */

#define MANUAL_INITIAL_FUND_DEFAULT 10000
#define MANUAL_INITIAL_FUND_MIN      1000
#define MANUAL_INITIAL_FUND_MAX     50000

/* ===== 统一错误码（规范 18） ===== */
typedef enum {
    RC_OK = 0,                    /* 无错误 */
    RC_INVALID_JSON,              /* JSON 无法解析 */
    RC_UNSUPPORTED_VERSION,       /* 不支持 schema_version */
    RC_UNSUPPORTED_MODE,          /* 不支持 mode（规范 18） */
    RC_INVALID_PRESET,            /* Preset 字段缺失或状态冲突 */
    RC_INVALID_MAP,               /* 地图文件错误 */
    RC_INVALID_COMMAND,           /* 不支持的 command */
    RC_INVALID_PARAMS,            /* Action 参数缺失、类型错误或越界 */
    RC_INVALID_PHASE,             /* 当前阶段不允许执行该 Action */
    RC_RANDOM_SEQUENCE_EMPTY,     /* 确定性随机流耗尽（规范 18） */
    RC_RANDOM_VALUE_OUT_OF_RANGE, /* 随机流值越界（规范 18） */
    RC_ACTION_AFTER_END,          /* 游戏结束后仍有 Action */
    RC_ASSERT_NOT_EQUAL,          /* Actual 与 Expected 不相等 */
    RC_ASSERT_NOT_FOUND,          /* Expected 要求的对象不存在 */
    RC_ASSERT_NOT_ABSENT,         /* 应不存在的对象实际存在 */
    RC_IO_ERROR,                  /* 文件读写失败 */
    RC_INTERNAL                   /* 未实现/内部错误 */
} ResultCode;

const char *result_code_name(ResultCode rc);    /* 输出 "INVALID_PARAMS" 等规范错误码文本 */

/* ===== 枚举（与 JSON 字符串的固定映射见 game.c） ===== */
typedef enum {
    CELL_START = 0,     /* 起点 */
    CELL_LAND_1,        /* 地段一普通地产 */
    CELL_LAND_2,        /* 地段二普通地产 */
    CELL_LAND_3,        /* 地段三普通地产 */
    CELL_TOOL_SHOP,     /* 道具屋 */
    CELL_GIFT_SHOP,     /* 礼品屋 */
    CELL_PARK,          /* 公园：到达和经过均无事件（规范 5.2） */
    CELL_MINE,          /* 矿地 */
    CELL_KIND_COUNT
} CellType;

/* BOMB 已在 v2.0 删除；枚举值保留仅供错误提示，任何 Preset/命令携带 BOMB 均报错 */
typedef enum { ITEM_BLOCK, ITEM_BOMB, ITEM_ROBOT, ITEM_KIND_COUNT } ItemKind;
typedef enum { PHASE_COMMAND, PHASE_PROMPT, PHASE_ENDED } GamePhase;
typedef enum { GAME_RUNNING, GAME_FINISHED } GameStatus;
typedef enum {
    PROMPT_NONE, PROMPT_BUY, PROMPT_UPGRADE, PROMPT_TOOL_SHOP, PROMPT_GIFT_SHOP
} PromptType;

/**
 * 玩家状态（规范 5.4）：v2.0 删除 HOSPITAL/JAIL 与 remaining_rounds，
 * 状态只允许 NORMAL 与 BANKRUPT。
 */
typedef enum player_status {
    NORMAL,       /* 可以正常行动 */
    BANKRUPT      /* 已经破产，跳过回合 */
} PLAYER_STATUS;

/** 玩家背包道具（规范 5.1：仅 BLOCK/ROBOT，各自 0..10，合计不超过 MAX_ITEM_TOTAL） */
typedef struct items {
    int8_t BLOCK;   /* 路障 */
    int8_t ROBOT;   /* 机器娃娃 */
} ITEMS;

/** 具名随机流（规范 7） */
typedef enum {
    RSTREAM_DICE = 0,              /* ROLL 骰子 1..6 */
    RSTREAM_FORTUNE_POSITION,      /* 财神候选位置 0..69 */
    RSTREAM_FORTUNE_RESPAWN_DELAY, /* 财神再生成延迟 1..10 */
    RSTREAM_GIFT,                  /* 礼品编号（项目定义，当前未使用） */
    RSTREAM_COUNT
} RandomStreamKind;

typedef enum {
    RANDOM_MODE_NONE = 0,          /* 未声明：使用语言自带随机 */
    RANDOM_MODE_SEQUENCE,          /* 顺序流 */
    RANDOM_MODE_PRNG               /* XORSHIFT32（规范 7.1） */
} RandomMode;

/**
 * 确定性随机控制（规范 7）：
 *   SEQUENCE：streams.<流名> 依次取值，耗尽/越界在取值时报错；
 *   PRNG：algorithm=XORSHIFT32，stream_seeds 每个流独立 32 位状态。
 */
typedef struct {
    RandomMode mode;
    int32_t    seq[RSTREAM_COUNT][MAX_DICE_SEQ];
    int32_t    seq_count[RSTREAM_COUNT];
    int32_t    seq_next[RSTREAM_COUNT];
    uint32_t   prng_state[RSTREAM_COUNT];
} RandomControl;

/* ===== 数据结构 ===== */

/** 地图格（由统一的 map.json 确定，规范 3.2 / 5.2） */
typedef struct {
    CellType type;
    int32_t  price;         /* 购买价格（LAND_1=200 LAND_2=500 LAND_3=300） */
    int32_t  upgrade_cost;  /* 每次升级费用 */
    int32_t  mine_points;   /* MINE 每次到达增加的点数（地图配置） */
} MapCell;

/**
 * 玩家（users 数组顺序即回合顺序，规范 3.1 / 5.1）。
 * fund/credit 为 int32_t：规范 2.1 要求整数限 int32 范围。
 */
typedef struct player {
    char          id;                 /* 角色标识：Q 钱夫人 / A 阿土伯 / S 孙小美 / J 金贝贝 */
    int32_t       fund;
    int32_t       credit;             /* 点数 */
    int8_t        position;           /* 逻辑位置 0~69，允许重叠（规范 5） */
    PLAYER_STATUS status;
    ITEMS         items;
    int8_t        god_of_wealth_rounds; /* 财神剩余回合，>0 免租（规范 14.5） */
} PLAYER;

/** 已购地产（规范 3.3） */
typedef struct {
    int32_t position;
    int32_t owner_index;    /* 玩家在 users 中的下标，-1 表示无人 */
    int32_t level;          /* 0 空地 / 1 茅屋 / 2 洋房 / 3 摩天楼 */
} Property;

/** 地图上的道具（规范 5.3：只允许 BLOCK） */
typedef struct {
    int32_t position;
    ItemKind kind;
} BoardItem;

/**
 * 地图财神（规范 14）。
 * position == -1 表示地图上无财神；next_spawn_after_turn == 0 表示无生成计划。
 * 生成计划在"该回合完成时"触发：完成第 N 回合（turn_number == N）的回合结束
 * 处理中生成财神，随后 turn_number 变为 N+1。
 */
typedef struct {
    int32_t position;               /* -1 无；0..69 存在 */
    int32_t spawned_after_turn;     /* 生成于第几回合完成后；0 表示无 */
    int32_t remaining_map_turns;    /* 保留回合数 0..5；0 表示已移除 */
    int32_t next_spawn_after_turn;  /* 0 无计划；>0 该回合完成时生成 */
} Fortune;

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

    int32_t turn_number;            /* 当前玩家回合编号，从 1 开始（规范 8） */
    Fortune fortune;                /* 地图财神完整生命周期（规范 8/14） */
    RandomControl rng;              /* 确定性随机源（规范 7） */

    int32_t winner_index;           /* 游戏结束时获胜玩家下标，-1 无 */
    bool    quit;                   /* QUIT 强制结束 */
    bool    god_acquired_this_turn; /* 本回合获得财神：回合结束不扣减财神回合 */
} Game;

/* ===== 生命周期（规范 19 步骤 3~4） ===== */
void game_init(Game *g);                                /* 初始化为空状态 */
int  game_load_map(Game *g, const char *map_file);      /* 读取 map.json，0 成功，否则 RC_INVALID_MAP */
void game_reset(Game *g);                               /* 完整重置：执行每个测试前必须调用 */
int  game_apply_initial_fund(Game *g, int32_t initial_fund); /* 为已选玩家设置初始资金 */
int  game_start_manual(Game *g, int32_t initial_fund);  /* 手动对局开局：四名玩家同额初始资金 */
int  game_apply_preset(Game *g, const struct cJSON *preset);   /* 加载 Preset，0 成功 */
const char *game_last_error(void);                      /* 最近一次游戏操作的错误描述 */
void game_set_error(const char *fmt, ...);              /* 设置最近一次错误描述（玩家可读提示） */
void game_set_error(const char *fmt, ...);              /* 设置最近一次错误描述（玩家可读） */

/* ===== 静默输出（自动化测试模式） ===== */
extern bool g_game_quiet;               /* true 时游戏内部提示不打印，仅测试模式置位 */
void game_print(const char *fmt, ...);  /* 游戏内部统一输出入口，受 g_game_quiet 控制 */

/* ===== 枚举 <-> JSON 字符串固定映射 ===== */
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
int  game_finish_action_turn(Game *g);
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

/* ===== Action 入口（规范 9；返回 0=成功，负数=ResultCode） ===== */
int game_roll(Game *g);                                 /* ROLL：从 DICE 流取值并移动 */
int game_step(Game *g, int32_t steps);                  /* STEP：1~2147483647，>70 对 70 取余后移动 */
int game_sell(Game *g, int32_t position);               /* SELL：出售地产 */
int game_block(Game *g, int32_t offset);                /* BLOCK：放置路障 */
int game_robot(Game *g);                                /* ROBOT：清除前方十格道具 */
int game_answer(Game *g, const char *value);            /* ANSWER：回答购买/升级/商店提示 */
int game_advance_turn(Game *g);                         /* ADVANCE_TURN：STATE 测试专用原地推进一回合 */
int game_query(const Game *g, char *buf, size_t bufsz); /* QUERY：查询当前玩家资产 */
int game_help(char *buf, size_t bufsz);                 /* HELP：命令帮助文本 */
int game_quit(Game *g);                                 /* QUIT：强制结束游戏 */

/* ===== 随机流（规范 7） ===== */
const char *random_stream_name(RandomStreamKind k);
/** 从指定流取一个 [min,max] 内的值；失败返回负数错误码 */
int random_next(Game *g, RandomStreamKind k, int32_t min, int32_t max, int32_t *out);

/* ===== 内部流程（规范 4 回合和游戏流程） ===== */
int  game_process_fortune_pickup(Game *g);  /* 领取地图财神并调度再生成（规范 14.4/14.5） */
int  game_process_fortune_turn_end(Game *g);/* 回合结束时财神保留/失效/生成处理（规范 14.3/14.4） */
int  game_move_to(Game *g, int32_t steps, int8_t last_position); /* 逐格移动+途中道具/财神触发 */
void game_settle_landing(Game *g);    /* 落点处理（规范 9） */
void game_next_turn(Game *g);         /* 回合切换（turn_number+1、下一位未破产玩家） */
void game_check_finish(Game *g);      /* 破产/结束判定 */
void game_boarditem_suc(Game *g, BoardItem *b, int8_t index); /* 道具生效判定 */
void game_remove_board_item(Game *g, int index);               /* 清除道具 */

int tool_shop_enter(Game *g, char *message, size_t message_size);
int tool_shop_answer(Game *g, const char *input,
                     char *message, size_t message_size);

#endif /* RICH_GAME_H */
