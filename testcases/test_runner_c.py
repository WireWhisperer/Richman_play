import json
import os
import subprocess

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "mini_rich.exe")
CASES_FILE = os.path.join(HERE, "test_cases.json")

with open(CASES_FILE, encoding="utf-8") as f:
    CASES = json.load(f)["tests"]


def run_case(case):
    preset = case.get("preset", {})
    cash = preset.get("cash", "10000")
    if isinstance(cash, list):
        cash_lines = [str(x) for x in cash]
    else:
        cash_lines = [str(cash)]
    lines = cash_lines + [str(preset.get("players", ""))]
    lines += case.get("actions", [])
    proc = subprocess.run(
        [EXE],
        input="\n".join(lines) + "\n",
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=30,
    )
    return proc


@pytest.mark.parametrize("case", CASES, ids=lambda c: c["case_id"])
def test_scenario(case):
    proc = run_case(case)
    exp = case.get("expected", {})
    if "exit_code" in exp:
        assert proc.returncode == exp["exit_code"], (
            f"退出码 {proc.returncode} != {exp['exit_code']}\n实际输出:\n{proc.stdout}"
        )
    for s in exp.get("contains", []):
        assert s in proc.stdout, f"缺少: {s}\n实际输出:\n{proc.stdout}"
    for s in exp.get("not_contains", []):
        assert s not in proc.stdout, f"不应出现: {s}\n实际输出:\n{proc.stdout}"
