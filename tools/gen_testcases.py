# -*- coding: utf-8 -*-
"""生成 Group3 v2.0 完备测试套件（《大富翁游戏自动化测试JSON接口规范v2.0》）。

覆盖：
  - 公园 PARK（14/49/63，到达/经过无事件、display_cells）
  - STEP 0~2147483647（负数/小数/字符串非法；0 原地结束回合；70 整圈；71→1；140→0；2147483647→57）
  - 命令 ASCII 大小写不敏感
  - 地图财神完整生命周期（首次生成/候选回退/自然失效/领取/当轮免租/再生成/随机流错误）
  - ADVANCE_TURN、turn_number、fortune_assert、fields_absent、display_cells
  - 删除功能负向（BOMB/HOSPITAL/JAIL/remaining_rounds/dice_sequence/非法地图）
  - 原有规则回归（购买/升级/租金/破产/路障/机器人/礼品屋/道具屋/出售/QUIT 等）
输出：Richman_play/testcases/Group3_Testcases.json（schema_version 2.0 套件）
"""
import json
import os

def find_repo_root():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parent = os.path.dirname(script_dir)
    if os.path.isdir(os.path.join(parent, "testcases")) and \
       os.path.isdir(os.path.join(parent, "src")):
        return parent
    return os.path.join(script_dir, "Richman_play")

SRC = os.path.join(find_repo_root(), "testcases", "Group3_Testcases.json")

# ---------- helpers ----------

def P(id, fund=1000, credit=0, pos=0, status="NORMAL", items=(0, 0), god=0):
    return {"id": id, "fund": fund, "credit": credit, "position": pos,
            "status": status,
            "items": {"BLOCK": items[0], "ROBOT": items[1]},
            "god_of_wealth_rounds": god}

def FORTUNE(position=None, spawned=None, remaining=0, next_spawn=None):
    return {"position": position, "spawned_after_turn": spawned,
            "remaining_map_turns": remaining, "next_spawn_after_turn": next_spawn}

def PR(players, users=None, current="A", properties=None, map_items=None,
       tn=1, fortune=None, rng=None):
    if users is None:
        users = [p["id"] for p in players]
    preset = {"users": users, "current_user": current, "phase": "COMMAND",
              "game_status": "RUNNING", "turn_number": tn,
              "players": players,
              "properties": properties or [], "map_items": map_items or [],
              "fortune": fortune if fortune is not None else FORTUNE()}
    if rng is not None:
        preset["random_control"] = rng
    return preset

def SEQ(**streams):
    return {"mode": "SEQUENCE", "streams": streams}

def PROP(pos, owner, level):
    return {"position": pos, "owner": owner, "level": level}

def ITEM(pos, kind="BLOCK"):
    return {"position": pos, "type": kind}

def C(cid, name, preset, actions, expected, ee=None):
    c = {"case_id": cid, "case_name": name,
         "preset": preset, "actions": actions, "expected": expected}
    if ee is not None:
        c["expected_outcome"] = "ERROR"
        c["expected_error"] = ee
    return c

def A(cmd, **params):
    if params:
        return {"command": cmd, "params": params}
    return {"command": cmd}

def ANS(v):
    return A("ANSWER", value=v)

def XORSHIFT32(seed, count):
    """计算 XORSHIFT32 序列（规范 7.1）"""
    x = seed
    out = []
    for _ in range(count):
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= (x >> 17)
        x ^= (x << 5) & 0xFFFFFFFF
        x &= 0xFFFFFFFF
        out.append(x)
    return out

NEW = []

# ==================== A. 公园 PARK ====================
NEW.append(C("TC-V2-PARK-001", "到达14号公园无任何效果",
    PR([P("A", pos=13), P("Q", pos=20)]),
    [A("STEP", steps=1)],
    {"current_user": "Q", "phase": "COMMAND", "pending_prompt": None,
     "turn_number": 2,
     "players": [{"id": "A", "position": 14, "fund": 1000, "credit": 0,
                  "status": "NORMAL", "god_of_wealth_rounds": 0}],
     "display_cells": [{"position": 14, "base_type": "PARK", "base_symbol": "P",
                        "visible_symbol": "A", "visible_entity": "PLAYER"}]}))

NEW.append(C("TC-V2-PARK-002", "到达49号公园无任何效果",
    PR([P("A", pos=48), P("Q", pos=20)]),
    [A("STEP", steps=1)],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 49, "fund": 1000, "credit": 0,
                  "status": "NORMAL"}]}))

NEW.append(C("TC-V2-PARK-003", "到达63号公园无任何效果",
    PR([P("A", pos=62), P("Q", pos=20)]),
    [A("STEP", steps=1)],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 63, "fund": 1000, "credit": 0,
                  "status": "NORMAL"}]}))

NEW.append(C("TC-V2-PARK-004", "经过公园不停留继续移动至普通地产",
    PR([P("A", pos=47), P("Q", pos=20)]),
    [A("STEP", steps=3), ANS("N")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 50, "fund": 1000}],
     "properties_absent": [50]}))

NEW.append(C("TC-V2-PARK-005", "公园不产生pending_prompt且正常结束回合",
    PR([P("A", pos=13), P("Q", pos=20)]),
    [A("STEP", steps=1)],
    {"current_user": "Q", "phase": "COMMAND", "pending_prompt": None,
     "turn_number": 2,
     "players": [{"id": "A", "position": 14, "god_of_wealth_rounds": 0}]}))

NEW.append(C("TC-V2-PARK-006", "离开公园后可见符号恢复P",
    PR([P("A", pos=12), P("Q", pos=20)]),
    [A("STEP", steps=4), ANS("N")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 16, "fund": 1000}],
     "properties_absent": [16],
     "display_cells": [{"position": 14, "base_symbol": "P",
                        "visible_symbol": "P", "visible_entity": "BASE"}]}))

# ==================== B. STEP 规则 ====================
NEW.append(C("TC-V2-STEP-001", "STEP 0合法：原地结束回合且不触发落点（小写step同样生效）",
    PR([P("A", pos=1), P("Q", pos=20)], properties=[PROP(1, "A", 0)]),
    [A("Step", steps=0)],
    {"current_user": "Q", "turn_number": 2, "phase": "COMMAND",
     "pending_prompt": None,
     "players": [{"id": "A", "position": 1, "fund": 1000}],
     "properties": [PROP(1, "A", 0)]}))

NEW.append(C("TC-V2-STEP-002", "STEP负数非法",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=-1)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0,
        "path": "actions[0].params.steps"}))

NEW.append(C("TC-V2-STEP-003", "STEP小数字符串非法",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=1.5)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0,
        "path": "actions[0].params.steps"}))

NEW.append(C("TC-V2-STEP-004", "STEP参数字符串非法",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps="5")], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0,
        "path": "actions[0].params.steps"}))

NEW.append(C("TC-V2-STEP-005", "STEP缺少steps参数非法",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP")], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0,
        "path": "actions[0].params.steps"}))

NEW.append(C("TC-V2-STEP-006", "STEP1移动一格",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=1), ANS("N")],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 1}], "properties_absent": [1]}))

NEW.append(C("TC-V2-STEP-007", "STEP69从起点到69",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=69)],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 69, "credit": 20}]}))

NEW.append(C("TC-V2-STEP-008", "STEP70移动整圈回到原位",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=70)],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 0}]}))

NEW.append(C("TC-V2-STEP-009", "STEP71对70取余移动1格",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=71), ANS("N")],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 1}], "properties_absent": [1]}))

NEW.append(C("TC-V2-STEP-010", "STEP140取余为0原地结束回合不触发落点",
    PR([P("A", pos=1), P("Q", pos=20)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=140)],
    {"current_user": "Q", "turn_number": 2, "phase": "COMMAND",
     "pending_prompt": None,
     "players": [{"id": "A", "position": 1, "fund": 1000}],
     "properties": [PROP(1, "A", 0)]}))

NEW.append(C("TC-V2-STEP-011", "STEP2147483647取余57",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=2147483647), ANS("N")],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 57}], "properties_absent": [57]}))

NEW.append(C("TC-V2-STEP-012", "STEP100取余30",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=100), ANS("N")],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 30}], "properties_absent": [30]}))

NEW.append(C("TC-V2-STEP-013", "小写step命令不区分大小写",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("step", steps=2), ANS("N")],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 2}], "properties_absent": [2]}))

NEW.append(C("TC-V2-STEP-014", "混合大小写sTeP命令生效",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("sTeP", steps=3), ANS("N")],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 3}], "properties_absent": [3]}))

NEW.append(C("TC-V2-STEP-015", "取余0的STEP仍推进计时器并扣减财神回合",
    PR([P("A", pos=1, god=3), P("Q", pos=20)]),
    [A("STEP", steps=140)],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 1, "god_of_wealth_rounds": 2}]}))

NEW.append(C("TC-V2-STEP-016", "STEP 0在自己地产上不触发升级提示",
    PR([P("A", pos=1), P("Q", pos=20)], properties=[PROP(1, "A", 1)]),
    [A("STEP", steps=0)],
    {"current_user": "Q", "turn_number": 2, "phase": "COMMAND",
     "pending_prompt": None,
     "players": [{"id": "A", "position": 1}],
     "properties": [PROP(1, "A", 1)]}))

# ==================== C. 命令大小写 ====================
NEW.append(C("TC-V2-CMD-001", "小写roll消耗DICE流",
    PR([P("A", pos=0), P("Q", pos=20)],
       rng=SEQ(DICE=[3])),
    [A("roll"), ANS("N")],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 3}], "properties_absent": [3]}))

NEW.append(C("TC-V2-CMD-002", "小写quit结束游戏并清空提示",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("quit")],
    {"game_status": "FINISHED", "phase": "ENDED", "winner": None,
     "pending_prompt": None}))

NEW.append(C("TC-V2-CMD-003", "混合大小写Sell出售成功",
    PR([P("A", pos=0), P("Q", pos=40)], properties=[PROP(1, "A", 0)]),
    [A("Sell", position=1)],
    {"current_user": "A",
     "players": [{"id": "A", "fund": 1400}],
     "properties_absent": [1]}))

NEW.append(C("TC-V2-CMD-004", "小写block放置路障",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)]),
    [A("block", offset=1)],
    {"current_user": "A",
     "players": [{"id": "A", "items": {"BLOCK": 0, "ROBOT": 0}}],
     "map_items": [ITEM(21)]}))

NEW.append(C("TC-V2-CMD-005", "小写robot清除前方路障",
    PR([P("A", pos=60, items=(0, 1)), P("Q", pos=40)],
       map_items=[ITEM(61), ITEM(62)]),
    [A("robot")],
    {"current_user": "A", "map_items_absent": [61, 62]}))

NEW.append(C("TC-V2-CMD-006", "小写query不改变状态",
    PR([P("A", pos=5), P("Q", pos=20)]),
    [A("query")],
    {"current_user": "A", "phase": "COMMAND",
     "players": [{"id": "A", "position": 5}]}))

NEW.append(C("TC-V2-CMD-007", "小写help不改变状态",
    PR([P("A", pos=5), P("Q", pos=20)]),
    [A("help")],
    {"current_user": "A", "phase": "COMMAND",
     "players": [{"id": "A", "position": 5}]}))

NEW.append(C("TC-V2-CMD-008", "BOMB命令返回INVALID_COMMAND（删除项）",
    PR([P("A", pos=20), P("Q", pos=40)]),
    [A("BOMB", offset=1)], {},
    ee={"code": "INVALID_COMMAND", "action_index": 0}))

NEW.append(C("TC-V2-CMD-009", "小写bomb同样返回INVALID_COMMAND",
    PR([P("A", pos=20), P("Q", pos=40)]),
    [A("bomb", offset=1)], {},
    ee={"code": "INVALID_COMMAND", "action_index": 0}))

# ==================== D. 财神生命周期 ====================
NEW.append(C("TC-V2-FORTUNE-001", "完成第10回合后首次生成财神（候选为道具屋时回退，规范21.1回归）",
    PR([P("A", pos=14), P("Q", pos=49)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[28, 20])),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune": {"position": 20, "symbol": "F", "spawned_after_turn": 10,
                 "remaining_map_turns": 5, "next_spawn_after_turn": None},
     "display_cells": [{"position": 20, "base_type": "LAND_1",
                        "visible_symbol": "F", "visible_entity": "FORTUNE"}]}))

NEW.append(C("TC-V2-FORTUNE-002", "候选格有玩家时回退到下一合格位置",
    PR([P("A", pos=20), P("Q", pos=49)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[20, 5])),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune": {"position": 5, "remaining_map_turns": 5}}))

NEW.append(C("TC-V2-FORTUNE-003", "候选格有路障时回退到下一合格位置",
    PR([P("A", pos=14), P("Q", pos=49)], tn=10, map_items=[ITEM(20)],
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[20, 8])),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune": {"position": 8, "remaining_map_turns": 5},
     "map_items": [ITEM(20)]}))

NEW.append(C("TC-V2-FORTUNE-004", "候选格为礼品屋时回退",
    PR([P("A", pos=14), P("Q", pos=49)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[35, 9])),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune": {"position": 9, "remaining_map_turns": 5}}))

NEW.append(C("TC-V2-FORTUNE-005", "财神可生成在公园",
    PR([P("A", pos=20), P("Q", pos=40)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[14])),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune": {"position": 14, "remaining_map_turns": 5},
     "display_cells": [{"position": 14, "base_type": "PARK", "base_symbol": "P",
                        "visible_symbol": "F", "visible_entity": "FORTUNE"}]}))

NEW.append(C("TC-V2-FORTUNE-006", "财神可生成在起点",
    PR([P("A", pos=20), P("Q", pos=40)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[0])),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune": {"position": 0, "remaining_map_turns": 5}}))

NEW.append(C("TC-V2-FORTUNE-007", "财神可生成在矿地",
    PR([P("A", pos=20), P("Q", pos=40)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[64])),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune": {"position": 64, "remaining_map_turns": 5}}))

NEW.append(C("TC-V2-FORTUNE-008", "财神5回合未领取自然消失并按延迟计划再生成",
    PR([P("A", pos=14), P("Q", pos=49)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[20], FORTUNE_RESPAWN_DELAY=[7])),
    [A("ADVANCE_TURN"), A("ADVANCE_TURN"), A("ADVANCE_TURN"),
     A("ADVANCE_TURN"), A("ADVANCE_TURN"), A("ADVANCE_TURN")],
    {"turn_number": 16,
     "fortune": {"position": None, "symbol": None, "spawned_after_turn": None,
                 "remaining_map_turns": 0, "next_spawn_after_turn": 22},
     "display_cells": [{"position": 20, "base_type": "LAND_1",
                        "visible_symbol": "0", "visible_entity": "BASE"}]}))

NEW.append(C("TC-V2-FORTUNE-009", "生成计划到期时再生成新财神",
    PR([P("A", pos=14), P("Q", pos=49)], tn=20,
       fortune=FORTUNE(next_spawn=22),
       rng=SEQ(FORTUNE_POSITION=[10])),
    [A("ADVANCE_TURN"), A("ADVANCE_TURN"), A("ADVANCE_TURN")],
    {"turn_number": 23,
     "fortune": {"position": 10, "spawned_after_turn": 22,
                 "remaining_map_turns": 5, "next_spawn_after_turn": None}}))

NEW.append(C("TC-V2-FORTUNE-010", "经过F后落在他人地产立即免租（规范21.2回归）",
    PR([P("Q", pos=4), P("A", pos=20)], users=["Q", "A"], current="Q", tn=11,
       properties=[PROP(6, "A", 0)],
       fortune=FORTUNE(position=5, spawned=10, remaining=5),
       rng=SEQ(FORTUNE_RESPAWN_DELAY=[3])),
    [A("STEP", steps=2)],
    {"turn_number": 12, "current_user": "A",
     "players": [{"id": "Q", "position": 6, "fund": 1000,
                  "god_of_wealth_rounds": 5},
                 {"id": "A", "fund": 1000}],
     "fortune": {"position": None, "symbol": None,
                 "remaining_map_turns": 0, "next_spawn_after_turn": 14},
     "display_cells": [{"position": 5, "visible_entity": "BASE"}]}))

NEW.append(C("TC-V2-FORTUNE-011", "领取当回合不递减下一回合递减",
    PR([P("Q", pos=4), P("A", pos=20)], users=["Q", "A"], current="Q", tn=11,
       properties=[PROP(6, "A", 0)],
       fortune=FORTUNE(position=5, spawned=10, remaining=5),
       rng=SEQ(FORTUNE_RESPAWN_DELAY=[3])),
    [A("STEP", steps=2), A("ADVANCE_TURN"), A("ADVANCE_TURN")],
    {"turn_number": 14, "current_user": "A",
     "players": [{"id": "Q", "position": 6, "god_of_wealth_rounds": 4},
                 {"id": "A", "position": 20}]}))

NEW.append(C("TC-V2-FORTUNE-012", "再次获得地图财神重置为5回合",
    PR([P("Q", pos=4, god=2), P("A", pos=20)], users=["Q", "A"], current="Q", tn=11,
       properties=[PROP(6, "A", 0)],
       fortune=FORTUNE(position=5, spawned=10, remaining=5),
       rng=SEQ(FORTUNE_RESPAWN_DELAY=[2])),
    [A("STEP", steps=2)],
    {"turn_number": 12, "current_user": "A",
     "players": [{"id": "Q", "position": 6, "god_of_wealth_rounds": 5}],
     "fortune": {"position": None, "next_spawn_after_turn": 13}}))

NEW.append(C("TC-V2-FORTUNE-013", "未到达财神格则财神保留",
    PR([P("Q", pos=3), P("A", pos=20)], users=["Q", "A"], current="Q", tn=11,
       fortune=FORTUNE(position=5, spawned=10, remaining=5)),
    [A("STEP", steps=1), ANS("N")],
    {"turn_number": 12, "current_user": "A",
     "players": [{"id": "Q", "position": 4, "god_of_wealth_rounds": 0}],
     "fortune": {"position": 5, "remaining_map_turns": 4},
     "properties_absent": [4]}))

NEW.append(C("TC-V2-FORTUNE-014", "路障拦截在财神之前则不领取",
    PR([P("Q", pos=3), P("A", pos=20)], users=["Q", "A"], current="Q", tn=11,
       map_items=[ITEM(4)],
       fortune=FORTUNE(position=5, spawned=10, remaining=5)),
    [A("STEP", steps=2), ANS("N")],
    {"turn_number": 12, "current_user": "A",
     "players": [{"id": "Q", "position": 4, "god_of_wealth_rounds": 0}],
     "fortune": {"position": 5, "remaining_map_turns": 4},
     "map_items_absent": [4], "properties_absent": [4]}))

NEW.append(C("TC-V2-FORTUNE-015", "拾取后生成的财神可被另一玩家拾取",
    PR([P("A", pos=6), P("Q", pos=6)], users=["A", "Q"], current="A", tn=14,
       fortune=FORTUNE(next_spawn=14),
       rng=SEQ(FORTUNE_POSITION=[7], FORTUNE_RESPAWN_DELAY=[5])),
    [A("ADVANCE_TURN"), A("STEP", steps=1), ANS("N")],
    {"turn_number": 16, "current_user": "A",
     "players": [{"id": "Q", "position": 7, "god_of_wealth_rounds": 5},
                 {"id": "A", "position": 6}],
     "fortune": {"position": None, "next_spawn_after_turn": 20},
     "properties_absent": [7]}))

NEW.append(C("TC-V2-FORTUNE-016", "FORTUNE_POSITION流耗尽返回RANDOM_SEQUENCE_EMPTY",
    PR([P("A", pos=14), P("Q", pos=49)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[])),
    [A("ADVANCE_TURN")], {},
    ee={"code": "RANDOM_SEQUENCE_EMPTY", "action_index": 0}))

NEW.append(C("TC-V2-FORTUNE-017", "FORTUNE_POSITION越界返回RANDOM_VALUE_OUT_OF_RANGE",
    PR([P("A", pos=14), P("Q", pos=49)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[70])),
    [A("ADVANCE_TURN")], {},
    ee={"code": "RANDOM_VALUE_OUT_OF_RANGE", "action_index": 0}))

NEW.append(C("TC-V2-FORTUNE-018", "自然失效时延迟流耗尽返回RANDOM_SEQUENCE_EMPTY",
    PR([P("A", pos=14), P("Q", pos=49)], tn=11,
       fortune=FORTUNE(position=20, spawned=10, remaining=1),
       rng=SEQ(FORTUNE_RESPAWN_DELAY=[])),
    [A("ADVANCE_TURN")], {},
    ee={"code": "RANDOM_SEQUENCE_EMPTY", "action_index": 0}))

NEW.append(C("TC-V2-FORTUNE-019", "延迟流越界返回RANDOM_VALUE_OUT_OF_RANGE",
    PR([P("A", pos=14), P("Q", pos=49)], tn=11,
       fortune=FORTUNE(position=20, spawned=10, remaining=1),
       rng=SEQ(FORTUNE_RESPAWN_DELAY=[11])),
    [A("ADVANCE_TURN")], {},
    ee={"code": "RANDOM_VALUE_OUT_OF_RANGE", "action_index": 0}))

NEW.append(C("TC-V2-FORTUNE-020", "领取时延迟流耗尽返回RANDOM_SEQUENCE_EMPTY",
    PR([P("Q", pos=4), P("A", pos=20)], users=["Q", "A"], current="Q", tn=11,
       fortune=FORTUNE(position=5, spawned=10, remaining=5),
       rng=SEQ(FORTUNE_RESPAWN_DELAY=[])),
    [A("STEP", steps=2)], {},
    ee={"code": "RANDOM_SEQUENCE_EMPTY", "action_index": 0}))

NEW.append(C("TC-V2-FORTUNE-021", "DICE流耗尽ROLL返回RANDOM_SEQUENCE_EMPTY",
    PR([P("A", pos=0), P("Q", pos=20)], rng=SEQ(DICE=[])),
    [A("ROLL")], {},
    ee={"code": "RANDOM_SEQUENCE_EMPTY", "action_index": 0}))

NEW.append(C("TC-V2-FORTUNE-022", "DICE流越界返回RANDOM_VALUE_OUT_OF_RANGE",
    PR([P("A", pos=0), P("Q", pos=20)], rng=SEQ(DICE=[7])),
    [A("ROLL")], {},
    ee={"code": "RANDOM_VALUE_OUT_OF_RANGE", "action_index": 0}))

NEW.append(C("TC-V2-FORTUNE-023", "ROLL按DICE流顺序消耗",
    PR([P("A", pos=13), P("Q", pos=20)], rng=SEQ(DICE=[3, 5])),
    [A("ROLL"), ANS("N"), A("ROLL"), ANS("N")],
    {"turn_number": 3, "current_user": "A",
     "players": [{"id": "A", "position": 16}, {"id": "Q", "position": 25}],
     "properties_absent": [16, 25]}))

NEW.append(C("TC-V2-FORTUNE-024", "PRNG XORSHIFT32骰子确定性",
    PR([P("A", pos=0), P("Q", pos=20)],
       rng={"mode": "PRNG", "algorithm": "XORSHIFT32",
            "stream_seeds": {"DICE": 12345}}),
    [A("ROLL"), ANS("N")],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 1 + XORSHIFT32(12345, 1)[0] % 6}],
     "properties_absent": [1 + XORSHIFT32(12345, 1)[0] % 6]}))

NEW.append(C("TC-V2-FORTUNE-025", "fortune_assert综合断言（存在/范围/占用/道具）",
    PR([P("A", pos=14), P("Q", pos=49)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng=SEQ(FORTUNE_POSITION=[28, 35, 20])),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune_assert": {"present": True, "position_between": [0, 69],
                        "position_not_in": [28, 35], "unoccupied": True,
                        "without_map_item": True},
     "fortune": {"remaining_map_turns": 5}}))

NEW.append(C("TC-V2-FORTUNE-026", "ADVANCE_TURN原地推进回合并扣减财神回合",
    PR([P("A", pos=20, god=3), P("Q", pos=49)], users=["A", "Q"], current="A", tn=5),
    [A("ADVANCE_TURN")],
    {"turn_number": 6, "current_user": "Q",
     "players": [{"id": "A", "position": 20, "god_of_wealth_rounds": 2}]}))

NEW.append(C("TC-V2-FORTUNE-027", "ADVANCE_TURN不触发移动与落点",
    PR([P("A", pos=1), P("Q", pos=49)], users=["A", "Q"], current="A", tn=5,
       properties=[PROP(1, "A", 0)]),
    [A("ADVANCE_TURN")],
    {"turn_number": 6, "current_user": "Q", "phase": "COMMAND",
     "pending_prompt": None,
     "players": [{"id": "A", "position": 1, "fund": 1000}],
     "properties": [PROP(1, "A", 0)]}))

NEW.append(C("TC-V2-FORTUNE-028", "PROMPT阶段ADVANCE_TURN报INVALID_PHASE",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("STEP", steps=1), A("ADVANCE_TURN")], {},
    ee={"code": "INVALID_PHASE", "action_index": 1}))

NEW.append(C("TC-V2-FORTUNE-029", "游戏结束后ADVANCE_TURN报ACTION_AFTER_END",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [A("QUIT"), A("ADVANCE_TURN")], {},
    ee={"code": "ACTION_AFTER_END", "action_index": 1}))

NEW.append(C("TC-V2-FORTUNE-030", "财神免租5回合后第6次经过恢复收费",
    PR([P("A", pos=0), P("Q", pos=34)], users=["A", "Q"], current="Q", tn=1,
       properties=[PROP(1, "A", 0), PROP(2, "A", 0), PROP(3, "A", 0),
                   PROP(4, "A", 0), PROP(5, "A", 0), PROP(6, "A", 0)]),
    [A("STEP", steps=1), ANS("3"),
     A("STEP", steps=70), A("STEP", steps=36),
     A("STEP", steps=70), A("STEP", steps=1),
     A("STEP", steps=70), A("STEP", steps=1),
     A("STEP", steps=70), A("STEP", steps=1),
     A("STEP", steps=70), A("STEP", steps=1),
     A("STEP", steps=70), A("STEP", steps=1)],
    {"current_user": "A", "turn_number": 14,
     "players": [{"id": "Q", "position": 6, "fund": 900,
                  "god_of_wealth_rounds": 0},
                 {"id": "A", "fund": 1100}]}))

# ==================== E. 删除功能负向 ====================
NEW.append(C("TC-V2-DEL-001", "preset含items.BOMB返回INVALID_PRESET",
    {"users": ["A", "Q"], "current_user": "A", "phase": "COMMAND",
     "game_status": "RUNNING", "turn_number": 1,
     "players": [{"id": "A", "fund": 1000, "credit": 0, "position": 0,
                  "status": "NORMAL", "items": {"BLOCK": 0, "BOMB": 1, "ROBOT": 0},
                  "god_of_wealth_rounds": 0},
                 {"id": "Q", "fund": 1000, "credit": 0, "position": 20,
                  "status": "NORMAL", "items": {"BLOCK": 0, "ROBOT": 0},
                  "god_of_wealth_rounds": 0}],
     "properties": [], "map_items": [], "fortune": FORTUNE()},
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-002", "map_items含BOMB返回INVALID_PRESET",
    PR([P("A", pos=0), P("Q", pos=20)], map_items=[ITEM(21, "BOMB")]),
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-003", "玩家状态HOSPITAL返回INVALID_PRESET",
    PR([P("A", pos=0, status="HOSPITAL"), P("Q", pos=20)]),
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-004", "玩家状态JAIL返回INVALID_PRESET",
    PR([P("A", pos=0, status="JAIL"), P("Q", pos=20)]),
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-005", "preset含remaining_rounds返回INVALID_PRESET",
    {"users": ["A", "Q"], "current_user": "A", "phase": "COMMAND",
     "game_status": "RUNNING", "turn_number": 1,
     "players": [{"id": "A", "fund": 1000, "credit": 0, "position": 0,
                  "status": "NORMAL", "remaining_rounds": 0,
                  "items": {"BLOCK": 0, "ROBOT": 0},
                  "god_of_wealth_rounds": 0},
                 {"id": "Q", "fund": 1000, "credit": 0, "position": 20,
                  "status": "NORMAL", "items": {"BLOCK": 0, "ROBOT": 0},
                  "god_of_wealth_rounds": 0}],
     "properties": [], "map_items": [], "fortune": FORTUNE()},
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-006", "2.0中dice_sequence返回INVALID_PRESET",
    {"users": ["A", "Q"], "current_user": "A", "phase": "COMMAND",
     "game_status": "RUNNING", "turn_number": 1,
     "players": [P("A"), P("Q", pos=20)],
     "properties": [], "map_items": [], "dice_sequence": [3],
     "fortune": FORTUNE()},
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-007", "地图含MAGIC_HOUSE返回INVALID_MAP",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [], {},
    ee={"code": "INVALID_MAP"}))

NEW.append(C("TC-V2-DEL-008", "地图含HOSPITAL返回INVALID_MAP",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [], {},
    ee={"code": "INVALID_MAP"}))

NEW.append(C("TC-V2-DEL-009", "地图含JAIL返回INVALID_MAP",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [], {},
    ee={"code": "INVALID_MAP"}))

NEW.append(C("TC-V2-DEL-010", "公园位置错误返回INVALID_MAP",
    PR([P("A", pos=0), P("Q", pos=20)]),
    [], {},
    ee={"code": "INVALID_MAP"}))

NEW.append(C("TC-V2-DEL-011", "fields_absent断言remaining_rounds不存在",
    PR([P("A", pos=5), P("Q", pos=20)]),
    [],
    {"players": [{"id": "A", "fields_absent": ["remaining_rounds"]}],
     "fields_absent": []}))

NEW.append(C("TC-V2-DEL-012", "fields_absent断言items.BOMB不存在",
    PR([P("A", pos=5), P("Q", pos=20)]),
    [],
    {"players": [{"id": "A", "items": {"fields_absent": ["BOMB"]}}]}))

NEW.append(C("TC-V2-DEL-013", "道具屋选择已删除的3号商品无效且不离开",
    PR([P("A", pos=27, credit=100), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("3")],
    {"current_user": "A", "phase": "PROMPT", "pending_prompt": "TOOL_SHOP",
     "players": [{"id": "A", "position": 28, "credit": 100,
                  "items": {"BLOCK": 0, "ROBOT": 0}}]}))

NEW.append(C("TC-V2-DEL-014", "turn_number缺失返回INVALID_PRESET",
    {"users": ["A", "Q"], "current_user": "A", "phase": "COMMAND",
     "game_status": "RUNNING",
     "players": [P("A"), P("Q", pos=20)],
     "properties": [], "map_items": [], "fortune": FORTUNE()},
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-015", "fortune缺失返回INVALID_PRESET",
    {"users": ["A", "Q"], "current_user": "A", "phase": "COMMAND",
     "game_status": "RUNNING", "turn_number": 1,
     "players": [P("A"), P("Q", pos=20)],
     "properties": [], "map_items": []},
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-016", "fortune与生成计划冲突返回INVALID_PRESET",
    PR([P("A", pos=0), P("Q", pos=20)],
       fortune=FORTUNE(position=5, spawned=10, remaining=5, next_spawn=20)),
    [], {},
    ee={"code": "INVALID_PRESET"}))

NEW.append(C("TC-V2-DEL-017", "god_of_wealth_rounds越界返回INVALID_PRESET",
    PR([P("A", pos=0, god=6), P("Q", pos=20)]),
    [], {},
    ee={"code": "INVALID_PRESET"}))

# ==================== F. 原有规则回归 ====================

# US06 查询
NEW.append(C("TC-US06-001", "QUERY不改变状态且不结束回合",
    PR([P("A", 1000, 50, 5, items=(1, 1), god=3), P("Q", 1000, 0, 20)]),
    [A("QUERY")],
    {"current_user": "A", "phase": "COMMAND",
     "players": [{"id": "A", "fund": 1000, "credit": 50, "position": 5,
                  "god_of_wealth_rounds": 3}]}))
NEW.append(C("TC-US06-002", "PROMPT阶段QUERY保留提示且不影响流程",
    PR([P("A", 1000, 0, 0), P("Q", 1000, 0, 20)]),
    [A("STEP", steps=1), A("QUERY"), ANS("N")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 1}],
     "properties_absent": [1]}))
NEW.append(C("TC-US06-003", "连续QUERY两次状态不变",
    PR([P("A", 1000, 50, 5), P("Q", 1000, 0, 20)]),
    [A("QUERY"), A("QUERY")],
    {"current_user": "A", "phase": "COMMAND",
     "players": [{"id": "A", "fund": 1000, "credit": 50, "position": 5}]}))

# US08 地块事件（公园化）
NEW.append(C("TC-US08-002", "正常到达起点无效果",
    PR([P("A", pos=69), P("Q", pos=20)]),
    [A("STEP", steps=1)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 0, "fund": 1000, "credit": 0}]}))
NEW.append(C("TC-US08-003", "经过矿地不停留不获得点数",
    PR([P("A", pos=66), P("Q", pos=20)]),
    [A("STEP", steps=4)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 0, "credit": 0}]}))
NEW.append(C("TC-US08-004", "经过公园不停留无效果",
    PR([P("A", pos=12), P("Q", pos=20)]),
    [A("STEP", steps=4), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 16, "status": "NORMAL"}],
     "properties_absent": [16]}))

# US10 租金
NEW.append(C("TC-US10-001", "地段一等级0租金100",
    PR([P("A"), P("Q", pos=0)], current="Q", properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 900, "position": 1},
                 {"id": "A", "fund": 1100}]}))
NEW.append(C("TC-US10-002", "地段二等级0租金250",
    PR([P("A"), P("Q", pos=28)], current="Q", properties=[PROP(29, "A", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 750, "position": 29},
                 {"id": "A", "fund": 1250}]}))
NEW.append(C("TC-US10-003", "地段三等级0租金150",
    PR([P("A"), P("Q", pos=35)], current="Q", properties=[PROP(36, "A", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 850, "position": 36},
                 {"id": "A", "fund": 1150}]}))
NEW.append(C("TC-US10-004", "地段一等级3租金400",
    PR([P("A"), P("Q", pos=0)], current="Q", properties=[PROP(1, "A", 3)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 600, "position": 1},
                 {"id": "A", "fund": 1400}]}))
NEW.append(C("TC-US10-005", "地段二等级3租金1000资金归0不破产",
    PR([P("A"), P("Q", pos=28)], current="Q", properties=[PROP(29, "A", 3)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 0, "position": 29, "status": "NORMAL"},
                 {"id": "A", "fund": 2000}]}))
NEW.append(C("TC-US10-006", "地段三等级3租金600",
    PR([P("A"), P("Q", pos=35)], current="Q", properties=[PROP(36, "A", 3)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 400, "position": 36},
                 {"id": "A", "fund": 1600}]}))
NEW.append(C("TC-US10-007", "地段一等级1租金200",
    PR([P("A"), P("Q", pos=0)], current="Q", properties=[PROP(1, "A", 1)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 800, "position": 1},
                 {"id": "A", "fund": 1200}]}))
NEW.append(C("TC-US10-010", "财神免收租金",
    PR([P("A"), P("Q", pos=0, god=2)], current="Q", properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 1000, "position": 1, "god_of_wealth_rounds": 1},
                 {"id": "A", "fund": 1000}]}))
NEW.append(C("TC-US10-011", "租金不足破产资金归0且游戏继续",
    PR([P("A"), P("Q", fund=50, pos=0), P("S", pos=30)],
       users=["A", "Q", "S"], current="Q",
       properties=[PROP(1, "A", 0), PROP(5, "Q", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "S", "game_status": "RUNNING",
     "players": [{"id": "Q", "fund": 0, "position": 1, "status": "BANKRUPT"},
                 {"id": "A", "fund": 1100}],
     "properties": [PROP(1, "A", 0)],
     "properties_absent": [5]}))
NEW.append(C("TC-US10-012", "经过他人地产不停留不收费",
    PR([P("A"), P("Q", pos=12)], current="Q", properties=[PROP(13, "A", 0)]),
    [A("STEP", steps=2)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 1000, "position": 14},
                 {"id": "A", "fund": 1000}]}))

# US11 升级
NEW.append(C("TC-US11-001", "空地升级为茅屋",
    PR([P("A", pos=0), P("Q", pos=20)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 800, "position": 1}],
     "properties": [PROP(1, "A", 1)]}))
NEW.append(C("TC-US11-002", "茅屋升级为洋房",
    PR([P("A", pos=0), P("Q", pos=20)], properties=[PROP(1, "A", 1)]),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 800, "position": 1}],
     "properties": [PROP(1, "A", 2)]}))
NEW.append(C("TC-US11-003", "洋房升级为摩天楼",
    PR([P("A", pos=0), P("Q", pos=20)], properties=[PROP(1, "A", 2)]),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 800, "position": 1}],
     "properties": [PROP(1, "A", 3)]}))
NEW.append(C("TC-US11-004", "摩天楼不再出现升级提示",
    PR([P("A", pos=0), P("Q", pos=20)], properties=[PROP(1, "A", 3)]),
    [A("STEP", steps=1)],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "fund": 1000, "position": 1}],
     "properties": [PROP(1, "A", 3)]}))
NEW.append(C("TC-US11-005", "升级资金不足仍弹提示且回答后不扣款",
    PR([P("A", fund=150, pos=0), P("Q", pos=20)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "fund": 150, "position": 1}],
     "properties": [PROP(1, "A", 0)]}))
NEW.append(C("TC-US11-006", "拒绝升级资金等级不变",
    PR([P("A", pos=0), P("Q", pos=20)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 1000, "position": 1}],
     "properties": [PROP(1, "A", 0)]}))
NEW.append(C("TC-US11-007", "升级资金恰好等于费用",
    PR([P("A", fund=200, pos=0), P("Q", pos=20)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 0, "position": 1, "status": "NORMAL"}],
     "properties": [PROP(1, "A", 1)]}))
NEW.append(C("TC-US11-008", "地段二升级费用500",
    PR([P("A", pos=28), P("Q", pos=20)], properties=[PROP(29, "A", 0)]),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 500, "position": 29}],
     "properties": [PROP(29, "A", 1)]}))
NEW.append(C("TC-US11-009", "地段三升级费用300",
    PR([P("A", pos=35), P("Q", pos=20)], properties=[PROP(36, "A", 0)]),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 700, "position": 36}],
     "properties": [PROP(36, "A", 1)]}))

# US14 破产与游戏结束
NEW.append(C("TC-US14-001", "资金为0不破产",
    PR([P("A", pos=20), P("Q", fund=0, pos=0)], current="Q"),
    [A("STEP", steps=1), ANS("N")],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 0, "position": 1, "status": "NORMAL"}],
     "properties_absent": [1]}))
NEW.append(C("TC-US14-002", "破产玩家在回合切换中被跳过",
    PR([P("A", pos=13), P("Q", status="BANKRUPT"), P("S", pos=30)],
       users=["A", "Q", "S"]),
    [A("STEP", steps=1)],
    {"current_user": "S",
     "players": [{"id": "A", "position": 14}]}))
NEW.append(C("TC-US14-003", "破产清除全部地产且点数保留",
    PR([P("A", pos=20), P("Q", fund=50, pos=0, credit=30), P("S", pos=30)],
       users=["A", "Q", "S"], current="Q",
       properties=[PROP(1, "A", 0), PROP(5, "Q", 0), PROP(10, "Q", 2)]),
    [A("STEP", steps=1)],
    {"current_user": "S", "game_status": "RUNNING",
     "players": [{"id": "Q", "fund": 0, "position": 1, "status": "BANKRUPT",
                  "credit": 30}],
     "properties": [PROP(1, "A", 0)],
     "properties_absent": [5, 10]}))
NEW.append(C("TC-US14-004", "破产释放的地产可被他人重新购买",
    PR([P("A", pos=4), P("Q", fund=50, pos=0), P("S", pos=13)],
       users=["A", "Q", "S"], current="Q",
       properties=[PROP(1, "A", 0), PROP(5, "Q", 0)]),
    [A("STEP", steps=1), A("STEP", steps=1), A("STEP", steps=1), ANS("Y")],
    {"current_user": "S",
     "players": [{"id": "Q", "fund": 0, "position": 1, "status": "BANKRUPT"},
                 {"id": "A", "fund": 900, "position": 5}],
     "properties": [PROP(1, "A", 0), PROP(5, "A", 0)]}))
NEW.append(C("TC-US14-005", "只剩一名玩家时游戏结束并产生胜者",
    PR([P("A", pos=4), P("Q", fund=50, pos=0)], current="Q",
       properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1)],
    {"game_status": "FINISHED", "phase": "ENDED", "winner": "A",
     "players": [{"id": "Q", "fund": 0, "position": 1, "status": "BANKRUPT"}]}))

# US17 路障
NEW.append(C("TC-US17-001", "前方1格放置路障",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=1)],
    {"current_user": "A",
     "players": [{"id": "A", "items": {"BLOCK": 0, "ROBOT": 0}}],
     "map_items": [ITEM(21)]}))
NEW.append(C("TC-US17-002", "前方10格边界放置路障",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=10)],
    {"map_items": [ITEM(30)]}))
NEW.append(C("TC-US17-003", "后方1格放置路障",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=-1)],
    {"map_items": [ITEM(19)]}))
NEW.append(C("TC-US17-004", "后方10格边界放置路障",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=-10)],
    {"map_items": [ITEM(10)]}))
NEW.append(C("TC-US17-005", "偏移0放置在当前位置",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=0)],
    {"map_items": [ITEM(20)]}))
NEW.append(C("TC-US17-006", "正向环绕放置路障",
    PR([P("A", pos=68, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=2)],
    {"map_items": [ITEM(0)]}))
NEW.append(C("TC-US17-007", "反向环绕放置路障",
    PR([P("A", pos=1, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=-2)],
    {"map_items": [ITEM(69)]}))
NEW.append(C("TC-US17-008", "offset超出上限报INVALID_PARAMS",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=11)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0,
        "path": "actions[0].params.offset"}))
NEW.append(C("TC-US17-009", "offset超出下限报INVALID_PARAMS",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=-11)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0}))
NEW.append(C("TC-US17-010", "没有路障时放置失败",
    PR([P("A", pos=20, items=(0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=2)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0}))
NEW.append(C("TC-US17-011", "目标位置已有路障放置失败",
    PR([P("A", pos=20, items=(1, 0)), P("Q", pos=40)],
       map_items=[ITEM(21)]),
    [A("BLOCK", offset=1)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0}))
NEW.append(C("TC-US17-012", "路障拦截在矿地获得点数",
    PR([P("A", pos=62), P("Q", pos=40)], map_items=[ITEM(64)]),
    [A("STEP", steps=5)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 64, "credit": 60}],
     "map_items_absent": [64]}))
NEW.append(C("TC-US17-013", "路障拦截在公园不触发任何事件",
    PR([P("A", pos=47), P("Q", pos=40)], map_items=[ITEM(49)]),
    [A("STEP", steps=6)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 49, "status": "NORMAL",
                  "god_of_wealth_rounds": 0}],
     "map_items_absent": [49]}))
NEW.append(C("TC-US17-014", "放置路障的玩家自己被拦截",
    PR([P("A", pos=0, items=(1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=3), A("STEP", steps=6), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 3,
                  "items": {"BLOCK": 0, "ROBOT": 0}}],
     "map_items_absent": [3]}))
NEW.append(C("TC-US17-015", "同一回合连续放置两个路障",
    PR([P("A", pos=0, items=(2, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=1), A("BLOCK", offset=2)],
    {"current_user": "A",
     "map_items": [ITEM(1), ITEM(2)]}))
NEW.append(C("TC-US17-017", "路障在起点上触发并处理起点",
    PR([P("A", pos=68), P("Q", pos=40)], map_items=[ITEM(0)]),
    [A("STEP", steps=3)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 0, "fund": 1000}],
     "map_items_absent": [0]}))
NEW.append(C("TC-US17-019", "放置路障后对方走到被拦截并处理落点",
    PR([P("A", pos=13, items=(1, 0)), P("Q", pos=20)]),
    [A("BLOCK", offset=8), A("STEP", steps=1), A("STEP", steps=1), ANS("N")],
    {"current_user": "A",
     "players": [{"id": "Q", "position": 21}, {"id": "A", "position": 14}],
     "map_items_absent": [21], "properties_absent": [21]}))
NEW.append(C("TC-US17-020", "路障拦在道具屋仍进入道具屋",
    PR([P("A", pos=27, credit=50), P("Q", pos=40)], map_items=[ITEM(28)]),
    [A("STEP", steps=1), ANS("F")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 28, "credit": 50}],
     "map_items_absent": [28]}))

# US19 机器娃娃
NEW.append(C("TC-US19-001", "清除前方10格内多个路障",
    PR([P("A", pos=60, items=(0, 1)), P("Q", pos=40)],
       map_items=[ITEM(61), ITEM(62), ITEM(63)]),
    [A("ROBOT")],
    {"current_user": "A",
     "map_items_absent": [61, 62, 63]}))
NEW.append(C("TC-US19-002", "第10格边界被清除",
    PR([P("A", pos=60, items=(0, 1)), P("Q", pos=40)],
       map_items=[ITEM(0)]),
    [A("ROBOT")],
    {"map_items_absent": [0]}))
NEW.append(C("TC-US19-003", "第11格道具不被清除",
    PR([P("A", pos=60, items=(0, 1)), P("Q", pos=40)],
       map_items=[ITEM(1)]),
    [A("ROBOT")],
    {"map_items": [ITEM(1)]}))
NEW.append(C("TC-US19-004", "不清除当前位置上的道具",
    PR([P("A", pos=60, items=(0, 1)), P("Q", pos=40)],
       map_items=[ITEM(60)]),
    [A("ROBOT")],
    {"map_items": [ITEM(60)]}))
NEW.append(C("TC-US19-005", "前方无道具也消耗机器娃娃",
    PR([P("A", pos=60, items=(0, 1)), P("Q", pos=40)]),
    [A("ROBOT")],
    {"current_user": "A",
     "players": [{"id": "A", "items": {"BLOCK": 0, "ROBOT": 0}}]}))
NEW.append(C("TC-US19-006", "没有机器娃娃时使用失败",
    PR([P("A", pos=60, items=(0, 0)), P("Q", pos=40)],
       map_items=[ITEM(61)]),
    [A("ROBOT")], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0}))
NEW.append(C("TC-US19-007", "环绕清除跨越地图边界",
    PR([P("A", pos=65, items=(0, 1)), P("Q", pos=40)],
       map_items=[ITEM(68), ITEM(0), ITEM(4)]),
    [A("ROBOT")],
    {"map_items_absent": [68, 0, 4]}))
NEW.append(C("TC-US19-008", "PROMPT阶段使用ROBOT报INVALID_PHASE",
    PR([P("A", pos=0, items=(0, 1)), P("Q", pos=40)]),
    [A("STEP", steps=1), A("ROBOT")], {},
    ee={"code": "INVALID_PHASE", "action_index": 1}))

# US21 礼品店
NEW.append(C("TC-US21-001", "礼品屋选择奖金获得2000元",
    PR([P("A", pos=34), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("1")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 3000, "position": 35}]}))
NEW.append(C("TC-US21-002", "礼品屋选择点数卡获得200点",
    PR([P("A", pos=34), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("2")],
    {"current_user": "Q",
     "players": [{"id": "A", "credit": 200, "position": 35}]}))
NEW.append(C("TC-US21-003", "礼品屋选择财神获得5回合且获得回合不扣减",
    PR([P("A", pos=34), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("3")],
    {"current_user": "Q",
     "players": [{"id": "A", "god_of_wealth_rounds": 5, "position": 35}]}))
NEW.append(C("TC-US21-004", "礼品屋非法选项0视为放弃直接退出",
    PR([P("A", pos=34), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("0")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 1000, "credit": 0,
                  "god_of_wealth_rounds": 0, "position": 35}]}))
NEW.append(C("TC-US21-005", "礼品屋非法字符a视为放弃直接退出",
    PR([P("A", pos=34), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("a")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 1000, "credit": 0,
                  "god_of_wealth_rounds": 0, "position": 35}]}))
NEW.append(C("TC-US21-006", "经过礼品屋不停留不触发奖励",
    PR([P("A", pos=34), P("Q", pos=40)]),
    [A("STEP", steps=2), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 1000, "credit": 0,
                  "god_of_wealth_rounds": 0, "position": 36}],
     "properties_absent": [36]}))

# US22 财神（礼品屋来源）
NEW.append(C("TC-US22-001", "财神免过路费且回合数递减",
    PR([P("A"), P("Q", pos=0, god=3)], current="Q", properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 1000, "position": 1, "god_of_wealth_rounds": 2},
                 {"id": "A", "fund": 1000}]}))
NEW.append(C("TC-US22-002", "财神不影响购买地产",
    PR([P("A", pos=20), P("Q", pos=0, god=3)], current="Q"),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 800, "position": 1, "god_of_wealth_rounds": 2}],
     "properties": [PROP(1, "Q", 0)]}))
NEW.append(C("TC-US22-003", "财神不影响升级地产",
    PR([P("A", pos=0, god=3), P("Q", pos=20)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 800, "position": 1, "god_of_wealth_rounds": 2}],
     "properties": [PROP(1, "A", 1)]}))
NEW.append(C("TC-US22-004", "财神不影响道具购买点数",
    PR([P("A", pos=27, credit=50, god=3), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("2")],
    {"current_user": "Q",
     "players": [{"id": "A", "credit": 20, "position": 28, "god_of_wealth_rounds": 2,
                  "items": {"BLOCK": 0, "ROBOT": 1}}]}))
NEW.append(C("TC-US22-006", "财神不影响矿地点数",
    PR([P("A", pos=63, god=3), P("Q", pos=40)]),
    [A("STEP", steps=1)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 64, "credit": 60, "god_of_wealth_rounds": 2}]}))
NEW.append(C("TC-US22-007", "财神最后一轮免租次轮恢复收费",
    PR([P("A", pos=13), P("Q", pos=0, god=1)], current="Q",
       properties=[PROP(1, "A", 0), PROP(2, "A", 0)]),
    [A("STEP", steps=1), A("STEP", steps=1), A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "fund": 900, "position": 2, "god_of_wealth_rounds": 0},
                 {"id": "A", "fund": 1100}]}))
NEW.append(C("TC-US22-009", "再次获得礼品屋财神重置为5回合",
    PR([P("A", pos=34, god=1), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("3")],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 35, "god_of_wealth_rounds": 5}]}))

# US23 出售
NEW.append(C("TC-US23-001", "出售地段一空地获得400",
    PR([P("A"), P("Q", pos=40)], properties=[PROP(1, "A", 0)]),
    [A("SELL", position=1)],
    {"current_user": "A",
     "players": [{"id": "A", "fund": 1400}],
     "properties_absent": [1]}))
NEW.append(C("TC-US23-002", "出售地段一摩天楼获得1600",
    PR([P("A"), P("Q", pos=40)], properties=[PROP(1, "A", 3)]),
    [A("SELL", position=1)],
    {"current_user": "A",
     "players": [{"id": "A", "fund": 2600}],
     "properties_absent": [1]}))
NEW.append(C("TC-US23-003", "出售地段二摩天楼获得4000",
    PR([P("A"), P("Q", pos=40)], properties=[PROP(29, "A", 3)]),
    [A("SELL", position=29)],
    {"current_user": "A",
     "players": [{"id": "A", "fund": 5000}],
     "properties_absent": [29]}))
NEW.append(C("TC-US23-004", "出售地段三空地获得600",
    PR([P("A"), P("Q", pos=40)], properties=[PROP(36, "A", 0)]),
    [A("SELL", position=36)],
    {"current_user": "A",
     "players": [{"id": "A", "fund": 1600}],
     "properties_absent": [36]}))
NEW.append(C("TC-US23-005", "出售他人地产失败",
    PR([P("A"), P("Q", pos=40)], properties=[PROP(5, "Q", 0)]),
    [A("SELL", position=5)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0}))
NEW.append(C("TC-US23-006", "出售无主地块失败",
    PR([P("A"), P("Q", pos=40)]),
    [A("SELL", position=10)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0}))
NEW.append(C("TC-US23-007", "出售位置越界报INVALID_PARAMS",
    PR([P("A"), P("Q", pos=40)]),
    [A("SELL", position=-1)], {},
    ee={"code": "INVALID_PARAMS", "action_index": 0}))
NEW.append(C("TC-US23-008", "出售后不结束回合可再行动",
    PR([P("A", pos=0), P("Q", pos=40)], properties=[PROP(1, "A", 0)]),
    [A("SELL", position=1), A("STEP", steps=1), ANS("Y")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 1200, "position": 1}],
     "properties": [PROP(1, "A", 0)]}))
NEW.append(C("TC-US23-009", "出售释放的地产可被他人购买",
    PR([P("A", pos=0), P("Q", pos=0)], properties=[PROP(1, "A", 0)]),
    [A("SELL", position=1), A("STEP", steps=2), ANS("N"), A("STEP", steps=1), ANS("Y")],
    {"current_user": "A",
     "players": [{"id": "A", "fund": 1400, "position": 2},
                 {"id": "Q", "fund": 800, "position": 1}],
     "properties": [PROP(1, "Q", 0)]}))

# US24 退出
NEW.append(C("TC-US24-006", "PROMPT阶段QUIT结束游戏且清空提示",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=1), A("QUIT")],
    {"game_status": "FINISHED", "phase": "ENDED", "winner": None,
     "pending_prompt": None}))
NEW.append(C("TC-US24-007", "COMMAND阶段QUIT后胜者为null",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("QUIT")],
    {"game_status": "FINISHED", "phase": "ENDED", "winner": None,
     "pending_prompt": None}))

# US25 自动化支撑
NEW.append(C("TC-US25-008", "PROMPT阶段ROLL报INVALID_PHASE",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=1), A("ROLL")], {},
    ee={"code": "INVALID_PHASE", "action_index": 1}))
NEW.append(C("TC-US25-009", "COMMAND阶段ANSWER报INVALID_PHASE",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [ANS("Y")], {},
    ee={"code": "INVALID_PHASE", "action_index": 0}))

# US26 STEP
NEW.append(C("TC-US26-008", "STEP70移动整圈回到原位",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=70)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 0}]}))
NEW.append(C("TC-US26-006", "STEP步数为0原地结束回合",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=0)],
    {"current_user": "Q", "turn_number": 2,
     "players": [{"id": "A", "position": 0}]}))
NEW.append(C("TC-US26-009", "STEP允许超过一圈的步数",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=100), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 30}],
     "properties_absent": [30]}))

# US07 位置显示
NEW.append(C("TC-US07-009", "三人重叠时显示当前玩家",
    PR([P("A", pos=5), P("Q", pos=5), P("S", pos=5)], users=["A", "Q", "S"], current="Q"),
    [],
    {"display_players": [{"position": 5, "visible_user": "Q"}]}))
NEW.append(C("TC-US07-010", "重叠且当前玩家不在其中时显示users最靠前者",
    PR([P("A", pos=5), P("Q", pos=5), P("S", pos=7)], users=["A", "Q", "S"], current="S"),
    [],
    {"display_players": [{"position": 5, "visible_user": "A"},
                         {"position": 7, "visible_user": "S"}]}))
NEW.append(C("TC-US07-011", "破产玩家不在地图显示",
    PR([P("A", pos=5), P("Q", pos=5, status="BANKRUPT")], users=["A", "Q"], current="A"),
    [],
    {"display_players": [{"position": 5, "visible_user": "A"}]}))
NEW.append(C("TC-US07-013", "四玩家同格显示当前玩家",
    PR([P("A", pos=5), P("Q", pos=5), P("S", pos=5), P("J", pos=5)],
       users=["A", "Q", "S", "J"], current="J"),
    [],
    {"display_players": [{"position": 5, "visible_user": "J"}]}))

# US16 道具屋
NEW.append(C("TC-US16-010", "道具屋购买路障成功",
    PR([P("A", pos=27, credit=50), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("1")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "credit": 0, "position": 28,
                  "items": {"BLOCK": 1, "ROBOT": 0}}]}))
NEW.append(C("TC-US16-011", "道具屋购买机器娃娃成功",
    PR([P("A", pos=27, credit=30), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("2")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "credit": 0, "position": 28,
                  "items": {"BLOCK": 0, "ROBOT": 1}}]}))
NEW.append(C("TC-US16-012", "点数充足购买后不自动离开可继续购买",
    PR([P("A", pos=27, credit=100), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("2")],
    {"current_user": "A", "phase": "PROMPT",
     "players": [{"id": "A", "credit": 70, "position": 28,
                  "items": {"BLOCK": 0, "ROBOT": 1}}]}))

# NOUS
NEW.append(C("TC-NOUS-001", "经过起点不发放工资",
    PR([P("A", pos=69), P("Q", pos=40)]),
    [A("STEP", steps=2), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 1000, "position": 1}],
     "properties_absent": [1]}))
NEW.append(C("TC-NOUS-003", "买地升级收租破产到结束的完整链路",
    PR([P("A", pos=0), P("Q", fund=100, pos=10)], current="A"),
    [A("STEP", steps=1), ANS("Y"),
     A("STEP", steps=1), ANS("N"),
     A("STEP", steps=70), ANS("Y"),
     A("STEP", steps=60)],
    {"game_status": "FINISHED", "phase": "ENDED", "winner": "A",
     "players": [{"id": "Q", "fund": 0, "status": "BANKRUPT"},
                 {"id": "A", "fund": 800, "position": 1}],
     "properties": [PROP(1, "A", 1)]}))

# pending_prompt 断言
NEW.append(C("TC-US04-006", "停在空地时待处理提示为BUY",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=1)],
    {"current_user": "A", "phase": "PROMPT", "pending_prompt": "BUY"}))
NEW.append(C("TC-US11-010", "停在自己地产时待处理提示为UPGRADE",
    PR([P("A", pos=0), P("Q", pos=40)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "A", "phase": "PROMPT", "pending_prompt": "UPGRADE"}))
NEW.append(C("TC-US16-013", "停在道具屋时待处理提示为TOOL_SHOP",
    PR([P("A", pos=27, credit=50), P("Q", pos=40)]),
    [A("STEP", steps=1)],
    {"current_user": "A", "phase": "PROMPT", "pending_prompt": "TOOL_SHOP"}))
NEW.append(C("TC-US21-007", "停在礼品屋时待处理提示为GIFT_SHOP",
    PR([P("A", pos=34), P("Q", pos=40)]),
    [A("STEP", steps=1)],
    {"current_user": "A", "phase": "PROMPT", "pending_prompt": "GIFT_SHOP"}))

# BUY/UPGRADE 非法回答
NEW.append(C("TC-US09-011", "购买提示非法回答应报参数错误",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("x")], {},
    ee={"code": "INVALID_PARAMS", "action_index": 1,
        "path": "actions[1].params.value"}))
NEW.append(C("TC-US11-011", "升级提示非法回答应报参数错误",
    PR([P("A", pos=0), P("Q", pos=40)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1), ANS("x")], {},
    ee={"code": "INVALID_PARAMS", "action_index": 1,
        "path": "actions[1].params.value"}))

# PRNG 财神位置确定性
_NEXT = XORSHIFT32(45678, 8)
_pos = [_n % 70 for _n in _NEXT]
_forbidden = {14, 49, 28, 35}
_pick = next(p for p in _pos if p not in _forbidden)
NEW.append(C("TC-V2-FORTUNE-031", "PRNG XORSHIFT32财神位置确定性",
    PR([P("A", pos=14), P("Q", pos=49)], tn=10,
       fortune=FORTUNE(next_spawn=10),
       rng={"mode": "PRNG", "algorithm": "XORSHIFT32",
            "stream_seeds": {"FORTUNE_POSITION": 45678}}),
    [A("ADVANCE_TURN")],
    {"turn_number": 11,
     "fortune": {"position": _pick, "remaining_map_turns": 5}}))

# ---- 组装套件 ----
suite = {
    "schema_version": "2.0",
    "suite": "Group3-v2",
    "map_file": "map.json",
    "tests": NEW,
}

ids = [c["case_id"] for c in NEW]
assert len(ids) == len(set(ids)), "duplicate case ids!"
for c in NEW:
    if "map_file" not in c:
        c["map_file"] = "map.json"
# 非法地图用例使用专用地图文件
for c in NEW:
    if c["case_id"] == "TC-V2-DEL-007":
        c["map_file"] = "map_invalid_magic.json"
    elif c["case_id"] == "TC-V2-DEL-008":
        c["map_file"] = "map_invalid_hospital.json"
    elif c["case_id"] == "TC-V2-DEL-009":
        c["map_file"] = "map_invalid_jail.json"
    elif c["case_id"] == "TC-V2-DEL-010":
        c["map_file"] = "map_invalid_park_pos.json"

with open(SRC, "w", encoding="utf-8") as fp:
    json.dump(suite, fp, ensure_ascii=False, indent=2)
    fp.write("\n")

print(f"OK: {len(NEW)} cases -> {SRC}")
