# Richman Play

命令行版大富翁游戏（C17，代码本身也兼容 C11）。

## 快速开始（Windows：双击即可，无需安装任何依赖）

新机器上**不需要预先安装 MinGW、CMake 或任何工具**。双击 **`build.bat`**，它会自己找编译器：

1. 优先用 `tools\w64devkit` 里的便携编译器
2. 其次用系统 PATH 里已有的 gcc
3. 两个都没有 → 自动下载 w64devkit（约 77 MB）解压到 `tools\`，**不需要管理员权限**

只有第 3 步需要联网，而且只发生一次，之后每次双击都是秒编。整个过程不需要 CMake。

### 1. 克隆

```powershell
git clone -b <分支名> git@github.com:WireWhisperer/Richman_play.git
cd Richman_play
```

> 仓库默认分支 `main` 与开发分支内容不同，克隆前先确认要拉哪一个。

### 2. 编译

双击 **`build.bat`**，或在项目根目录执行：

```powershell
.\build.bat
```

成功后生成：

| 文件 | 路径 |
|------|------|
| 游戏程序 | `dist\rich_demo.exe` |
| 地图文件 | `dist\map.json` |

若自动下载工具链失败（网络受限），任选一种后再双击 `build.bat`：

```powershell
winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
```

或手动下载 `w64devkit-1.23.0.zip` 放进 `tools\` 目录（脚本会自动解压）：
<https://github.com/skeeto/w64devkit/releases/download/v1.23.0/w64devkit-1.23.0.zip>

也可用 Makefile（若已安装 `mingw32-make`）：

```powershell
mingw32-make
```

### 3. 运行

双击 **`run-game.bat`**，或：

```powershell
.\dist\rich_demo.exe
```

> 注意：请从 `dist` 目录运行，或使用 `run-game.bat`，确保同目录有 `map.json`。

---

## 备选：CMake 编译

若机器同时装了 CMake，也可使用：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物在 `build\dist\rich_demo.exe`。也可双击 `build-cmake.bat`。

**Linux / macOS：**

```bash
git clone -b <分支名> git@github.com:WireWhisperer/Richman_play.git
cd Richman_play
chmod +x build.sh run-game.sh
./build.sh          # 或: make
./run-game.sh       # 或: ./dist/rich_demo
```

若没有编译器：`sudo apt install build-essential`（Debian/Ubuntu），macOS 用 `xcode-select --install`。

也可用 CMake：`cmake -S . -B build && cmake --build build`。

---

## 编译器要求

| 构建方式 | 要求 |
|----------|------|
| `build.bat` / `build.sh` / `Makefile` | 任意 gcc 或 clang。优先用 `-std=c17`，编译器不支持时自动回退 `-std=c11` |
| CMake | CMake 3.16+。`CMAKE_C_STANDARD_REQUIRED` 设为 OFF，编译器不支持 C17 时自动降级 |

`tools/` 存放自动下载的便携工具链，已被 `.gitignore` 排除，不会进仓库。

---

## 游戏操作

1. **选玩家**：一行输入 2~4 位角色编号  
   - 例：`21` = 阿土伯 + 钱夫人  
   - `1` 钱夫人 / `2` 阿土伯 / `3` 孙小美 / `4` 金贝贝
2. **初始资金**：直接回车默认 10000（范围 1000~50000）
3. **游戏中**：`ROLL` 掷骰，`QUERY` 查资产，`HELP` 帮助，`QUIT` 退出  
   任意提示阶段也可输入 `QUIT` 退出

## 自动化测试

```powershell
.\build.bat
.\dist\rich_demo.exe test testcases
# 或双击 run-tests.bat
```

Linux：

```bash
./build.sh
./dist/rich_demo test testcases
```

报告与 Actual JSON 输出到 `results/`。

## 目录说明

| 路径 | 说明 |
|------|------|
| `dist/rich_demo` / `dist/rich_demo.exe` | **gcc** 生成的可执行文件（Linux / Windows） |
| `dist/map.json` | 游戏地图（编译时自动复制） |
| `spec/map.json` | 地图源文件 |
| `build.bat` / `build.sh` | 一键编译（Windows / Linux），自动准备编译器 |
| `run-game.bat` / `run-game.sh` | 一键运行 |
| `Makefile` | `mingw32-make` / `make` 编译 |
| `sources.rsp` | gcc 源文件列表 |
| `testcases/` | 自动化测试用例 |
| `tools/` | 自动下载的便携编译器，不入库 |
