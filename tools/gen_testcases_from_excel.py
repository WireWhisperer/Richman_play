#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate automated testcase JSON suites from Excel stories + JSON spec."""
from __future__ import annotations

import json
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "testcases"


def player(pid, fund=1000, credit=0, position=0, status="NORMAL",
           remaining_rounds=0, block=0, bomb=0, robot=0, gow=0):
    return {
        "id": pid,
        "fund": fund,
        "credit": credit,
        "position": position,
        "status": status,
        "remaining_rounds": remaining_rounds,
        "items": {"BLOCK": block, "BOMB": bomb, "ROBOT": robot},
        "god_of_wealth_rounds": gow,
    }


def preset(users, current, players, properties=None, map_items=None, dice=None):
    return {
        "users": users,
        "current_user": current,
        "phase": "COMMAND",
        "game_status": "RUNNING",
        "players": players,
        "properties": properties or [],
        "map_items": map_items or [],
        "dice_sequence": dice or [],
    }


def case(cid, name, preset_obj, actions, expected, **extra):
    obj = {
        "case_id": cid,
        "case_name": name,
        "map_file": "map.json",
        "preset": preset_obj,
        "actions": actions,
        "expected": expected,
    }
    obj.update(extra)
    return obj


def aq(a_kwargs=None, q_kwargs=None, current="A"):
    a = player("A", **(a_kwargs or {}))
    q = player("Q", **(q_kwargs or {"position": 20}))
    return preset(["A", "Q"], current, [a, q])


def write_suite(us, tests):
    path = OUT / f"TC-{us}.json"
    data = {"schema_version": "1.0", "user_story": us, "tests": tests}
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {path.name} ({len(tests)} cases)")


def gen_us08():
    p5 = aq({"position": 0}, {"position": 20, "fund": 800})
    p5["properties"] = [{"position": 1, "owner": "Q", "level": 0}]
    p6 = aq({"position": 0, "fund": 500})
    p6["properties"] = [{"position": 1, "owner": "A", "level": 0}]
    tests = [
        case("TC-US08-001", "到达普通空地进入购买提示",
             aq({"position": 0}), [{"command": "STEP", "params": {"steps": 1}}],
             {"phase": "PROMPT", "pending_prompt": "BUY", "players": [{"id": "A", "position": 1}]}),
        case("TC-US08-002", "到达医院无额外事件",
             aq({"position": 13}), [{"command": "STEP", "params": {"steps": 1}}],
             {"phase": "COMMAND", "current_user": "Q",
              "players": [{"id": "A", "position": 14, "status": "NORMAL"}]}),
        case("TC-US08-003", "到达监狱进入JAIL轮空2",
             aq({"position": 48}), [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "position": 49, "status": "JAIL", "remaining_rounds": 2}],
              "current_user": "Q"}),
        case("TC-US08-004", "到达起点无额外扣费",
             aq({"position": 69}), [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "position": 0, "fund": 1000}], "current_user": "Q"}),
        case("TC-US08-005", "到达他人地产结算租金",
             p5, [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 900, "position": 1}, {"id": "Q", "fund": 900}],
              "properties": [{"position": 1, "owner": "Q", "level": 0}]}),
        case("TC-US08-006", "到达自己已购地产可升级提示",
             p6, [{"command": "STEP", "params": {"steps": 1}}],
             {"phase": "PROMPT", "pending_prompt": "UPGRADE", "players": [{"id": "A", "position": 1}]}),
    ]
    write_suite("US08", tests)


def gen_us10():
    tests = [
        case("TC-US10-001", "RENT-1 地段一空地租金100",
             {**aq({"position": 0, "fund": 1000}, {"position": 20, "fund": 800}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 900}, {"id": "Q", "fund": 900}]}),
        case("TC-US10-002", "RENT-2 地段一租金为价值一半",
             {**aq({"position": 0, "fund": 1000}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 900}, {"id": "Q", "fund": 900}]}),
        case("TC-US10-003", "RENT-3 地段二租金250",
             {**aq({"position": 28, "fund": 1000}, {"fund": 800, "position": 5}),
              "properties": [{"position": 29, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 750, "position": 29}, {"id": "Q", "fund": 1050}]}),
        case("TC-US10-004", "RENT-4 地段三租金150",
             {**aq({"position": 35, "fund": 1000}, {"fund": 800, "position": 5}),
              "properties": [{"position": 36, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 850, "position": 36}, {"id": "Q", "fund": 950}]}),
        case("TC-US10-005", "RENT-6 地主在医院免租",
             {**aq({"position": 0, "fund": 1000},
                   {"fund": 800, "position": 14, "status": "HOSPITAL", "remaining_rounds": 2}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 1000}, {"id": "Q", "fund": 800}]}),
        case("TC-US10-006", "RENT-7 地主在监狱免租",
             {**aq({"position": 0, "fund": 1000},
                   {"fund": 800, "position": 49, "status": "JAIL", "remaining_rounds": 1}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 1000}, {"id": "Q", "fund": 800}]}),
        case("TC-US10-007", "RENT-8 财神免租",
             {**aq({"position": 0, "fund": 1000, "gow": 3}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 1000}, {"id": "Q", "fund": 800}]}),
        case("TC-US10-008", "RENT-9 正常缴纳租金金额正确",
             {**aq({"position": 0, "fund": 1000}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 900}, {"id": "Q", "fund": 900}]}),
        case("TC-US10-009", "RENT-10 资金不足支付租金后破产",
             {**aq({"position": 0, "fund": 80}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 0, "status": "BANKRUPT"}, {"id": "Q", "fund": 900}],
              "game_status": "FINISHED", "winner": "Q"}),
        case("TC-US10-010", "RENT-11 资金恰好等于租金不破产",
             {**aq({"position": 0, "fund": 100}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 0, "status": "NORMAL"}, {"id": "Q", "fund": 900}],
              "game_status": "RUNNING"}),
    ]
    write_suite("US10", tests)


def gen_us11():
    tests = []
    # fund不足：不进入升级提示，资金与等级不变
    tests.append(case(
        "TC-US11-001", "地段一资金不足199无法升级",
        {**aq({"position": 0, "fund": 199}),
         "properties": [{"position": 1, "owner": "A", "level": 0}]},
        [{"command": "STEP", "params": {"steps": 1}}],
        {"players": [{"id": "A", "fund": 199, "position": 1}],
         "properties": [{"position": 1, "owner": "A", "level": 0}],
         "current_user": "Q", "phase": "COMMAND"}))
    for i, fund in enumerate([200, 201], 2):
        tests.append(case(
            f"TC-US11-{i:03d}", f"地段一空地升级成功 fund={fund}",
            {**aq({"position": 0, "fund": fund}),
             "properties": [{"position": 1, "owner": "A", "level": 0}]},
            [{"command": "STEP", "params": {"steps": 1}},
             {"command": "ANSWER", "params": {"value": "Y"}}],
            {"players": [{"id": "A", "fund": fund - 200, "position": 1}],
             "properties": [{"position": 1, "owner": "A", "level": 1}],
             "current_user": "Q"}))
    for i, fund in enumerate([200, 201], 4):
        tests.append(case(
            f"TC-US11-{i:03d}", f"地段一茅屋升级洋房 fund={fund}",
            {**aq({"position": 0, "fund": fund}),
             "properties": [{"position": 1, "owner": "A", "level": 1}]},
            [{"command": "STEP", "params": {"steps": 1}},
             {"command": "ANSWER", "params": {"value": "Y"}}],
            {"players": [{"id": "A", "fund": fund - 200}],
             "properties": [{"position": 1, "owner": "A", "level": 2}]}))
    for i, fund in enumerate([200, 201], 6):
        tests.append(case(
            f"TC-US11-{i:03d}", f"地段一洋房升级摩天楼 fund={fund}",
            {**aq({"position": 0, "fund": fund}),
             "properties": [{"position": 1, "owner": "A", "level": 2}]},
            [{"command": "STEP", "params": {"steps": 1}},
             {"command": "ANSWER", "params": {"value": "Y"}}],
            {"players": [{"id": "A", "fund": fund - 200}],
             "properties": [{"position": 1, "owner": "A", "level": 3}]}))
    for i, fund in enumerate([500, 501], 8):
        tests.append(case(
            f"TC-US11-{i:03d}", f"地段二空地升级 fund={fund}",
            {**aq({"position": 28, "fund": fund}, {"position": 5}),
             "properties": [{"position": 29, "owner": "A", "level": 0}]},
            [{"command": "STEP", "params": {"steps": 1}},
             {"command": "ANSWER", "params": {"value": "Y"}}],
            {"players": [{"id": "A", "fund": fund - 500, "position": 29}],
             "properties": [{"position": 29, "owner": "A", "level": 1}]}))
    for i, fund in enumerate([300, 301], 10):
        tests.append(case(
            f"TC-US11-{i:03d}", f"地段三空地升级 fund={fund}",
            {**aq({"position": 35, "fund": fund}, {"position": 5}),
             "properties": [{"position": 36, "owner": "A", "level": 0}]},
            [{"command": "STEP", "params": {"steps": 1}},
             {"command": "ANSWER", "params": {"value": "Y"}}],
            {"players": [{"id": "A", "fund": fund - 300, "position": 36}],
             "properties": [{"position": 36, "owner": "A", "level": 1}]}))
    tests.append(case(
        "TC-US11-012", "取消升级不扣款",
        {**aq({"position": 0, "fund": 501}),
         "properties": [{"position": 1, "owner": "A", "level": 0}]},
        [{"command": "STEP", "params": {"steps": 1}},
         {"command": "ANSWER", "params": {"value": "N"}}],
        {"players": [{"id": "A", "fund": 501}],
         "properties": [{"position": 1, "owner": "A", "level": 0}]}))
    tests.append(case(
        "TC-US11-013", "摩天楼不再升级",
        {**aq({"position": 0, "fund": 501}),
         "properties": [{"position": 1, "owner": "A", "level": 3}]},
        [{"command": "STEP", "params": {"steps": 1}}],
        {"phase": "COMMAND", "current_user": "Q",
         "players": [{"id": "A", "fund": 501, "position": 1}],
         "properties": [{"position": 1, "owner": "A", "level": 3}]}))
    tests.append(case(
        "TC-US11-014", "他人地产不出现升级",
        {**aq({"position": 0, "fund": 501}, {"fund": 800, "position": 20}),
         "properties": [{"position": 1, "owner": "Q", "level": 1}]},
        [{"command": "STEP", "params": {"steps": 1}}],
        {"phase": "COMMAND",
         "players": [{"id": "A", "fund": 301}, {"id": "Q", "fund": 1000}],
         "properties": [{"position": 1, "owner": "Q", "level": 1}]}))
    write_suite("US11", tests)


def gen_us14():
    tests = [
        case("TC-US14-001", "扣款后资金>0不破产",
             {**aq({"position": 0, "fund": 200}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             # rent 100 -> fund 100
             {"players": [{"id": "A", "fund": 100, "status": "NORMAL"}]}),
        case("TC-US14-002", "扣款后资金=0不破产",
             {**aq({"position": 0, "fund": 100}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 0, "status": "NORMAL"}]}),
        case("TC-US14-003", "扣款后资金<0破产",
             {**aq({"position": 0, "fund": 99}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 0, "status": "BANKRUPT"}],
              "game_status": "FINISHED", "winner": "Q"}),
        case("TC-US14-004", "破产后地产恢复空地",
             {**aq({"position": 0, "fund": 50, "block": 1, "credit": 100},
                   {"fund": 800, "position": 20}),
              "properties": [
                  {"position": 1, "owner": "Q", "level": 0},
                  {"position": 2, "owner": "A", "level": 1}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "status": "BANKRUPT", "fund": 0, "credit": 0,
                           "items": {"BLOCK": 0, "BOMB": 0, "ROBOT": 0}}],
              "properties_absent": [2],
              "properties": [{"position": 1, "owner": "Q", "level": 0}]}),
        case("TC-US14-005", "破产后只剩一人游戏结束",
             {**aq({"position": 0, "fund": 50}, {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"game_status": "FINISHED", "winner": "Q", "phase": "ENDED"}),
    ]
    write_suite("US14", tests)


def gen_us17():
    tests = [
        case("TC-US17-001", "BL-01 50点购路障成功",
             aq({"position": 27, "credit": 50, "block": 0, "bomb": 4, "robot": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "1"}}],
             {"players": [{"id": "A", "credit": 0, "items": {"BLOCK": 1}}]}),
        case("TC-US17-002", "BL-02 49点购路障失败",
             aq({"position": 27, "credit": 49}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "1"}}],
             {"players": [{"id": "A", "credit": 49, "items": {"BLOCK": 0}}]}),
        case("TC-US17-003", "BL-03 道具已满购路障失败",
             aq({"position": 27, "credit": 50, "block": 10}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "1"}}],
             {"players": [{"id": "A", "credit": 50, "items": {"BLOCK": 10}}]}),
        case("TC-US17-004", "BL-04 前方1格放置路障",
             aq({"position": 20, "block": 1}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": 1}}],
             {"players": [{"id": "A", "items": {"BLOCK": 0}}],
              "map_items": [{"position": 21, "type": "BLOCK"}]}),
        case("TC-US17-005", "BL-05 前方10格放置路障",
             aq({"position": 20, "block": 1}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": 10}}],
             {"map_items": [{"position": 30, "type": "BLOCK"}],
              "players": [{"id": "A", "items": {"BLOCK": 0}}]}),
        case("TC-US17-006", "BL-06 后方1格放置路障",
             aq({"position": 20, "block": 1}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": -1}}],
             {"map_items": [{"position": 19, "type": "BLOCK"}]}),
        case("TC-US17-007", "BL-07 后方10格放置路障",
             aq({"position": 20, "block": 1}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": -10}}],
             {"map_items": [{"position": 10, "type": "BLOCK"}]}),
        case("TC-US17-008", "BL-08 前方超过10格INVALID_PARAMS",
             aq({"position": 20, "block": 1}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": 11}}],
             {}, expected_result="ERROR", expected_error_code="INVALID_PARAMS"),
        case("TC-US17-009", "BL-09 后方超过10格INVALID_PARAMS",
             aq({"position": 20, "block": 1}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": -11}}],
             {}, expected_result="ERROR", expected_error_code="INVALID_PARAMS"),
        case("TC-US17-010", "BL-10 偏移0放置在当前位置",
             aq({"position": 20, "block": 1}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": 0}}],
             {"map_items": [{"position": 20, "type": "BLOCK"}],
              "players": [{"id": "A", "items": {"BLOCK": 0}}]}),
        case("TC-US17-011", "BL-11 无路障放置失败",
             aq({"position": 20, "block": 0}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": 3}}],
             {"map_items_absent": [23], "players": [{"id": "A", "items": {"BLOCK": 0}}],
              "current_user": "A", "phase": "COMMAND"}),
        case("TC-US17-012", "BL-12 已有路障禁止再放",
             {**aq({"position": 20, "block": 1}, {"position": 5}),
              "map_items": [{"position": 23, "type": "BLOCK"}]},
             [{"command": "BLOCK", "params": {"offset": 3}}],
             {"map_items": [{"position": 23, "type": "BLOCK"}],
              "players": [{"id": "A", "items": {"BLOCK": 1}}]}),
        case("TC-US17-013", "BL-13 已有炸弹禁止放路障",
             {**aq({"position": 20, "block": 1}, {"position": 5}),
              "map_items": [{"position": 23, "type": "BOMB"}]},
             [{"command": "BLOCK", "params": {"offset": 3}}],
             {"map_items": [{"position": 23, "type": "BOMB"}],
              "players": [{"id": "A", "items": {"BLOCK": 1}}]}),
        case("TC-US17-014", "BL-17 路障拦截其他玩家",
             {**aq({"position": 5}, {"position": 28}),
              "map_items": [{"position": 30, "type": "BLOCK"}]},
             # current A first? need B to move - set current Q
             [{"command": "STEP", "params": {"steps": 5}}],
             {"players": [{"id": "Q", "position": 30}], "map_items_absent": [30]}),
        case("TC-US17-015", "BL-18 路障拦截放置者自己",
             {**aq({"position": 28, "block": 0}, {"position": 5}),
              "map_items": [{"position": 30, "type": "BLOCK"}]},
             [{"command": "STEP", "params": {"steps": 5}}],
             {"players": [{"id": "A", "position": 30}], "map_items_absent": [30]}),
        case("TC-US17-016", "BL-19 机器娃娃10格内清除路障",
             {**aq({"position": 20, "robot": 1}, {"position": 5}),
              "map_items": [{"position": 30, "type": "BLOCK"}]},
             [{"command": "ROBOT"}],
             {"map_items_absent": [30], "players": [{"id": "A", "items": {"ROBOT": 0}}]}),
        case("TC-US17-017", "BL-20 超过10格不清除",
             {**aq({"position": 19, "robot": 1}, {"position": 5}),
              "map_items": [{"position": 30, "type": "BLOCK"}]},
             [{"command": "ROBOT"}],
             {"map_items": [{"position": 30, "type": "BLOCK"}],
              "players": [{"id": "A", "items": {"ROBOT": 0}}]}),
    ]
    # Fix BL-17: current_user Q
    tests[13]["preset"]["current_user"] = "Q"
    tests[13]["preset"]["players"] = [
        player("A", position=5),
        player("Q", position=28),
    ]
    write_suite("US17", tests)


def gen_us18():
    tests = [
        case("TC-US18-001", "踩到炸弹送医院轮空3",
             {**aq({"position": 20}, {"position": 5}),
              "map_items": [{"position": 22, "type": "BOMB"}]},
             [{"command": "STEP", "params": {"steps": 3}}],
             {"players": [{"id": "A", "position": 14, "status": "HOSPITAL",
                           "remaining_rounds": 3}],
              "map_items_absent": [22], "current_user": "Q"}),
        case("TC-US18-002", "路径无道具正常前进",
             aq({"position": 20}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 3}}],
             {"players": [{"id": "A", "position": 23}]}),
        case("TC-US18-003", "机器娃娃前10格清炸弹",
             {**aq({"position": 20, "robot": 1}, {"position": 5}),
              "map_items": [{"position": 30, "type": "BOMB"}]},
             [{"command": "ROBOT"}],
             {"map_items_absent": [30]}),
        case("TC-US18-004", "机器娃娃前11格不清炸弹",
             {**aq({"position": 19, "robot": 1}, {"position": 5}),
              "map_items": [{"position": 30, "type": "BOMB"}]},
             [{"command": "ROBOT"}],
             {"map_items": [{"position": 30, "type": "BOMB"}]}),
        case("TC-US18-005", "机器娃娃前9格清炸弹",
             {**aq({"position": 21, "robot": 1}, {"position": 5}),
              "map_items": [{"position": 30, "type": "BOMB"}]},
             [{"command": "ROBOT"}],
             {"map_items_absent": [30]}),
        case("TC-US18-006", "bomb 10 前方放置",
             aq({"position": 20, "bomb": 1}, {"position": 5}),
             [{"command": "BOMB", "params": {"offset": 10}}],
             {"map_items": [{"position": 30, "type": "BOMB"}],
              "players": [{"id": "A", "items": {"BOMB": 0}}]}),
        case("TC-US18-007", "BOMB -10 后方放置",
             aq({"position": 20, "bomb": 1}, {"position": 5}),
             [{"command": "BOMB", "params": {"offset": -10}}],
             {"map_items": [{"position": 10, "type": "BOMB"}],
              "players": [{"id": "A", "items": {"BOMB": 0}}]}),
        case("TC-US18-008", "bomb 11 距离过远",
             aq({"position": 20, "bomb": 1}, {"position": 5}),
             [{"command": "BOMB", "params": {"offset": 11}}],
             {}, expected_result="ERROR", expected_error_code="INVALID_PARAMS"),
        case("TC-US18-009", "bomb -11 距离过远",
             aq({"position": 20, "bomb": 1}, {"position": 5}),
             [{"command": "BOMB", "params": {"offset": -11}}],
             {}, expected_result="ERROR", expected_error_code="INVALID_PARAMS"),
        case("TC-US18-010", "bomb 0 当前位置放置",
             aq({"position": 20, "bomb": 1}, {"position": 5}),
             [{"command": "BOMB", "params": {"offset": 0}}],
             {"map_items": [{"position": 20, "type": "BOMB"}],
              "players": [{"id": "A", "items": {"BOMB": 0}}]}),
        case("TC-US18-011", "目标有路障放置炸弹失败",
             {**aq({"position": 20, "bomb": 1}, {"position": 5}),
              "map_items": [{"position": 24, "type": "BLOCK"}]},
             [{"command": "BOMB", "params": {"offset": 4}}],
             {"map_items": [{"position": 24, "type": "BLOCK"}],
              "players": [{"id": "A", "items": {"BOMB": 1}}]}),
        case("TC-US18-012", "有建筑位置可放炸弹",
             {**aq({"position": 20, "bomb": 1}, {"position": 5}),
              "properties": [{"position": 25, "owner": "Q", "level": 1}]},
             [{"command": "BOMB", "params": {"offset": 5}}],
             {"map_items": [{"position": 25, "type": "BOMB"}],
              "players": [{"id": "A", "items": {"BOMB": 0}}]}),
        case("TC-US18-013", "炸弹在道具屋格仍送医院不进店",
             {**aq({"position": 26}, {"position": 5}),
              "map_items": [{"position": 28, "type": "BOMB"}]},
             [{"command": "STEP", "params": {"steps": 2}}],
             {"players": [{"id": "A", "position": 14, "status": "HOSPITAL",
                           "remaining_rounds": 3}],
              "phase": "COMMAND", "current_user": "Q"}),
        case("TC-US18-014", "炸弹在礼品屋格仍送医院不进店",
             {**aq({"position": 33}, {"position": 5}),
              "map_items": [{"position": 35, "type": "BOMB"}]},
             [{"command": "STEP", "params": {"steps": 2}}],
             {"players": [{"id": "A", "position": 14, "status": "HOSPITAL",
                           "remaining_rounds": 3}],
              "phase": "COMMAND", "current_user": "Q"}),
    ]
    write_suite("US18", tests)


def gen_us19():
    """Robot use before roll: clear / consume / no-item soft fail."""
    tests = []
    # success clear both
    tests.append(case(
        "TC-US19-001", "robot清除前方路障和炸弹",
        {**aq({"position": 20, "robot": 1}, {"position": 5}),
         "map_items": [{"position": 22, "type": "BLOCK"}, {"position": 25, "type": "BOMB"}]},
        [{"command": "ROBOT"}],
        {"map_items_absent": [22, 25],
         "players": [{"id": "A", "items": {"ROBOT": 0}}],
         "current_user": "A", "phase": "COMMAND"}))
    tests.append(case(
        "TC-US19-002", "robot仅清除路障",
        {**aq({"position": 20, "robot": 1}, {"position": 5}),
         "map_items": [{"position": 22, "type": "BLOCK"}]},
        [{"command": "ROBOT"}],
        {"map_items_absent": [22], "players": [{"id": "A", "items": {"ROBOT": 0}}]}))
    tests.append(case(
        "TC-US19-003", "robot仅清除炸弹",
        {**aq({"position": 20, "robot": 1}, {"position": 5}),
         "map_items": [{"position": 25, "type": "BOMB"}]},
        [{"command": "ROBOT"}],
        {"map_items_absent": [25], "players": [{"id": "A", "items": {"ROBOT": 0}}]}))
    tests.append(case(
        "TC-US19-004", "前方无障碍仍消耗机器娃娃",
        aq({"position": 20, "robot": 1}, {"position": 5}),
        [{"command": "ROBOT"}],
        {"players": [{"id": "A", "items": {"ROBOT": 0}}], "map_items": []}))
    tests.append(case(
        "TC-US19-005", "无机器娃娃无法使用",
        {**aq({"position": 20, "robot": 0}, {"position": 5}),
         "map_items": [{"position": 22, "type": "BLOCK"}]},
        [{"command": "ROBOT"}],
        {"map_items": [{"position": 22, "type": "BLOCK"}],
         "players": [{"id": "A", "items": {"ROBOT": 0}}]}))
    tests.append(case(
        "TC-US19-006", "大写ROBOT同样有效",
        {**aq({"position": 20, "robot": 1}, {"position": 5}),
         "map_items": [{"position": 22, "type": "BLOCK"}]},
        [{"command": "ROBOT"}],
        {"map_items_absent": [22], "players": [{"id": "A", "items": {"ROBOT": 0}}]}))
    tests.append(case(
        "TC-US19-007", "ROBOT不结束回合可继续STEP",
        aq({"position": 20, "robot": 1}, {"position": 5}),
        [{"command": "ROBOT"},
         {"command": "STEP", "params": {"steps": 2}},
         {"command": "ANSWER", "params": {"value": "N"}}],
        {"players": [{"id": "A", "position": 22, "items": {"ROBOT": 0}}],
         "current_user": "Q", "phase": "COMMAND"}))
    write_suite("US19", tests)


def gen_us21():
    """Gift shop + god of wealth + sell from US21~24 sheet."""
    tests = [
        case("TC-US21-001", "GS-01 到达礼品屋触发提示",
             aq({"position": 34}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}}],
             {"phase": "PROMPT", "pending_prompt": "GIFT_SHOP",
              "players": [{"id": "A", "position": 35}]}),
        case("TC-US21-002", "GS-02 选1得2000金钱",
             aq({"position": 34, "fund": 1000}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "1"}}],
             {"players": [{"id": "A", "fund": 3000}], "phase": "COMMAND",
              "current_user": "Q"}),
        case("TC-US21-003", "GS-03 选2得200点数",
             aq({"position": 34, "credit": 10}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "2"}}],
             {"players": [{"id": "A", "credit": 210}], "current_user": "Q"}),
        case("TC-US21-004", "GS-04 选3得财神Buff",
             aq({"position": 34}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "3"}}],
             # 回合结束会扣减1回合财神计数：5 -> 4
             {"players": [{"id": "A", "god_of_wealth_rounds": 4}],
              "current_user": "Q"}),
        case("TC-US21-005", "GS-05 输入0非法不发奖",
             aq({"position": 34, "fund": 1000, "credit": 10}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "0"}}],
             {"players": [{"id": "A", "fund": 1000, "credit": 10,
                           "god_of_wealth_rounds": 0}], "current_user": "Q"}),
        case("TC-US21-006", "GS-06 输入4非法不发奖",
             aq({"position": 34, "fund": 1000}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "4"}}],
             {"players": [{"id": "A", "fund": 1000, "god_of_wealth_rounds": 0}],
              "current_user": "Q"}),
        case("TC-US21-007", "GS-07 输入非数字非法不发奖",
             aq({"position": 34, "fund": 1000}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "a"}}],
             {"players": [{"id": "A", "fund": 1000}], "current_user": "Q"}),
        case("TC-US21-008", "CB-01 财神免过路费",
             {**aq({"position": 0, "fund": 1000, "gow": 5},
                   {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 1000}, {"id": "Q", "fund": 800}]}),
        case("TC-US21-009", "CB-06 无财神正常收费",
             {**aq({"position": 0, "fund": 1000, "gow": 0},
                   {"fund": 800, "position": 20}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "fund": 900}, {"id": "Q", "fund": 900}]}),
        case("TC-US21-010", "SP-01 合法出售自有房产",
             {**aq({"position": 5, "fund": 1000}),
              "properties": [{"position": 1, "owner": "A", "level": 0}]},
             [{"command": "SELL", "params": {"position": 1}}],
             # sell price = 200*2 = 400
             {"players": [{"id": "A", "fund": 1400}],
              "properties_absent": [1], "current_user": "A", "phase": "COMMAND"}),
        case("TC-US21-011", "SP-02 出售茅屋价格=2*(200+200)",
             {**aq({"position": 5, "fund": 1000}),
              "properties": [{"position": 1, "owner": "A", "level": 1}]},
             [{"command": "SELL", "params": {"position": 1}}],
             {"players": [{"id": "A", "fund": 1800}], "properties_absent": [1]}),
        case("TC-US21-012", "SP-05 出售他人房产失败",
             {**aq({"position": 5, "fund": 1000}),
              "properties": [{"position": 1, "owner": "Q", "level": 0}]},
             [{"command": "SELL", "params": {"position": 1}}],
             {"players": [{"id": "A", "fund": 1000}],
              "properties": [{"position": 1, "owner": "Q", "level": 0}]}),
        case("TC-US21-013", "SP-06 出售空地失败",
             aq({"position": 5, "fund": 1000}),
             [{"command": "SELL", "params": {"position": 1}}],
             {"players": [{"id": "A", "fund": 1000}], "properties_absent": [1]}),
        case("TC-US21-014", "SP-07 非法位置出售",
             aq({"position": 5}),
             [{"command": "SELL", "params": {"position": 999}}],
             {}, expected_result="ERROR", expected_error_code="INVALID_PARAMS"),
        case("TC-US21-015", "SP-09 大写SELL可用",
             {**aq({"position": 5, "fund": 1000}),
              "properties": [{"position": 5, "owner": "A", "level": 0}]},
             [{"command": "SELL", "params": {"position": 5}}],
             {"players": [{"id": "A", "fund": 1400}], "properties_absent": [5]}),
    ]
    write_suite("US21", tests)


def gen_us07_extra_overlap():
    """Append MOV / display cases into existing TC-US07 if missing MOV ids."""
    path = OUT / "TC-US07.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    existing = {t["case_id"] for t in data["tests"]}
    extras = [
        case("TC-US07-009", "MOV-1 位置3掷2到5",
             {**aq({"position": 3}), "dice_sequence": [2]},
             [{"command": "ROLL"}],
             {"players": [{"id": "A", "position": 5}]}),
        case("TC-US07-010", "MOV-4 位置69掷1回起点",
             {**aq({"position": 69}), "dice_sequence": [1]},
             [{"command": "ROLL"}],
             {"players": [{"id": "A", "position": 0}]}),
        case("TC-US07-011", "MOV-5 位置69掷3到2",
             {**aq({"position": 69}), "dice_sequence": [3]},
             [{"command": "ROLL"}],
             {"players": [{"id": "A", "position": 2}]}),
        case("TC-US07-012", "POS-3 重叠显示当前玩家",
             preset(["A", "Q"], "Q",
                    [player("A", position=5), player("Q", position=5)]),
             [],
             {"players": [{"id": "A", "position": 5}, {"id": "Q", "position": 5}],
              "display_players": [{"position": 5, "visible_user": "Q"}]}),
        case("TC-US07-013", "规范示例同回合连续放置路障炸弹",
             aq({"position": 0, "block": 1, "bomb": 1}, {"position": 20}),
             [{"command": "BLOCK", "params": {"offset": 2}},
              {"command": "BOMB", "params": {"offset": 3}}],
             {"current_user": "A", "phase": "COMMAND",
              "players": [{"id": "A", "items": {"BLOCK": 0, "BOMB": 0, "ROBOT": 0}}],
              "map_items": [{"position": 2, "type": "BLOCK"},
                            {"position": 3, "type": "BOMB"}]}),
        case("TC-US07-014", "规范示例同位置只能放一个道具",
             aq({"position": 0, "block": 1, "bomb": 1}, {"position": 20}),
             [{"command": "BLOCK", "params": {"offset": 2}},
              {"command": "BOMB", "params": {"offset": 2}}],
             {"map_items": [{"position": 2, "type": "BLOCK"}],
              "players": [{"id": "A", "items": {"BLOCK": 0, "BOMB": 1}}]}),
    ]
    added = 0
    for t in extras:
        if t["case_id"] not in existing:
            data["tests"].append(t)
            added += 1
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"updated TC-US07.json (+{added})")


def gen_us04_map_data():
    """State-checkable map data cases (not terminal rendering)."""
    tests = [
        case("TC-US04-006", "医院位置14可到达且不住院",
             aq({"position": 13}),
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "position": 14, "status": "NORMAL"}]}),
        case("TC-US04-007", "道具屋位置28触发道具屋",
             aq({"position": 27, "credit": 50}),
             [{"command": "STEP", "params": {"steps": 1}}],
             {"phase": "PROMPT", "pending_prompt": "TOOL_SHOP",
              "players": [{"id": "A", "position": 28}]}),
        case("TC-US04-008", "礼品屋位置35触发礼品屋",
             aq({"position": 34}),
             [{"command": "STEP", "params": {"steps": 1}}],
             {"phase": "PROMPT", "pending_prompt": "GIFT_SHOP",
              "players": [{"id": "A", "position": 35}]}),
        case("TC-US04-009", "监狱位置49触发入狱",
             aq({"position": 48}),
             [{"command": "STEP", "params": {"steps": 1}}],
             {"players": [{"id": "A", "position": 49, "status": "JAIL",
                           "remaining_rounds": 2}]}),
        case("TC-US04-010", "地图环绕68走3步到1",
             aq({"position": 68}),
             [{"command": "STEP", "params": {"steps": 3}}],
             {"players": [{"id": "A", "position": 1}]}),
        case("TC-US04-011", "重叠位置display_players当前优先",
             preset(["A", "Q"], "Q",
                    [player("A", position=5), player("Q", position=5)]),
             [],
             {"display_players": [{"position": 5, "visible_user": "Q"}]}),
        case("TC-US04-012", "放置路障后map_items可见",
             aq({"position": 20, "block": 1}, {"position": 5}),
             [{"command": "BLOCK", "params": {"offset": 2}}],
             {"map_items": [{"position": 22, "type": "BLOCK"}]}),
        case("TC-US04-013", "放置炸弹后map_items可见",
             aq({"position": 20, "bomb": 1}, {"position": 5}),
             [{"command": "BOMB", "params": {"offset": 2}}],
             {"map_items": [{"position": 22, "type": "BOMB"}]}),
        case("TC-US04-014", "地段一价格通过购买验证200",
             aq({"position": 0, "fund": 200}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "Y"}}],
             {"players": [{"id": "A", "fund": 0}],
              "properties": [{"position": 1, "owner": "A", "level": 0}]}),
        case("TC-US04-015", "地段二价格通过购买验证500",
             aq({"position": 28, "fund": 500}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "Y"}}],
             {"players": [{"id": "A", "fund": 0}],
              "properties": [{"position": 29, "owner": "A", "level": 0}]}),
        case("TC-US04-016", "地段三价格通过购买验证300",
             aq({"position": 35, "fund": 300}, {"position": 5}),
             [{"command": "STEP", "params": {"steps": 1}},
              {"command": "ANSWER", "params": {"value": "Y"}}],
             {"players": [{"id": "A", "fund": 0}],
              "properties": [{"position": 36, "owner": "A", "level": 0}]}),
    ]
    path = OUT / "TC-US04.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    existing = {t["case_id"] for t in data["tests"]}
    added = 0
    for t in tests:
        if t["case_id"] not in existing:
            data["tests"].append(t)
            added += 1
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"updated TC-US04.json (+{added})")


def main():
    OUT.mkdir(exist_ok=True)
    gen_us08()
    gen_us10()
    gen_us11()
    gen_us14()
    gen_us17()
    gen_us18()
    gen_us19()
    gen_us21()
    gen_us07_extra_overlap()
    gen_us04_map_data()
    print("done")


if __name__ == "__main__":
    main()
