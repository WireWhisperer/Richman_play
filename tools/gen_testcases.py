# -*- coding: utf-8 -*-
"""生成补充测试用例并合并进 Group3_Testcases.json（按规则文档/接口规范为准）。"""
import json
import os

def find_repo_root():
    """定位仓库根目录：脚本位于 tools/ 内时取上一级；否则取同级 Richman_play 目录。"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parent = os.path.dirname(script_dir)
    if os.path.isdir(os.path.join(parent, "testcases")) and \
       os.path.isdir(os.path.join(parent, "src")):
        return parent
    return os.path.join(script_dir, "Richman_play")

SRC = os.path.join(find_repo_root(), "testcases", "Group3_Testcases.json")

# ---------- helpers ----------

def P(id, fund=1000, credit=0, pos=0, status="NORMAL", rr=0, items=(0, 0, 0), god=0):
    return {"id": id, "fund": fund, "credit": credit, "position": pos,
            "status": status, "remaining_rounds": rr,
            "items": {"BLOCK": items[0], "BOMB": items[1], "ROBOT": items[2]},
            "god_of_wealth_rounds": god}

def PR(players, users=None, current="A", properties=None, map_items=None):
    if users is None:
        users = [p["id"] for p in players]
    return {"users": users, "current_user": current, "phase": "COMMAND",
            "game_status": "RUNNING", "players": players,
            "properties": properties or [], "map_items": map_items or [],
            "dice_sequence": []}

def PROP(pos, owner, level):
    return {"position": pos, "owner": owner, "level": level}

def ITEM(pos, kind):
    return {"position": pos, "type": kind}

def C(cid, name, preset, actions, expected, er=None, ec=None):
    c = {"case_id": cid, "case_name": name, "map_file": "map.json",
         "preset": preset, "actions": actions, "expected": expected}
    if er:
        c["expected_result"] = er
    if ec:
        c["expected_error_code"] = ec
    return c

def A(cmd, **params):
    if params:
        return {"command": cmd, "params": params}
    return {"command": cmd}

def ANS(v):
    return A("ANSWER", value=v)

NEW = []

# ================= US06 查询资产 =================
NEW.append(C("TC-US06-001", "QUERY不改变状态且不结束回合",
    PR([P("A", 1000, 50, 5, items=(1, 1, 1), god=3), P("Q", 1000, 0, 20)]),
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

# ================= US08 地块事件触发 =================
NEW.append(C("TC-US08-001", "正常到达医院不住院",
    PR([P("A", pos=13), P("Q", pos=20)]),
    [A("STEP", steps=1)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 14, "status": "NORMAL", "remaining_rounds": 0}]}))
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
NEW.append(C("TC-US08-004", "经过医院不停留无效果",
    PR([P("A", pos=12), P("Q", pos=20)]),
    [A("STEP", steps=4), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 16, "status": "NORMAL"}],
     "properties_absent": [16]}))

# ================= US10 租金 =================
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
NEW.append(C("TC-US10-008", "地主住院免收租金且轮空恢复",
    PR([P("A", status="HOSPITAL", rr=1), P("Q", pos=0)], current="Q",
       properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "Q",
     "players": [{"id": "Q", "fund": 1000, "position": 1},
                 {"id": "A", "fund": 1000, "status": "NORMAL", "remaining_rounds": 0}]}))
NEW.append(C("TC-US10-009", "地主在监狱免收租金",
    PR([P("A", status="JAIL", rr=2), P("Q", pos=0)], current="Q",
       properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1)],
    {"current_user": "Q",
     "players": [{"id": "Q", "fund": 1000, "position": 1},
                 {"id": "A", "fund": 1000, "status": "JAIL", "remaining_rounds": 1}]}))
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

# ================= US11 升级 =================
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
NEW.append(C("TC-US11-005", "升级资金不足不出现提示且不扣款",
    PR([P("A", fund=150, pos=0), P("Q", pos=20)], properties=[PROP(1, "A", 0)]),
    [A("STEP", steps=1)],
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

# ================= US14 破产与游戏结束 =================
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

# ================= US17 路障 =================
NEW.append(C("TC-US17-001", "前方1格放置路障",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=1)],
    {"current_user": "A",
     "players": [{"id": "A", "items": {"BLOCK": 0, "BOMB": 0, "ROBOT": 0}}],
     "map_items": [ITEM(21, "BLOCK")]}))
NEW.append(C("TC-US17-002", "前方10格边界放置路障",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=10)],
    {"map_items": [ITEM(30, "BLOCK")]}))
NEW.append(C("TC-US17-003", "后方1格放置路障",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=-1)],
    {"map_items": [ITEM(19, "BLOCK")]}))
NEW.append(C("TC-US17-004", "后方10格边界放置路障",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=-10)],
    {"map_items": [ITEM(10, "BLOCK")]}))
NEW.append(C("TC-US17-005", "偏移0放置在当前位置",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=0)],
    {"map_items": [ITEM(20, "BLOCK")]}))
NEW.append(C("TC-US17-006", "正向环绕放置路障",
    PR([P("A", pos=68, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=2)],
    {"map_items": [ITEM(0, "BLOCK")]}))
NEW.append(C("TC-US17-007", "反向环绕放置路障",
    PR([P("A", pos=1, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=-2)],
    {"map_items": [ITEM(69, "BLOCK")]}))
NEW.append(C("TC-US17-008", "offset超出上限报INVALID_PARAMS",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=11)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US17-009", "offset超出下限报INVALID_PARAMS",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=-11)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US17-010", "没有路障时放置失败",
    PR([P("A", pos=20, items=(0, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=2)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US17-011", "目标位置已有路障放置失败",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)],
       map_items=[ITEM(21, "BLOCK")]),
    [A("BLOCK", offset=1)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US17-012", "路障拦截在矿地获得点数",
    PR([P("A", pos=62), P("Q", pos=40)], map_items=[ITEM(64, "BLOCK")]),
    [A("STEP", steps=5)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 64, "credit": 60}],
     "map_items_absent": [64]}))
NEW.append(C("TC-US17-013", "路障拦截在监狱被关押",
    PR([P("A", pos=47), P("Q", pos=40)], map_items=[ITEM(49, "BLOCK")]),
    [A("STEP", steps=6)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 49, "status": "JAIL", "remaining_rounds": 2}],
     "map_items_absent": [49]}))
NEW.append(C("TC-US17-014", "放置路障的玩家自己被拦截",
    PR([P("A", pos=0, items=(1, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=3), A("STEP", steps=6), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 3,
                  "items": {"BLOCK": 0, "BOMB": 0, "ROBOT": 0}}],
     "map_items_absent": [3]}))
NEW.append(C("TC-US17-015", "同一回合连续放置两个路障",
    PR([P("A", pos=0, items=(2, 0, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=1), A("BLOCK", offset=2)],
    {"current_user": "A",
     "map_items": [ITEM(1, "BLOCK"), ITEM(2, "BLOCK")]}))
NEW.append(C("TC-US17-016", "同一回合连续放置路障和炸弹",
    PR([P("A", pos=0, items=(1, 1, 0)), P("Q", pos=40)]),
    [A("BLOCK", offset=2), A("BOMB", offset=3)],
    {"current_user": "A",
     "map_items": [ITEM(2, "BLOCK"), ITEM(3, "BOMB")]}))
NEW.append(C("TC-US17-017", "路障在起点上触发并处理起点",
    PR([P("A", pos=68), P("Q", pos=40)], map_items=[ITEM(0, "BLOCK")]),
    [A("STEP", steps=3)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 0, "fund": 1000}],
     "map_items_absent": [0]}))

# ================= US18 炸弹 =================
NEW.append(C("TC-US18-001", "前方1格放置炸弹",
    PR([P("A", pos=20, items=(0, 1, 0)), P("Q", pos=40)]),
    [A("BOMB", offset=1)],
    {"map_items": [ITEM(21, "BOMB")]}))
NEW.append(C("TC-US18-002", "偏移0在当前位放置炸弹",
    PR([P("A", pos=20, items=(0, 1, 0)), P("Q", pos=40)]),
    [A("BOMB", offset=0)],
    {"map_items": [ITEM(20, "BOMB")]}))
NEW.append(C("TC-US18-003", "正向环绕放置炸弹",
    PR([P("A", pos=68, items=(0, 1, 0)), P("Q", pos=40)]),
    [A("BOMB", offset=2)],
    {"map_items": [ITEM(0, "BOMB")]}))
NEW.append(C("TC-US18-004", "炸弹offset超出上限报INVALID_PARAMS",
    PR([P("A", pos=20, items=(0, 1, 0)), P("Q", pos=40)]),
    [A("BOMB", offset=11)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US18-005", "没有炸弹时放置失败",
    PR([P("A", pos=20, items=(0, 0, 0)), P("Q", pos=40)]),
    [A("BOMB", offset=2)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US18-006", "目标位置已有路障放置炸弹失败",
    PR([P("A", pos=20, items=(0, 1, 0)), P("Q", pos=40)],
       map_items=[ITEM(21, "BLOCK")]),
    [A("BOMB", offset=1)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US18-007", "踩炸弹送医院轮空三轮且不获得矿地点数",
    PR([P("A", pos=62), P("Q", pos=40)], map_items=[ITEM(64, "BOMB")]),
    [A("STEP", steps=5)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 14, "status": "HOSPITAL",
                  "remaining_rounds": 3, "credit": 0}],
     "map_items_absent": [64]}))
NEW.append(C("TC-US18-008", "炸弹在道具屋上不进入道具屋",
    PR([P("A", pos=27), P("Q", pos=40)], map_items=[ITEM(28, "BOMB")]),
    [A("STEP", steps=1)],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 14, "status": "HOSPITAL"}],
     "map_items_absent": [28]}))
NEW.append(C("TC-US18-009", "炸弹在礼品屋上不进入礼品屋",
    PR([P("A", pos=34), P("Q", pos=40)], map_items=[ITEM(35, "BOMB")]),
    [A("STEP", steps=1)],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 14, "status": "HOSPITAL"}],
     "map_items_absent": [35]}))
NEW.append(C("TC-US18-010", "移动起点上的炸弹不触发",
    PR([P("A", pos=20, items=(0, 1, 0)), P("Q", pos=40)]),
    [A("BOMB", offset=0), A("STEP", steps=1), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 21, "status": "NORMAL"}],
     "map_items": [ITEM(20, "BOMB")]}))
NEW.append(C("TC-US18-011", "住院轮空三轮后恢复行动",
    PR([P("A", pos=62), P("Q", pos=0)], map_items=[ITEM(64, "BOMB")]),
    [A("STEP", steps=5), A("STEP", steps=70), A("STEP", steps=70),
     A("STEP", steps=70), A("STEP", steps=70)],
    {"current_user": "A",
     "players": [{"id": "A", "position": 14, "status": "NORMAL", "remaining_rounds": 0},
                 {"id": "Q", "position": 0}]}))

# ================= US19 机器娃娃 =================
NEW.append(C("TC-US19-001", "清除前方10格内多个路障和炸弹",
    PR([P("A", pos=60, items=(0, 0, 1)), P("Q", pos=40)],
       map_items=[ITEM(61, "BLOCK"), ITEM(62, "BOMB"), ITEM(63, "BLOCK")]),
    [A("ROBOT")],
    {"current_user": "A",
     "map_items_absent": [61, 62, 63]}))
NEW.append(C("TC-US19-002", "第10格边界被清除",
    PR([P("A", pos=60, items=(0, 0, 1)), P("Q", pos=40)],
       map_items=[ITEM(0, "BLOCK")]),
    [A("ROBOT")],
    {"map_items_absent": [0]}))
NEW.append(C("TC-US19-003", "第11格道具不被清除",
    PR([P("A", pos=60, items=(0, 0, 1)), P("Q", pos=40)],
       map_items=[ITEM(1, "BLOCK")]),
    [A("ROBOT")],
    {"map_items": [ITEM(1, "BLOCK")]}))
NEW.append(C("TC-US19-004", "不清除当前位置上的道具",
    PR([P("A", pos=60, items=(0, 0, 1)), P("Q", pos=40)],
       map_items=[ITEM(60, "BOMB")]),
    [A("ROBOT")],
    {"map_items": [ITEM(60, "BOMB")]}))
NEW.append(C("TC-US19-005", "前方无道具也消耗机器娃娃",
    PR([P("A", pos=60, items=(0, 0, 1)), P("Q", pos=40)]),
    [A("ROBOT")],
    {"current_user": "A",
     "players": [{"id": "A", "items": {"BLOCK": 0, "BOMB": 0, "ROBOT": 0}}]}))
NEW.append(C("TC-US19-006", "没有机器娃娃时使用失败",
    PR([P("A", pos=60, items=(0, 0, 0)), P("Q", pos=40)],
       map_items=[ITEM(61, "BLOCK")]),
    [A("ROBOT")], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US19-007", "环绕清除跨越地图边界",
    PR([P("A", pos=65, items=(0, 0, 1)), P("Q", pos=40)],
       map_items=[ITEM(68, "BLOCK"), ITEM(0, "BOMB"), ITEM(4, "BLOCK")]),
    [A("ROBOT")],
    {"map_items_absent": [68, 0, 4]}))
NEW.append(C("TC-US19-008", "PROMPT阶段使用ROBOT报INVALID_PHASE",
    PR([P("A", pos=0, items=(0, 0, 1)), P("Q", pos=40)]),
    [A("STEP", steps=1), A("ROBOT")], {}, er="ERROR", ec="INVALID_PHASE"))

# ================= US20 魔法屋 =================
NEW.append(C("TC-US20-001", "到达魔法屋无任何效果",
    PR([P("A", pos=62), P("Q", pos=40)]),
    [A("STEP", steps=1)],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "position": 63, "fund": 1000, "credit": 0}]}))

# ================= US21 礼品店 =================
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

# ================= US22 财神 =================
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
    [A("STEP", steps=1), ANS("3")],
    {"current_user": "Q",
     "players": [{"id": "A", "credit": 0, "position": 28, "god_of_wealth_rounds": 2,
                  "items": {"BLOCK": 0, "BOMB": 1, "ROBOT": 0}}]}))
NEW.append(C("TC-US22-005", "财神不影响监狱关押",
    PR([P("A", pos=48, god=3), P("Q", pos=40)]),
    [A("STEP", steps=1)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 49, "status": "JAIL",
                  "remaining_rounds": 2, "god_of_wealth_rounds": 2}]}))
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
NEW.append(C("TC-US22-008", "监狱轮空消耗财神回合",
    PR([P("A", pos=20), P("Q", pos=49, status="JAIL", rr=1, god=2)], current="Q"),
    [A("STEP", steps=1)],
    {"current_user": "A",
     "players": [{"id": "Q", "position": 49, "status": "NORMAL",
                  "remaining_rounds": 0, "god_of_wealth_rounds": 1}]}))

# ================= US23 出售房产 =================
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
    [A("SELL", position=5)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US23-006", "出售无主地块失败",
    PR([P("A"), P("Q", pos=40)]),
    [A("SELL", position=10)], {}, er="ERROR", ec="INVALID_PARAMS"))
NEW.append(C("TC-US23-007", "出售位置越界报INVALID_PARAMS",
    PR([P("A"), P("Q", pos=40)]),
    [A("SELL", position=-1)], {}, er="ERROR", ec="INVALID_PARAMS"))
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

# ================= US24 退出（补充） =================
NEW.append(C("TC-US24-006", "PROMPT阶段QUIT结束游戏且清空提示",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=1), A("QUIT")],
    {"game_status": "FINISHED", "phase": "ENDED", "winner": None,
     "pending_prompt": None}))

# ================= US25 自动化支撑（补充） =================
NEW.append(C("TC-US25-008", "PROMPT阶段ROLL报INVALID_PHASE",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=1), A("ROLL")], {}, er="ERROR", ec="INVALID_PHASE"))
NEW.append(C("TC-US25-009", "COMMAND阶段ANSWER报INVALID_PHASE",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [ANS("Y")], {}, er="ERROR", ec="INVALID_PHASE"))

# ================= US26 STEP（补充） =================
NEW.append(C("TC-US26-008", "STEP70移动整圈回到原位",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=70)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 0}]}))
NEW.append(C("TC-US26-009", "STEP超过70报INVALID_PARAMS",
    PR([P("A", pos=0), P("Q", pos=40)]),
    [A("STEP", steps=71)], {}, er="ERROR", ec="INVALID_PARAMS"))

# ================= US07 位置显示（补充） =================
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

# ================= US16 道具屋（补充购买路障/机器娃娃） =================
NEW.append(C("TC-US16-010", "道具屋购买路障成功",
    PR([P("A", pos=27, credit=50), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("1")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "credit": 0, "position": 28,
                  "items": {"BLOCK": 1, "BOMB": 0, "ROBOT": 0}}]}))
NEW.append(C("TC-US16-011", "道具屋购买机器娃娃成功",
    PR([P("A", pos=27, credit=30), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("2")],
    {"current_user": "Q", "phase": "COMMAND",
     "players": [{"id": "A", "credit": 0, "position": 28,
                  "items": {"BLOCK": 0, "BOMB": 0, "ROBOT": 1}}]}))
NEW.append(C("TC-US16-012", "点数充足购买后不自动离开可继续购买",
    PR([P("A", pos=27, credit=100), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("2")],
    {"current_user": "A", "phase": "PROMPT",
     "players": [{"id": "A", "credit": 70, "position": 28,
                  "items": {"BLOCK": 0, "BOMB": 0, "ROBOT": 1}}]}))

# ================= US17 补充：炸弹上放路障 =================
NEW.append(C("TC-US17-018", "目标位置已有炸弹时放置路障失败",
    PR([P("A", pos=20, items=(1, 0, 0)), P("Q", pos=40)],
       map_items=[ITEM(21, "BOMB")]),
    [A("BLOCK", offset=1)], {}, er="ERROR", ec="INVALID_PARAMS"))

# ================= US18 补充：建筑上放炸弹 =================
NEW.append(C("TC-US18-012", "有建筑的位置放置炸弹成功",
    PR([P("A", pos=20, items=(0, 1, 0)), P("Q", pos=40)],
       properties=[PROP(21, "A", 0)]),
    [A("BOMB", offset=1)],
    {"current_user": "A",
     "map_items": [ITEM(21, "BOMB")],
     "properties": [PROP(21, "A", 0)]}))

# ================= US22 补充：财神重置 =================
NEW.append(C("TC-US22-009", "再次获得财神重置为5回合",
    PR([P("A", pos=34, god=1), P("Q", pos=40)]),
    [A("STEP", steps=1), ANS("3")],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 35, "god_of_wealth_rounds": 5}]}))
NEW.append(C("TC-US22-010", "回合切换轮空也消耗财神",
    PR([P("A", pos=48, god=2), P("Q", pos=0)]),
    [A("STEP", steps=1), A("STEP", steps=70)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 49, "status": "JAIL",
                  "remaining_rounds": 1, "god_of_wealth_rounds": 0}]}))

# ================= NOUS 无对应US =================
NEW.append(C("TC-NOUS-001", "经过起点不发放工资",
    PR([P("A", pos=69), P("Q", pos=40)]),
    [A("STEP", steps=2), ANS("N")],
    {"current_user": "Q",
     "players": [{"id": "A", "fund": 1000, "position": 1}],
     "properties_absent": [1]}))
NEW.append(C("TC-NOUS-002", "起点上的炸弹按途中规则触发",
    PR([P("A", pos=68), P("Q", pos=40)], map_items=[ITEM(0, "BOMB")]),
    [A("STEP", steps=3)],
    {"current_user": "Q",
     "players": [{"id": "A", "position": 14, "status": "HOSPITAL", "remaining_rounds": 3}],
     "map_items_absent": [0]}))

# ---------- merge ----------
with open(SRC, encoding="utf-8") as fp:
    data = json.load(fp)

existing_ids = {t["case_id"] for t in data["tests"]}
new_ids = [c["case_id"] for c in NEW]
if len(set(new_ids)) != len(new_ids):
    raise SystemExit("duplicate ids within NEW")

# 幂等合并：先移除同 id 的旧用例，再追加
data["tests"] = [t for t in data["tests"] if t["case_id"] not in set(new_ids)]
data["tests"].extend(NEW)
with open(SRC, "w", encoding="utf-8") as fp:
    json.dump(data, fp, ensure_ascii=False, indent=2)
    fp.write("\n")

print(f"OK: existing {len(existing_ids)} + new {len(NEW)} = total {len(data['tests'])}")
