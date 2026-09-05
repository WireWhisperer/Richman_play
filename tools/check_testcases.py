# -*- coding: utf-8 -*-
"""独立静态合规校验器 —— 《大富翁游戏自动化测试 JSON 接口规范 v2.0》
按 Richman_play 运行器(C) 的实现口径校验 testcases/*.json：

级别定义：
  FATAL —— 运行器在 加载/套件/预置校验 阶段必然报 ERROR 的硬伤（不合规）
  ERROR —— actions 形式级/参数级必然执行报错（除非用例自身用 expected_error 声明）
  WARN  —— 静态可疑项：期望结构/枚举/取值范围不吻合，大概率运行 FAIL 或作者笔误

用法: python tools/check_testcases.py [文件或目录，缺省 testcases]
"""
import json
import os
import sys

MAP_SIZE = 70
MAX_PLAYERS = 4
MAX_ITEM_TOTAL = 10
LAND_MAX_LEVEL = 3
BLOCK_OFFSET_LIMIT = 10
STEP_MAX = 2147483647
FORTUNE_MAP_TURNS = 5
GOD_OF_WEALTH_TURNS = 5
MAX_DICE_SEQ = 1024
INT32_MIN, INT32_MAX = -(2 ** 31), 2 ** 31 - 1

PARK_POS = {14, 49, 63}
STREAMS = {"DICE", "FORTUNE_POSITION", "FORTUNE_RESPAWN_DELAY", "GIFT"}
COMMANDS = {"ROLL", "STEP", "SELL", "BLOCK", "ROBOT", "QUERY", "HELP",
            "ANSWER", "QUIT", "ADVANCE_TURN"}
STATUS = {"NORMAL", "BANKRUPT"}
CELL_TYPES = {"START", "LAND_1", "LAND_2", "LAND_3", "TOOL_SHOP",
              "GIFT_SHOP", "PARK", "MINE"}
PHASES = {"COMMAND", "PROMPT", "ENDED"}
GAME_STATUSES = {"RUNNING", "FINISHED"}
PROMPTS = {"BUY", "UPGRADE", "TOOL_SHOP", "GIFT_SHOP"}
ENTITY = {"BASE", "MAP_ITEM", "FORTUNE", "PLAYER"}

# Actual 顶层键（expected 可写出的合法对象键）
ACTUAL_TOP_KEYS = {"users", "current_user", "phase", "pending_prompt",
                   "game_status", "winner", "turn_number", "players",
                   "properties", "map_items", "fortune", "display_players",
                   "display_cells"}
PLAYER_KEYS = {"id", "fund", "credit", "position", "status", "items",
               "god_of_wealth_rounds"}
PROP_KEYS = {"position", "owner", "level"}
MAPITEM_KEYS = {"position", "type"}
FORTUNE_KEYS = {"position", "symbol", "spawned_after_turn",
                "remaining_map_turns", "next_spawn_after_turn"}
DISP_PLAYER_KEYS = {"position", "visible_user"}
DISP_CELL_KEYS = {"position", "base_type", "base_symbol", "visible_symbol",
                  "visible_entity"}
# 数组主键映射（expected 数组项按主键匹配 actual）
ARRAY_PK = {"players": "id", "properties": "position", "map_items": "position",
            "display_players": "position", "display_cells": "position"}


class Issue(object):
    def __init__(self, level, case, msg):
        self.level = level      # FATAL / ERROR / WARN
        self.case = case
        self.msg = msg

    def __str__(self):
        return "[%s] %s: %s" % (self.level, self.case, self.msg)


def is_int(v):
    return isinstance(v, bool) is False and isinstance(v, int)


def in_int32(v):
    return is_int(v) and INT32_MIN <= v <= INT32_MAX


def is_str(v):
    return isinstance(v, str)


# ---------------- 地图加载镜像（仅校验静态规则） ----------------

def load_map_types(root_dir, map_file, issues, case):
    """返回 {pos: type} 或 None（加载失败已记录）"""
    if not isinstance(map_file, str) or not map_file:
        issues.append(Issue("FATAL", case, "map_file 必须为非空字符串"))
        return None
    # 解析顺序与 runner load_map_for_case 一致
    dirs = [root_dir,
            os.path.join(root_dir, ".."),
            os.path.join(root_dir, "..", "spec"),
            os.path.join(root_dir, "..", "..", "spec")]
    base = os.path.join(os.getcwd(), "spec")
    if not os.path.exists(base):
        base = os.path.join(os.getcwd())
    candidates = [os.path.join(d, map_file) for d in dirs] + \
                 [os.path.join(os.getcwd(), "spec", map_file),
                  os.path.join(os.getcwd(), map_file)]
    path = None
    for c in candidates:
        if os.path.isfile(c):
            path = c
            break
    if path is None:
        issues.append(Issue("FATAL", case,
                            "map_file '%s' 在用例目录/上级/spec/cwd 均找不到" % map_file))
        return None
    try:
        with open(path, "r", encoding="utf-8-sig") as f:
            data = json.load(f)
    except Exception as e:
        issues.append(Issue("FATAL", case, "map_file '%s' 无法解析: %s" % (map_file, e)))
        return None
    cells = data.get("cells") if isinstance(data, dict) else None
    if not isinstance(cells, list) or len(cells) != MAP_SIZE:
        issues.append(Issue("FATAL", case,
                            "map '%s' cells 必须恰好 70 项（当前 %s）"
                            % (map_file, len(cells) if isinstance(cells, list) else "非数组")))
        return None
    seen = {}
    for c in cells:
        if not isinstance(c, dict):
            issues.append(Issue("FATAL", case, "map cell 必须为对象"))
            return None
        pos, typ = c.get("position"), c.get("type")
        if not is_int(pos) or not (0 <= pos < MAP_SIZE):
            issues.append(Issue("FATAL", case, "map cell.position 非法"))
            return None
        if pos in seen:
            issues.append(Issue("FATAL", case, "map position 重复: %d" % pos))
            return None
        if not is_str(typ) or typ not in CELL_TYPES:
            issues.append(Issue("FATAL", case, "map position %d type 非法" % pos))
            return None
        seen[pos] = typ
    for p in PARK_POS:
        if seen.get(p) != "PARK":
            issues.append(Issue("FATAL", case, "map 位置 %d 必须为 PARK" % p))
            return None
    for p, t in seen.items():
        if p not in PARK_POS and t == "PARK":
            issues.append(Issue("FATAL", case, "map 位置 %d 不得为 PARK（仅 14/49/63）" % p))
            return None
    return seen


# ---------------- Preset 校验（镜像 case_validate_preset） ----------------

def check_preset(preset, map_types, issues, case):
    if not isinstance(preset, dict):
        issues.append(Issue("FATAL", case, "preset 必须为对象"))
        return

    # users
    users = preset.get("users")
    if not isinstance(users, list):
        issues.append(Issue("FATAL", case, "preset.users 必须为数组"))
        return
    if not (2 <= len(users) <= MAX_PLAYERS):
        issues.append(Issue("FATAL", case, "preset.users 必须包含 2~4 名玩家"))
    for i, u in enumerate(users):
        if not is_str(u) or len(u) != 1:
            issues.append(Issue("FATAL", case,
                                "preset.users[%d] 必须为单字符标识" % i))
        if is_str(u) and users.index(u) != i:
            issues.append(Issue("FATAL", case, "preset.users 存在重复角色: %s" % u))

    # players
    players = preset.get("players")
    if not isinstance(players, list):
        issues.append(Issue("FATAL", case, "preset.players 必须为数组"))
        return
    if len(players) != len(users):
        issues.append(Issue("FATAL", case,
                            "preset.players 数量(%d)必须与 users(%d)一致"
                            % (len(players), len(users))))
    for i, p in enumerate(players):
        tag = "players[%d]" % i
        if not isinstance(p, dict):
            issues.append(Issue("FATAL", case, "preset.%s 必须为对象" % tag))
            continue
        pid = p.get("id")
        if not is_str(pid) or pid not in users:
            issues.append(Issue("FATAL", case,
                                "preset.%s.id 非法或不在 users 中" % tag))
            pid = None
        if is_str(pid) and (i >= len(users) or users[i] != pid):
            issues.append(Issue("FATAL", case,
                                "preset.players 必须按 users 顺序排列（第 %d 项）" % i))
        for f in ("fund", "credit", "position", "god_of_wealth_rounds"):
            v = p.get(f)
            if not in_int32(v):
                issues.append(Issue("FATAL", case,
                                    "preset.%s.%s 非法（需 int32 整数）" % (tag, f)))
        pos = p.get("position")
        if is_int(pos) and not (0 <= pos < MAP_SIZE):
            issues.append(Issue("FATAL", case,
                                "preset.%s.position 越界（0~69）" % tag))
        g = p.get("god_of_wealth_rounds")
        if is_int(g) and not (0 <= g <= GOD_OF_WEALTH_TURNS):
            issues.append(Issue("FATAL", case,
                                "preset.%s.god_of_wealth_rounds 越界（0~5）" % tag))
        st = p.get("status")
        if not is_str(st) or st not in STATUS:
            issues.append(Issue("FATAL", case,
                                "preset.%s.status 非法（只允许 NORMAL/BANKRUPT）" % tag))
        if "remaining_rounds" in p:
            issues.append(Issue("FATAL", case,
                                "preset.%s.remaining_rounds 已在 v2.0 删除" % tag))
        items = p.get("items")
        if not isinstance(items, dict):
            issues.append(Issue("FATAL", case, "preset.%s.items 必须为对象" % tag))
        else:
            if "BOMB" in items:
                issues.append(Issue("FATAL", case,
                                    "preset.%s.items.BOMB 已在 v2.0 删除" % tag))
            total = 0
            for k in ("BLOCK", "ROBOT"):
                v = items.get(k)
                if not is_int(v) or not (0 <= v <= MAX_ITEM_TOTAL):
                    issues.append(Issue("FATAL", case,
                                        "preset.%s.items.%s 非法（需 0~10 整数）" % (tag, k)))
                else:
                    total += v
            if total > MAX_ITEM_TOTAL:
                issues.append(Issue("FATAL", case,
                                    "preset.%s 背包道具总数 %d 超过 10" % (tag, total)))

    # current_user
    cur = preset.get("current_user")
    if not is_str(cur) or cur not in users:
        issues.append(Issue("FATAL", case, "preset.current_user 必须属于 users"))
    else:
        for p in players:
            if isinstance(p, dict) and p.get("id") == cur and \
                    p.get("status") == "BANKRUPT":
                issues.append(Issue("FATAL", case,
                                    "preset.current_user 不能为 BANKRUPT"))

    # phase
    if preset.get("phase") != "COMMAND":
        issues.append(Issue("FATAL", case, "preset.phase 必须为 COMMAND"))

    # properties
    props = preset.get("properties")
    if not isinstance(props, list):
        issues.append(Issue("FATAL", case, "preset.properties 必须为数组"))
        props = []
    used = set()
    for i, pp in enumerate(props):
        tag = "properties[%d]" % i
        if not isinstance(pp, dict):
            issues.append(Issue("FATAL", case, "preset.%s 必须为对象" % tag))
            continue
        pos, lv = pp.get("position"), pp.get("level")
        if not is_int(pos) or not (0 <= pos < MAP_SIZE):
            issues.append(Issue("FATAL", case,
                                "preset.%s.position 非法（0~69）" % tag))
        elif pos in used:
            issues.append(Issue("FATAL", case,
                                "preset.properties 地产位置重复: %s" % pos))
        elif is_int(pos):
            used.add(pos)
            if map_types is not None and map_types.get(pos) not in \
                    ("LAND_1", "LAND_2", "LAND_3", None):
                issues.append(Issue("WARN", case,
                                    "preset.properties 位置 %d 非地产格(%s)"
                                    % (pos, map_types.get(pos))))
        if not is_int(lv) or not (0 <= lv <= LAND_MAX_LEVEL):
            issues.append(Issue("FATAL", case,
                                "preset.%s.level 非法（0~3）" % tag))
        owner = pp.get("owner")
        if not is_str(owner) or owner not in users:
            issues.append(Issue("FATAL", case,
                                "preset.%s.owner 必须属于 users" % tag))
        else:
            for p in players:
                if isinstance(p, dict) and p.get("id") == owner and \
                        p.get("status") == "BANKRUPT":
                    issues.append(Issue("FATAL", case,
                                        "preset.%s.owner 不能为 BANKRUPT" % tag))

    # map_items
    bits = preset.get("map_items")
    if not isinstance(bits, list):
        issues.append(Issue("FATAL", case, "preset.map_items 必须为数组"))
        bits = []
    usedb = set()
    for i, bi in enumerate(bits):
        tag = "map_items[%d]" % i
        if not isinstance(bi, dict):
            issues.append(Issue("FATAL", case, "preset.%s 必须为对象" % tag))
            continue
        pos = bi.get("position")
        if not is_int(pos) or not (0 <= pos < MAP_SIZE):
            issues.append(Issue("FATAL", case,
                                "preset.%s.position 非法（0~69）" % tag))
        elif pos in usedb:
            issues.append(Issue("FATAL", case,
                                "preset.map_items 位置重复: %s" % pos))
        elif is_int(pos):
            usedb.add(pos)
        if bi.get("type") != "BLOCK":
            issues.append(Issue("FATAL", case,
                                "preset.%s.type 只能是 BLOCK（BOMB 已删除）" % tag))

    if "dice_sequence" in preset:
        issues.append(Issue("FATAL", case,
                            "preset.dice_sequence 已被 random_control.streams.DICE 取代"))

    tn = preset.get("turn_number")
    if not is_int(tn) or tn < 1:
        issues.append(Issue("FATAL", case,
                            "preset.turn_number 必须为 >=1 的整数"))

    # fortune
    check_fortune(preset.get("fortune"), issues, case)

    # random_control
    rc = preset.get("random_control")
    if rc is not None and not isinstance(rc, dict):
        issues.append(Issue("FATAL", case, "preset.random_control 必须为对象"))
    elif isinstance(rc, dict):
        mode = rc.get("mode")
        if mode == "SEQUENCE":
            streams = rc.get("streams")
            if not isinstance(streams, dict):
                issues.append(Issue("FATAL", case,
                                    "random_control(SEQUENCE) 缺少 streams 对象"))
            else:
                for name, arr in streams.items():
                    if name not in STREAMS:
                        issues.append(Issue("FATAL", case,
                                            "random_control.streams 含未知流: %s" % name))
                    elif not isinstance(arr, list):
                        issues.append(Issue("FATAL", case,
                                            "random_control.streams.%s 必须为数组" % name))
                    elif len(arr) > MAX_DICE_SEQ:
                        issues.append(Issue("FATAL", case,
                                            "random_control.streams.%s 超限(1024)" % name))
                    else:
                        for i, v in enumerate(arr):
                            if not in_int32(v):
                                issues.append(Issue(
                                    "FATAL", case,
                                    "random_control.streams.%s[%d] 必须为 int32 整数"
                                    % (name, i)))
        elif mode == "PRNG":
            if rc.get("algorithm") != "XORSHIFT32":
                issues.append(Issue("FATAL", case,
                                    "random_control(PRNG).algorithm 必须为 XORSHIFT32"))
            seeds = rc.get("stream_seeds")
            if not isinstance(seeds, dict):
                issues.append(Issue("FATAL", case,
                                    "random_control(PRNG) 缺少 stream_seeds 对象"))
            else:
                for name, v in seeds.items():
                    if not is_int(v) or not (1 <= v <= INT32_MAX):
                        issues.append(Issue("FATAL", case,
                                            "stream_seeds.%s 必须为 1~2147483647 整数" % name))
        else:
            issues.append(Issue("FATAL", case,
                                "random_control.mode 必须为 SEQUENCE 或 PRNG"))


def check_fortune(f, issues, case):
    if not isinstance(f, dict):
        issues.append(Issue("FATAL", case, "preset.fortune 必须为对象（规范 8）"))
        return
    pos, sat, rmt, nsat = (f.get("position"), f.get("spawned_after_turn"),
                           f.get("remaining_map_turns"),
                           f.get("next_spawn_after_turn"))
    pos_null = pos is None
    sat_null = sat is None
    nsat_null = nsat is None
    if not pos_null and not (is_int(pos) and 0 <= pos < MAP_SIZE):
        issues.append(Issue("FATAL", case,
                            "fortune.position 非法（null 或 0~69）"))
    if not sat_null and not (is_int(sat) and sat >= 1):
        issues.append(Issue("FATAL", case,
                            "fortune.spawned_after_turn 非法（null 或 >=1）"))
    if not is_int(rmt) or not (0 <= rmt <= FORTUNE_MAP_TURNS):
        issues.append(Issue("FATAL", case,
                            "fortune.remaining_map_turns 非法（0~5）"))
    if not nsat_null and not (is_int(nsat) and nsat >= 1):
        issues.append(Issue("FATAL", case,
                            "fortune.next_spawn_after_turn 非法（null 或 >=1）"))
    if pos_null:
        if not sat_null or rmt != 0:
            issues.append(Issue(
                "FATAL", case,
                "fortune 冲突：无财神时 spawned_after_turn 须 null 且 remaining 须 0"))
    else:
        if sat_null or not (is_int(rmt) and 1 <= rmt <= FORTUNE_MAP_TURNS) \
                or not nsat_null:
            issues.append(Issue(
                "FATAL", case,
                "fortune 冲突：有财神时 spawned 须非 null、remaining 须 1~5、next 须 null"))


# ---------------- Actions 校验（镜像 action_validate 形式级规则） ----------------

def check_actions(actions, issues, case, intended_err_idx=None, limit=None):
    """intended_err_idx: 该下标的失败已由 expected_error 声明（负向），不再重复报；
       limit: 只检查到该下标为止（其后的 action 不会被执行）。"""
    if not isinstance(actions, list):
        issues.append(Issue("FATAL", case, "actions 必须为数组"))
        return
    if not actions:
        issues.append(Issue("INFO", case,
                            "actions 为空（快照/负向用例属正常；其余请自查）"))
        return
    for i, a in enumerate(actions):
        if limit is not None and i > limit:
            break   # 期望失败下标之后的 action 实际不会被执行
        tag = "actions[%d]" % i
        if not isinstance(a, dict):
            if i == intended_err_idx:
                continue
            issues.append(Issue("FATAL", case, "%s 必须为对象" % tag))
            continue
        cmd = a.get("command")
        if not is_str(cmd):
            if i == intended_err_idx:
                continue
            issues.append(Issue("FATAL", case, "%s 缺少 command 字符串" % tag))
            continue
        c = cmd.upper()
        if c not in COMMANDS:
            if i == intended_err_idx:
                continue
            issues.append(Issue("ERROR", case,
                                "%s 不支持的 command: %s" % (tag, cmd)))
            continue
        params = a.get("params")
        bad = False
        if c == "STEP":
            v = params.get("steps") if isinstance(params, dict) else None
            if not is_int(v) or not (0 <= v <= STEP_MAX):
                bad = "%s STEP.steps 必须为 0~2147483647 整数" % tag
        elif c == "SELL":
            v = params.get("position") if isinstance(params, dict) else None
            if not is_int(v) or not (0 <= v < MAP_SIZE):
                bad = "%s SELL.position 必须为 0~69 整数" % tag
        elif c == "BLOCK":
            v = params.get("offset") if isinstance(params, dict) else None
            if not is_int(v) or not (-BLOCK_OFFSET_LIMIT <= v <= BLOCK_OFFSET_LIMIT):
                bad = "%s BLOCK.offset 必须为 -10~10 整数" % tag
        elif c == "ANSWER":
            v = params.get("value") if isinstance(params, dict) else None
            if not is_str(v):
                bad = "%s ANSWER.value 必须为字符串" % tag
        if bad:
            if i == intended_err_idx:
                continue
            issues.append(Issue("ERROR", case, bad))


# ---------------- Expected 结构校验（部分匹配静态检查） ----------------

def _warn_unknown_keys(known, obj, issues, case, path):
    for k in obj:
        if k in known:
            continue
        if k == "fields_absent" or k == "fortune_assert":
            continue        # 这两类断言在任意对象层级都合法（规范 11）
        if k.endswith("_absent"):
            continue        # *_absent 断言同样递归合法
        issues.append(Issue("WARN", case,
                            "expected.%s 含未知键 '%s'（Actual 无此键，运行必 FAIL）"
                            % (path, k)))


def _special_keys(obj, issues, case, path):
    """检查断言型键的形态（fields_absent / *_absent / fortune_assert）"""
    for key, val in obj.items():
        if key == "fields_absent":
            if not isinstance(val, list):
                issues.append(Issue("WARN", case,
                                    "expected.%s.fields_absent 必须为字符串数组" % path))
            elif not all(is_str(v) for v in val):
                issues.append(Issue("WARN", case,
                                    "expected.%s.fields_absent 元素必须为字符串" % path))
        elif key.endswith("_absent"):
            base = key[:-7]
            if base in ARRAY_PK and not isinstance(val, list):
                issues.append(Issue("WARN", case,
                                    "expected.%s.%s 必须为数组" % (path, key)))
        elif key == "fortune_assert" and not isinstance(val, dict):
            issues.append(Issue("WARN", case,
                                "expected.%s.fortune_assert 必须为对象" % path))


def check_expected(expected, users, issues, case, map_types):
    if not isinstance(expected, dict):
        issues.append(Issue("FATAL", case, "expected 必须为对象"))
        return
    for key, val in expected.items():
        if key in ACTUAL_TOP_KEYS or key in ("fields_absent", "fortune_assert"):
            continue
        if key.endswith("_absent"):
            base = key[:-7]
            if base in ARRAY_PK:
                if not isinstance(val, list):
                    issues.append(Issue("WARN", case,
                                        "expected.%s 必须为数组" % key))
                continue
        issues.append(Issue("WARN", case,
                            "expected 顶层键 '%s' 不是 Actual 输出键（运行必 FAIL）" % key))

    # players（主键 id）
    pl = expected.get("players")
    if isinstance(pl, list):
        for item in pl:
            if not isinstance(item, dict):
                issues.append(Issue("WARN", case, "expected.players 元素必须为对象"))
                continue
            pid = item.get("id")
            _warn_unknown_keys(PLAYER_KEYS, item, issues, case,
                               "players[%s]" % pid)
            if pid is not None and users is not None and pid not in users:
                issues.append(Issue("WARN", case,
                                    "expected.players[id=%s] 不在 users 中" % pid))
            it = item.get("items")
            if isinstance(it, dict):
                _warn_unknown_keys({"BLOCK", "ROBOT"}, it, issues, case,
                                   "players[%s].items" % pid)
            st = item.get("status")
            if is_str(st) and st not in STATUS:
                issues.append(Issue("WARN", case,
                                    "expected.players[id=%s].status 非法: %s" % (pid, st)))
            pos = item.get("position")
            if is_int(pos) and not (0 <= pos < MAP_SIZE):
                issues.append(Issue("WARN", case,
                                    "expected.players[id=%s].position 越界" % pid))
            f = item.get("fund")
            if f is not None and not is_int(f):
                issues.append(Issue("WARN", case,
                                    "expected.players[id=%s].fund 应为整数" % pid))
            cr = item.get("credit")
            if cr is not None and not is_int(cr):
                issues.append(Issue("WARN", case,
                                    "expected.players[id=%s].credit 应为整数" % pid))
            for k in ("BLOCK", "ROBOT"):
                if isinstance(it, dict) and it.get(k) is not None:
                    if not is_int(it[k]) or not (0 <= it[k] <= MAX_ITEM_TOTAL):
                        issues.append(Issue("WARN", case,
                                            "expected.players[id=%s].items.%s 非法" % (pid, k)))

    # properties / map_items / display_players / display_cells（主键 position）
    for key, known in (("properties", PROP_KEYS), ("map_items", MAPITEM_KEYS),
                       ("display_players", DISP_PLAYER_KEYS),
                       ("display_cells", DISP_CELL_KEYS)):
        arr = expected.get(key)
        if not isinstance(arr, list):
            continue
        for item in arr:
            if not isinstance(item, dict):
                continue
            p = item.get("position")
            _warn_unknown_keys(known, item, issues, case,
                               "%s[pos=%s]" % (key, p))
            if is_int(p) and not (0 <= p < MAP_SIZE):
                issues.append(Issue("WARN", case,
                                    "expected.%s 的 position 越界: %s" % (key, p)))
            if key == "properties":
                lv = item.get("level")
                if is_int(lv) and not (0 <= lv <= LAND_MAX_LEVEL):
                    issues.append(Issue("WARN", case,
                                        "expected.properties level 非法: %s" % lv))
                if map_types is not None and is_int(p) and \
                        map_types.get(p) not in ("LAND_1", "LAND_2", "LAND_3", None):
                    issues.append(Issue("WARN", case,
                                        "expected.properties 位置 %s 非地产格" % p))
            elif key == "map_items" and item.get("type") not in (None, "BLOCK"):
                issues.append(Issue("WARN", case,
                                    "expected.map_items.type 非法: %s" % item.get("type")))
            elif key == "display_cells":
                for fld in ("base_type", "base_symbol", "visible_symbol"):
                    v = item.get(fld)
                    if is_str(v) and len(v) != 1 and fld == "base_symbol":
                        issues.append(Issue("WARN", case,
                                            "expected.display_cells.%s 应为单字符" % fld))
                bt = item.get("base_type")
                if is_str(bt) and bt not in CELL_TYPES:
                    issues.append(Issue("WARN", case,
                                        "expected.display_cells.base_type 非法: %s" % bt))
                ve = item.get("visible_entity")
                if is_str(ve) and ve not in ENTITY:
                    issues.append(Issue("WARN", case,
                                        "expected.display_cells.visible_entity 非法: %s" % ve))

    # fortune 对象
    ft = expected.get("fortune")
    if isinstance(ft, dict):
        _warn_unknown_keys(FORTUNE_KEYS, ft, issues, case, "fortune")
        p = ft.get("position")
        if p is not None and not is_int(p):
            issues.append(Issue("WARN", case, "expected.fortune.position 应为 null/整数"))

    # fortune_assert
    fa = expected.get("fortune_assert")
    if isinstance(fa, dict):
        _warn_unknown_keys(
            {"present", "position_between", "position_not_in", "unoccupied",
             "without_map_item"}, fa, issues, case, "fortune_assert")
        pb = fa.get("position_between")
        if isinstance(pb, list) and len(pb) == 2:
            for v in pb:
                if not is_int(v) or not (0 <= v < MAP_SIZE):
                    issues.append(Issue("WARN", case,
                                        "fortune_assert.position_between 越界"))
        pn = fa.get("position_not_in")
        if isinstance(pn, list):
            for v in pn:
                if not is_int(v) or not (0 <= v < MAP_SIZE):
                    issues.append(Issue("WARN", case,
                                        "fortune_assert.position_not_in 越界"))

    # 顶层标量枚举/类型抽查
    for k in ("current_user", "winner"):
        v = expected.get(k)
        if v is not None:
            if not is_str(v):
                issues.append(Issue("WARN", case, "expected.%s 应为 null/字符串" % k))
            elif users is not None and v not in users:
                issues.append(Issue("WARN", case,
                                    "expected.%s='%s' 不在 users 中" % (k, v)))
    ph = expected.get("phase")
    if is_str(ph) and ph not in PHASES:
        issues.append(Issue("WARN", case, "expected.phase 非法: %s" % ph))
    gs = expected.get("game_status")
    if is_str(gs) and gs not in GAME_STATUSES:
        issues.append(Issue("WARN", case, "expected.game_status 非法: %s" % gs))
    pp = expected.get("pending_prompt")
    if is_str(pp) and pp not in PROMPTS:
        issues.append(Issue("WARN", case, "expected.pending_prompt 非法: %s" % pp))
    tn = expected.get("turn_number")
    if tn is not None and not is_int(tn):
        issues.append(Issue("WARN", case, "expected.turn_number 应为整数"))


# ---------------- 用例级校验 ----------------

def expected_error_decl(obj):
    """返回 (code, action_index, path) 三元组；未声明返回 None"""
    ee = obj.get("expected_error") if isinstance(obj, dict) else None
    if not isinstance(ee, dict):
        return None
    code = ee.get("code")
    if not is_str(code):
        return None
    ai = ee.get("action_index")
    p = ee.get("path")
    return (code, ai if is_int(ai) else None, p if is_str(p) else None)


def is_negative_preset_case(obj):
    """预设级校验失败是否被 expected_outcome/expected_error 声明为预期（规范 17）"""
    if obj.get("expected_outcome") != "ERROR":
        return False
    d = expected_error_decl(obj)
    if d is None or d[0] != "INVALID_PRESET":
        return False
    ai, p = d[1], d[2]
    if ai is not None and ai != -1:
        return False
    if p is not None and p != "preset":
        return False
    return True


def is_negative_action_case(obj, action_idx):
    """actions[i] 形式级校验失败是否被声明为预期"""
    if obj.get("expected_outcome") != "ERROR":
        return False
    d = expected_error_decl(obj)
    if d is None:
        return False
    code, ai, p = d
    if ai is not None and ai != action_idx:
        return False
    if code not in ("INVALID_PARAMS", "INVALID_COMMAND", "INVALID_PHASE",
                    "ACTION_AFTER_END"):
        return False
    if p is not None:
        # 路径必须指向 actions[i]（形式为 actions[i].params.xxx / actions[i].command）
        pfx = "actions[%d]" % action_idx
        if not (p == pfx or p.startswith(pfx + ".")):
            return False
    return True


def check_case(idx, obj, schema_ver, default_map, root_dir, issues):
    case = "#%d" % (idx + 1)
    cid = obj.get("case_id") if isinstance(obj, dict) else None
    if is_str(cid):
        case = cid
    if not isinstance(obj, dict):
        issues.append(Issue("FATAL", case, "用例必须为 JSON 对象"))
        return set()

    mode = obj.get("mode")
    if is_str(mode) and mode != "STATE":
        issues.append(Issue("FATAL", case,
                            "不支持的 mode: %s（仅实现 STATE）" % mode))

    ok_fields = True
    for f in ("case_id", "case_name"):
        v = obj.get(f)
        if not is_str(v) or not v:
            issues.append(Issue("FATAL", case,
                                "缺少必填字符串字段 %s" % f))
            ok_fields = False
    mf = obj.get("map_file")
    if not is_str(mf) or not mf:
        if is_str(default_map) and default_map:
            mf = default_map
        else:
            issues.append(Issue("FATAL", case, "缺少 map_file（且套件无默认值）"))
            ok_fields = False
    preset = obj.get("preset")
    actions = obj.get("actions")
    expected = obj.get("expected")
    if not isinstance(preset, dict) or not isinstance(actions, list) or \
            not isinstance(expected, dict):
        issues.append(Issue("FATAL", case,
                            "preset 必须为对象、actions 必须为数组、expected 必须为对象"))
        return set()

    # expected_outcome / expected_error（规范 17）
    eo = obj.get("expected_outcome", "SUCCESS")
    if eo not in ("SUCCESS", "ERROR"):
        issues.append(Issue("FATAL", case,
                            "expected_outcome 必须为 SUCCESS/ERROR"))
    ee = obj.get("expected_error")
    ee_code = None
    ee_ai = None
    ee_path = None
    if ee is not None:
        if not isinstance(ee, dict):
            issues.append(Issue("FATAL", case, "expected_error 必须为对象"))
        else:
            if not is_str(ee.get("code")):
                issues.append(Issue("WARN", case, "expected_error.code 应为字符串"))
            elif ee.get("code"):
                ee_code = ee["code"]
            ai = ee.get("action_index")
            if ai is not None and not is_int(ai):
                issues.append(Issue("WARN", case,
                                    "expected_error.action_index 应为整数"))
            elif is_int(ai):
                ee_ai = ai
            ep = ee.get("path")
            if ep is not None and not is_str(ep):
                issues.append(Issue("WARN", case,
                                    "expected_error.path 应为字符串"))
            elif is_str(ep):
                ee_path = ep
        if eo != "ERROR":
            issues.append(Issue("WARN", case,
                                "出现 expected_error 但 expected_outcome 不是 ERROR"))

    neg_preset = (eo == "ERROR" and ee_code == "INVALID_PRESET" and
                  (ee_ai is None or ee_ai == -1) and
                  (ee_path is None or ee_path == "preset"))
    neg_map = (eo == "ERROR" and ee_code == "INVALID_MAP" and
               (ee_ai is None or ee_ai == -1) and
               (ee_path is None or ee_path == "map_file"))
    neg_action = (eo == "ERROR" and
                  ee_code in ("INVALID_COMMAND", "INVALID_PARAMS",
                              "INVALID_PHASE", "RANDOM_SEQUENCE_EMPTY",
                              "RANDOM_VALUE_OUT_OF_RANGE",
                              "ACTION_AFTER_END") and
                  (ee_ai is None or ee_ai >= 0))

    # 镜像运行器顺序：preset 校验 -> 地图加载 -> actions -> expected
    sub = []
    users = preset.get("users") if isinstance(preset, dict) else None

    # 1) preset 深度校验（与地图无关）
    preset_end = len(sub)
    check_preset(preset, None, sub, case)
    preset_findings = sub[:preset_end]
    preset_issue_found = any(x.level in ("FATAL", "ERROR")
                             for x in preset_findings)

    if preset_issue_found and not neg_preset:
        # 运行器在此报 INVALID_PRESET ERROR，后续不再执行
        issues.extend(preset_findings)
        return set()
    if neg_preset:
        issues.append(Issue("INFO", case,
                            "负向用例：preset 违规被 expected_error(INVALID_PRESET) 声明"
                            if preset_issue_found else
                            "声明 expected INVALID_PRESET，但静态未发现 preset 违规"
                            "（运行可能不按预期失败）"))
        return set()

    # 2) 地图加载
    map_types = None
    if ok_fields and is_str(mf):
        if neg_map:
            issues.append(Issue("INFO", case,
                                "负向用例：期望 INVALID_MAP（map_file 由运行器验证）"))
            return set()
        before = len(sub)
        map_types = load_map_types(root_dir, mf, sub, case)
        if map_types is None:
            issues.extend(sub)
            return set()
    # preset 正常但属性位置非地产格的跨检查（警告级）
    if map_types is not None and isinstance(preset, dict):
        for i, pp in enumerate(preset.get("properties") or []):
            if isinstance(pp, dict) and is_int(pp.get("position")) and \
                    map_types.get(pp["position"]) not in ("LAND_1", "LAND_2",
                                                          "LAND_3", None):
                sub.append(Issue("WARN", case,
                                 "preset.properties 位置 %d 非地产格(%s)"
                                 % (pp["position"], map_types.get(pp["position"]))))

    # 3) actions 形式级校验
    if neg_action:
        issues.append(Issue("INFO", case,
                            "负向用例：期望 %s @actions[%s]（运行器验证）"
                            % (ee_code, ee_ai if ee_ai is not None else "?")))
        if ee_ai is not None:
            # 期望失败下标之前的 action 若有形式错误 => 真实问题（会提前失败）；
            # 下标处错误即触发点（豁免），下标之后不会被执行
            check_actions(actions, sub, case,
                          intended_err_idx=ee_ai, limit=ee_ai)
        else:
            # 未指定下标：任一 action 失败即按 code 匹配，全部交给运行器
            pass
    else:
        check_actions(actions, sub, case)

    # 4) expected：负向用例不参与比较
    if eo != "ERROR":
        check_expected(expected, users, sub, case, map_types)

    issues.extend(sub)
    return set()


def audit_file(path):
    print("=" * 78)
    print("FILE: %s" % path)
    issues = []
    try:
        with open(path, "rb") as f:
            raw = f.read()
        if raw.startswith(b"\xef\xbb\xbf"):
            issues.append(Issue("FATAL", "<file>",
                                "文件带 UTF-8 BOM（规范 2.3 拒绝，运行必 ERROR）"))
            text = raw[3:].decode("utf-8", "replace")
        else:
            text = raw.decode("utf-8")
        data = json.loads(text)
    except Exception as e:
        issues.append(Issue("FATAL", "<file>", "JSON 无法解析: %s" % e))
        _dump(issues)
        return 0, issues

    if not isinstance(data, dict):
        issues.append(Issue("FATAL", "<file>", "顶层必须是 JSON 对象"))
        _dump(issues)
        return 0, issues

    root_dir = os.path.dirname(os.path.abspath(path))
    suite_map = data.get("map_file")

    if "tests" in data:                       # 套件文件
        sv = data.get("schema_version")
        if sv != "2.0":
            issues.append(Issue("FATAL", "<file>",
                                "套件 schema_version 必须为 '2.0'，实际: %s" % sv))
            _dump(issues)
            return 0, issues
        tests = data.get("tests")
        if not isinstance(tests, list):
            issues.append(Issue("FATAL", "<file>", "tests 必须为数组"))
            _dump(issues)
            return 0, issues
        if not tests:
            issues.append(Issue("FATAL", "<file>", "tests 不能为空数组"))
        seen_ids = {}
        n_total = len(tests)
        for i, obj in enumerate(tests):
            if isinstance(obj, dict):
                cid = obj.get("case_id")
                if is_str(cid):
                    seen_ids.setdefault(cid, []).append(i + 1)
            check_case(i, obj, sv, suite_map if is_str(suite_map) else None,
                       root_dir, issues)
        for cid, pos_list in seen_ids.items():
            if len(pos_list) > 1:
                issues.append(Issue(
                    "FATAL", cid,
                    "套件内重复 case_id（出现于第 %s 项）" % ", ".join(map(str, pos_list))))
    else:                                      # 单用例文件
        check_case(0, data, "2.0", None, root_dir, issues)

    _dump(issues)
    print("FILE TOTAL: %d 项（FATAL=%d ERROR=%d WARN=%d INFO=%d）"
          % (len(issues),
             sum(1 for x in issues if x.level == "FATAL"),
             sum(1 for x in issues if x.level == "ERROR"),
             sum(1 for x in issues if x.level == "WARN"),
             sum(1 for x in issues if x.level == "INFO")))
    return len(issues), issues


def _dump(issues):
    from collections import Counter
    c = Counter(x.level for x in issues)
    print("  问题统计: FATAL=%d ERROR=%d WARN=%d INFO=%d"
          % (c["FATAL"], c["ERROR"], c["WARN"], c["INFO"]))
    for lv in ("FATAL", "ERROR", "WARN"):
        sub = [x for x in issues if x.level == lv]
        if not sub:
            continue
        print("  ---- %s 类问题（前 12 条） ----" % lv)
        for x in sub[:12]:
            print("   %s" % x)
        if len(sub) > 12:
            print("   ... 其余 %d 条略" % (len(sub) - 12))


def main(argv):
    target = argv[1] if len(argv) > 1 else "testcases"
    if os.path.isdir(target):
        files = sorted(f for f in os.listdir(target)
                       if f.lower().endswith(".json") and f != "map.json")
    else:
        files = [target]
    grand = {"FATAL": 0, "ERROR": 0, "WARN": 0, "INFO": 0}
    for f in files:
        path = os.path.join(target, f) if os.path.isdir(target) else f
        if not os.path.isfile(path):
            print("跳过不存在文件: %s" % path)
            continue
        _, issues = audit_file(path)
        from collections import Counter
        c = Counter(x.level for x in issues)
        for k in grand:
            grand[k] += c[k]
    print("=" * 78)
    print("合计: 文件 %d 个；FATAL=%d ERROR=%d WARN=%d INFO=%d"
          % (len(files), grand["FATAL"], grand["ERROR"], grand["WARN"],
             grand["INFO"]))


if __name__ == "__main__":
    main(sys.argv)
