# -*- coding: utf-8 -*-
"""生成 v2.0 非法地图测试资产（置于 spec/，供 INVALID_MAP 用例使用）。"""
import json
import os

SPEC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Richman_play", "spec")

with open(os.path.join(SPEC, "map.json"), encoding="utf-8") as fp:
    base = json.load(fp)

def make(name, pos, type_):
    cells = [dict(c) for c in base["cells"]]
    for c in cells:
        if c["position"] == pos:
            c["type"] = type_
    with open(os.path.join(SPEC, name), "w", encoding="utf-8") as fp:
        json.dump({"size": 70, "cells": cells}, fp, ensure_ascii=False, indent=2)
        fp.write("\n")
    print("wrote", name, pos, type_)

make("map_invalid_magic.json", 63, "MAGIC_HOUSE")
make("map_invalid_hospital.json", 14, "HOSPITAL")
make("map_invalid_jail.json", 49, "JAIL")
make("map_invalid_park_pos.json", 4, "PARK")
