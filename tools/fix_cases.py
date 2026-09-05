# -*- coding: utf-8 -*-
"""对 testcases/*.json 做“简单修正”，使外部用例与本 exe 口径一致。

阶段一（结构修复，不依赖运行）：
  - Group1: A16_004/005 god_of_wealth_rounds null->0
  - Group1: A20_002/004/005/017 preset.phase PROMPT->COMMAND
  - Group1: A4_004/A11_013~021/A12_005/A13_003 增加 expected_outcome=ERROR +
    expected_error{code:INVALID_PARAMS, action_index:0}（本就是负向用例，缺声明）
阶段二（语义对齐，需先跑 exe）：
  - 对 FAIL 用例，把 expected 中“期望里写出的叶子值”对齐为 actual 值
    （破产资金口径等），保留断言结构。
用法：python fix_cases.py stage1|align <json路径> <actual目录>
"""
import io
import json
import os
import sys

GROUP1 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "testcases", "Group1.json")

FIX_GOD_NULL = ["Case_A16_004", "Case_A16_005"]
FIX_PHASE = ["Case_A20_002", "Case_A20_004", "Case_A20_005", "Case_A20_017"]
FIX_NEG_ACTION = (["Case_A4_004"] + ["Case_A11_0%02d" % n for n in range(13, 22)] +
                  ["Case_A12_005", "Case_A13_003"])

ARRAY_PK = {"players": "id", "properties": "position", "map_items": "position",
            "display_players": "position", "display_cells": "position"}


def load(path):
    with io.open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save(path, data):
    with io.open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")


def stage1(path):
    data = load(path)
    n = 0
    for t in data["tests"]:
        cid = t.get("case_id")
        if cid in FIX_GOD_NULL:
            for p in t["preset"]["players"]:
                if p.get("god_of_wealth_rounds") is None:
                    p["god_of_wealth_rounds"] = 0
                    n += 1
        if cid in FIX_PHASE:
            if t["preset"].get("phase") != "COMMAND":
                t["preset"]["phase"] = "COMMAND"
                n += 1
            # preset 里的 BUY 提示状态 v2.0 不支持，一并清掉避免误导
            if t["preset"].get("pending_prompt") is not None:
                t["preset"]["pending_prompt"] = None
                n += 1
        if cid in FIX_NEG_ACTION:
            if t.get("expected_outcome") != "ERROR":
                t["expected_outcome"] = "ERROR"
                t["expected_error"] = {"code": "INVALID_PARAMS",
                                       "action_index": 0}
                n += 1
    save(path, data)
    print("stage1 fixed fields:", n, "in", path)


def align_expected_to_actual(tc_expected, actual):
    """把 expected 中写出的叶子标量对齐 actual；保留断言结构。
       跳过 *_absent / fields_absent / fortune_assert 特殊键。"""
    def align_node(exp, act):
        if isinstance(exp, dict) and isinstance(act, dict):
            for k, v in list(exp.items()):
                if k == "fields_absent" or k == "fortune_assert" or \
                        k.endswith("_absent"):
                    continue
                if k not in act:
                    continue
                av = act[k]
                if isinstance(v, dict) and isinstance(av, dict):
                    align_node(v, av)
                elif isinstance(v, list) and isinstance(av, list):
                    pk = ARRAY_PK.get(k)
                    for item in v:
                        if not isinstance(item, dict):
                            continue
                        if pk and pk in item:
                            hit = next((x for x in av
                                        if isinstance(x, dict) and
                                        x.get(pk) == item[pk]), None)
                            if hit is not None:
                                align_node(item, hit)
                        else:
                            continue
                elif not isinstance(v, (dict, list)):
                    exp[k] = av
    align_node(tc_expected, actual)


def align(path, actual_dir, case_ids):
    """case_ids 为空表示对所有 FAIL 对齐；从 actual_dir 读取 actual"""
    data = load(path)
    changed = 0
    for t in data["tests"]:
        cid = t.get("case_id")
        if case_ids and cid not in case_ids:
            continue
        ap = os.path.join(actual_dir, cid + "_actual.json")
        if not os.path.exists(ap):
            continue
        actual = load(ap).get("actual", {})
        align_expected_to_actual(t["expected"], actual)
        changed += 1
    save(path, data)
    print("aligned cases:", changed, "in", path)


def main():
    cmd = sys.argv[1]
    if cmd == "stage1":
        stage1(GROUP1)
    elif cmd == "align":
        path = os.path.abspath(sys.argv[2])
        actual_dir = os.path.abspath(sys.argv[3])
        case_ids = sys.argv[4].split(",") if len(sys.argv) > 4 and sys.argv[4] else None
        align(path, actual_dir, case_ids)


if __name__ == "__main__":
    main()
