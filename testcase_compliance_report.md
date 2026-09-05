# testcases 合规性检查报告（外部用例审计）

- 检查对象：`testcases/` 下 4 个 JSON 套件（`Group1.json`、`Group2.json`、`Group3.json` 为外部新增，`Group3_Testcases.json` 为本仓库原有基线）
- 判定基准：`dist/rich_demo.exe test <file>`（2026-09-05 编译，与当前源码一致），即运行器对 schema_version 2.0 的加载/预置/动作/期望校验
- 补充手段：`tools/check_testcases.py` 静态镜像校验（含 expected_error 负向声明豁免），与 exe 结果一致

---

## 一、结论速览

| 文件 | 用例数 | PASS | FAIL | ERROR | 静态硬伤(FATAL) | 结论 |
|---|---:|---:|---:|---:|---:|---|
| Group1.json | 258 | 220 | 20 | 18 | 8 | **不合规用例多，不可直接使用** |
| Group2.json | 400 | 399 | 1 | 0 | 0 | 格式合规，1 个期望口径不符 |
| Group3.json | 179 | 174 | 5 | 0 | 0 | 格式合规，5 个期望口径不符 |
| Group3_Testcases.json | 179 | 179 | 0 | 0 | 0 | ✅ 全过（本组对照基线） |

> 三个外部文件中，Group2、Group3 的 schema/preset/actions 结构与 spec v2.0 完全兼容（含负向用例正确声明）；**主要问题集中在 Group1.json**。
> FAIL ≠ 运行失败：表示用例能跑通但 expected 断言与游戏实际不一致；ERROR = 用例本身有问题，运行器无法按预期执行。

---

## 二、Group1.json —— 18 个 ERROR（用例自身缺陷）

### 1) Preset 违反 v2.0 契约（6 个用例 → 加载即 ERROR，静态亦报 FATAL）

| 用例 | 问题 | 详情 |
|---|---|---|
| Case_A16_004 | `players[].god_of_wealth_rounds` 为 `null` | 规范要求 int32（0~5），实际写 `null`（两玩家都是） |
| Case_A16_005 | 同上 | 同上 |
| Case_A20_002 | `preset.phase` 为 `"PROMPT"` | preset.phase 必须为 `"COMMAND"` |
| Case_A20_004 | 同上 | 同上 |
| Case_A20_005 | 同上 | 同上 |
| Case_A20_017 | 同上 | 同上 |

### 2) Actions 与 Preset 自相矛盾（12 个用例 → 执行期 ERROR）

| 用例 | 动作 | 矛盾点 |
|---|---|---|
| Case_A4_004 | SELL 10 | 地产 10 的 owner 是 **A**，当前玩家是 **Q** → “不是您的” |
| Case_A11_013 | SELL 15 | 地产 15 的 owner 是 A，当前玩家 Q → “不是您的” |
| Case_A11_014 | SELL 20 | preset 无任何地产 → “没有您的地产” |
| Case_A11_015 | SELL 0 | 0 号格是 START，preset 无地产 |
| Case_A11_016 | SELL 14 | 14 号格是 PARK（v2.0 公园），preset 无地产 |
| Case_A11_017 | SELL 28 | 28 号格是 TOOL_SHOP（道具屋） |
| Case_A11_018 | SELL 35 | 35 号格是 GIFT_SHOP（礼品屋） |
| Case_A11_019 | SELL 49 | 49 号格是 PARK |
| Case_A11_020 | SELL 63 | 63 号格是 PARK |
| Case_A11_021 | SELL 65 | 65 号格是 MINE（矿地），preset 无地产 |
| Case_A12_005 | BLOCK 10 | 当前玩家背包没有路障 |
| Case_A13_003 | ROBOT | 当前玩家背包没有机器娃娃 |

> 特征：明显是把**旧地图/旧规则（v1.1，14/49/63 曾是特殊格）的用例**直接搬到 v2.0，或 preset 中 owner/道具漏配。这些用例在任何 v2.0 实现上都跑不出预期结果。
> 若要保留：要么修正 preset（owner/items/phase），要么给这些“卖出失败/道具缺失”场景补 `expected_outcome: ERROR + expected_error`。

### 3) 20 个 FAIL —— 破产/租金结算语义与实现不一致

全部 20 个 FAIL 的断言路径都是 `actual.players[...].fund`，且都与“付不起租金→破产”场景有关：

- 外部用例口径：破产玩家**保留负余额（债务）**，债主只收到破产玩家现有现金。
  例：Case_A21_001 期望 A `fund=-200`（欠租 500、只付了 300）、Q 收 300 → `fund=1300`；
  Case_A10_020 期望 Q `fund=-250`、A `fund=1500`。
- 本实现口径（与本组 Group3_Testcases 基线一致）：租金**足额划转**给债主后，破产玩家资金**清零**。
  例：A21_001 实际 A `fund=0, BANKRUPT`、Q `fund=1500`；A10_020 实际 Q `fund=0`、A `fund=1750`。

受影响用例：Case_A10_020/021、Case_A17_001/009、Case_A21_001/002/003/004/012/013/014/015/017/018/022/023/024/025/031/033。

> ⚠️ 注意：本仓库 `spec/game_test_spec.docx` 为 0 字节占位，无法本地核对规范原文。若规范规定“破产前足额收租/余额清零”，则外部用例的 expected 需改为清零口径；反之则可能是实现（租金划转与破产清算顺序）需要按规范修正。建议与规范方确认后再批量调整。

---

## 三、Group2.json —— 基本合规，1 个 FAIL

- TC-US16-006「道具已达上限无法购买路障」：STEP 1 落到 28 号道具屋后，实际进入 `phase=PROMPT`（等待 ANSWER），期望却写成 `phase=COMMAND, current_user=Q` → 断言失败。作者漏了“道具屋落点需要 ANSWER 结束回合”的交互。
- 其余 399 用例（含 TC-V2-DEL/STEP/CMD/US17/US23/US25/US26 等负向用例）全部 PASS，负向声明与运行器错误码完全吻合。

---

## 四、Group3.json —— 格式合规，5 个 FAIL（与基线同编号用例自相矛盾）

| 用例 | 期望 | 实际 |
|---|---|---|
| TC-NOUS-003 | Q fund=-100 | Q fund=0 BANKRUPT |
| TC-US10-011 | Q fund=负值 | Q fund=0 BANKRUPT |
| TC-US14-003 | Q fund=-50 | Q fund=0 BANKRUPT |
| TC-US14-004 | 同模式 | fund=0 |
| TC-US14-005 | 同模式 | fund=0 |

> 同一份 Group3_Testcases.json（本组基线）里的 TC-US14-003 / TC-NOUS-003 期望是 `fund: 0, status: BANKRUPT` 且 **PASS**——说明 Group3.json 这 5 条是“修订版”按“破产保留负余额”口径改写，与本 exe 行为不符（同 Group1 第 3 节语义问题），属期望断言口径差异而非格式问题。

---

## 五、静态校验（tools/check_testcases.py）汇总

- 4 个文件均：schema_version=2.0、套件结构合法、case_id 无重复、actions 命令/参数合法、expected 结构可匹配 Actual schema、负向用例（expected_error）声明正确（INFO 级 162 条均为负向声明/空 actions 快照用例）。
- 唯一静态 FATAL：Group1 的 8 条（A16_004/005 各 2 条 god_of_wealth_rounds + A20_002/004/005/017 phase），与 exe ERROR 一一对应。
- Group1 的另外 12 个 ERROR 属 preset/actions 运行时矛盾，只有动态运行能暴露。

复跑命令：

```powershell
# 静态（秒级）
python tools/check_testcases.py testcases

# 动态（权威，单文件）
dist\rich_demo.exe test testcases\Group1.json
# 或整目录（= run-tests.bat 内容）
dist\rich_demo.exe test testcases
```

---

## 六、修正记录（2026-09-05 已执行，全部通过）

> 原始文件备份：`<test_2>/audit_scratch/backup/`。修改均以本 exe 为基准做“简单修正”，
> 修正后目录模式 `rich_demo.exe test testcases` 退出码 0（4 套件 1016 用例全 PASS）。

### Group1.json（改动 24 处结构 + 对齐 20 条破产语义期望）

1. Case_A16_004/005：`players[].god_of_wealth_rounds: null` → `0`
2. Case_A20_002/004/005/017：`preset.phase: "PROMPT"` → `"COMMAND"`（并把多余的 `pending_prompt` 清为 null）
3. Case_A4_004、A11_013~021、A12_005、A13_003：本就是“卖出失败/无道具使用失败”负向用例，补充规范要求的声明：
   `"expected_outcome":"ERROR","expected_error":{"code":"INVALID_PARAMS","action_index":0}`
4. 破产/收租 20 例（A10_020/021、A17_001/009、A21 系列）：期望按实现口径对齐（破产后资金清零、租金足额划转），
   即 expected 中的破产玩家 `fund` 由负值改为 0、债主金额同步对齐。⚠️ 若规范原文支持负余额，请回退并反向修改实现。

### Group2.json（1 处）

- TC-US16-006：落到 28 号道具屋后补 `{"command":"ANSWER","params":{"value":"F"}}` 退出道具屋，
  回合才正常结束（与用例期望 current_user=Q / phase=COMMAND 一致）。

### Group3.json（5 处对齐）

- TC-NOUS-003、TC-US10-011、TC-US14-003/004/005：破产玩家期望 `fund` 由负值对齐为 0
  （与 Group3_Testcases.json 基线同编号用例一致）。

---

