# Richman Play

命令行版大富翁游戏（C17）。

## 快速开始（仅需 gcc，推荐）

适用于全新 Windows 机器：只要安装了 **MinGW-w64 / gcc**，**不需要 CMake**。

### 1. 克隆

```powershell
git clone -b FINAL https://github.com/WireWhisperer/Richman_play.git
cd Richman_play
```

### 2. 确认 gcc 可用

```powershell
gcc --version
```

若提示找不到命令，把 MinGW 的 `bin` 目录加入系统 PATH，例如：

- `C:\mingw64\bin`
- `C:\msys64\mingw64\bin`

然后重新打开终端。

### 3. 编译

双击 **`build.bat`**，或在项目根目录执行：

```powershell
.\build.bat
```

成功后生成：

| 文件 | 路径 |
|------|------|
| 游戏程序 | `dist\rich_demo.exe` |
| 地图文件 | `dist\map.json` |

也可用 Makefile（若已安装 `mingw32-make`）：

```powershell
mingw32-make
```

### 4. 运行

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

**Linux / macOS（仅需 gcc）：**

```bash
git clone -b FINAL https://github.com/WireWhisperer/Richman_play.git
cd Richman_play
chmod +x build.sh run-game.sh run-tests.sh
./build.sh          # 或: make
./run-game.sh       # 或: ./dist/rich_demo
```

若没有 gcc：`sudo apt install build-essential`（Debian/Ubuntu）。

也可用 CMake：`cmake -S . -B build && cmake --build build`。

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

Linux / macOS：

```bash
./build.sh
./run-tests.sh
# 或: ./dist/rich_demo test testcases
# 指定用例目录: ./run-tests.sh testcases
```

报告与 Actual JSON 输出到 `results/`。

## 目录说明

| 路径 | 说明 |
|------|------|
| `dist/rich_demo` / `dist/rich_demo.exe` | **gcc** 生成的可执行文件（Linux / Windows） |
| `dist/map.json` | 游戏地图（编译时自动复制） |
| `spec/map.json` | 地图源文件 |
| `build.bat` / `build.sh` | 仅 gcc 一键编译（Windows / Linux） |
| `run-game.bat` / `run-game.sh` | 一键运行 |
| `run-tests.bat` / `run-tests.sh` | 一键自动化测试 |
| `Makefile` | `mingw32-make` / `make` 编译 |
| `sources.rsp` | gcc 源文件列表 |
| `testcases/` | 自动化测试用例 |
