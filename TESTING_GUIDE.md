# Windows / Linux(WSL) 自动化测试教程

> 被测对象：`dist/rich_demo.exe`（Windows 版）或 `dist/rich_demo`（Linux 版）
> 测试用例：`testcases/*.json`（schema_version 2.0 套件）
> 退出码约定：**0 = 全部 PASS**；1 = 有 FAIL/ERROR；2 = 运行器自身失败

## 0. 结果怎么看

运行后每个用例打印一行：

```
[PASS] TC-V2-PARK-001        # 通过
[FAIL] TC-xxx                # 能跑但期望与实际不符（下面有 mismatch 路径）
[ERROR] TC-xxx               # 用例本身有问题（preset 违规/动作失败等）
```

报告文件与 Actual 快照自动写到 `results/`：

| 文件 | 内容 |
|---|---|
| `results/<case_id>_report.json` | 结论 + 错误码 + expected/actual 差异 |
| `results/<case_id>_actual.json` | 该用例执行后的完整实际状态 |

---

## 1. Windows（cmd）

```bat
:: 进入项目目录（按实际路径改）
cd /d E:\桌面\Wayne File\课程\暑期课程\软件工程\test_2\Richman_play

:: ① 编译（有 dist\rich_demo.exe 后可跳过）
build.bat

:: ② 跑全部 testcases（4 个套件）
dist\rich_demo.exe test testcases

:: ③ 只跑某一个套件/单文件
dist\rich_demo.exe test testcases\Group2.json

:: ④ 查看退出码（0=全过）
echo %ERRORLEVEL%
```

## 2. Windows（PowerShell）

```powershell
# 进入项目目录
cd 'E:\桌面\Wayne File\课程\暑期课程\软件工程\test_2\Richman_play'

# ① 编译
.\build.bat

# ② 跑全部
.\dist\rich_demo.exe test .\testcases

# ③ 单个套件
.\dist\rich_demo.exe test .\testcases\Group1.json

# ④ 退出码（$true 表示全过）
$LASTEXITCODE -eq 0
```

> 提示：PowerShell 里调用外部程序请用 `.\` 前缀；`results` 报告若需清空：
> `Remove-Item results\*_report.json, results\*_actual.json -ErrorAction SilentlyContinue`

## 3. Linux（WSL，已有 WSL）

### 3.1 为什么要在 WSL 里重新编译

`dist/rich_demo.exe` 是 **Windows 程序**，在 WSL 里不能直接跑。
要测 Linux 版，需要在 WSL 里用 gcc 重新编译出 ELF 程序 `dist/rich_demo`。

### 3.2 步骤

```bash
# ① 进入项目（路径含中文/空格必须加引号；WSL 下 Windows 盘挂载在 /mnt/<盘符小写>）
cd "/mnt/e/桌面/Wayne File/课程/暑期课程/软件工程/test_2/Richman_play"

# ② 确认 gcc（Ubuntu/Debian 首次需安装）
sudo apt update && sudo apt install -y build-essential

# ③ 编译（产物：dist/rich_demo，与 .exe 同名不同后缀）
./build.sh

# ④ 跑全部 testcases
./dist/rich_demo test testcases

# ⑤ 只跑一个套件
./dist/rich_demo test testcases/Group3.json

# ⑥ 退出码（0=全过）
echo $?
```

> 常见问题：
> - 提示 `Permission denied`：`chmod +x build.sh`
> - Windows 编辑过的脚本报 `\r` 错误：`sed -i 's/\r$//' build.sh`（本项目脚本已是 LF，一般无需处理）
> - `/mnt/e` 路径访问慢属正常（跨文件系统）；追求速度可 `git clone` 到 WSL 家目录再测

### 3.3 对比测试（同一套用例，双平台验证）

```bash
# Windows PowerShell：
.\dist\rich_demo.exe test .\testcases; $LASTEXITCODE

# WSL：
./dist/rich_demo test testcases; echo $?
```

两边都应输出 `失败/错误用例合计 0 个` 且退出码 0，即双平台行为一致。

---

## 4. 当前状态（本次修正后）

| 套件 | 用例数 | PASS | 说明 |
|---|---:|---:|---|
| testcases/Group1.json | 258 | 258 | 已修正（详见 testcase_compliance_report.md 修正记录） |
| testcases/Group2.json | 400 | 400 | 已修正 1 例 |
| testcases/Group3.json | 179 | 179 | 已修正 5 例 |
| testcases/Group3_Testcases.json | 179 | 179 | 本组基线，无需改动 |

原始未修改副本备份于：`<test_2>/audit_scratch/backup/`
