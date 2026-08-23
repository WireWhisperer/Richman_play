# Richman Play

命令行版大富翁游戏（C17）。

## 快速开始

### 1. 克隆到本地空文件夹

```bash
git clone git@github.com:WireWhisperer/Richman_play.git
cd Richman_play
```

### 2. 使用 CMake 编译

**Windows（MinGW，无需 Visual Studio）**

先确认 `gcc` 和 `cmake` 在 PATH 中，然后**删除旧的 build 目录**（若之前配置失败过）：

```powershell
# 若 gcc 不在 PATH，先设置（按实际安装路径修改）
$env:Path = "D:\Download\MinGW\bin;" + $env:Path

Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

或直接双击 **`build-cmake.bat`**（自动使用 MinGW 预设）。

也可使用 CMake Preset：

```powershell
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --preset windows-mingw
```

**Linux / macOS**

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

> **常见错误**：若出现 `nmake` / `CMAKE_C_COMPILER not set`，说明 CMake 误用了 Visual Studio 生成器。请删除 `build` 文件夹，改用上面的 `-G "MinGW Makefiles"`。

| 文件 | 路径 |
|------|------|
| 游戏程序 | `build/dist/rich_demo.exe`（Windows）或 `build/dist/rich_demo`（Linux） |
| 地图文件 | `build/dist/map.json`（编译时自动复制） |

运行自动化测试：

```bash
ctest --test-dir build --output-on-failure
```

Windows 多配置生成器（Visual Studio）需指定配置：

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### 3. 运行游戏

**双击** `build/dist/rich_demo.exe` 即可开始（地图与程序在同一目录）。

或在终端运行：

```powershell
.\build\dist\rich_demo.exe
```

### 备选：build.bat（仅 Windows + MinGW）

若未安装 CMake，可双击 `build.bat`，生成 `dist/rich_demo.exe`。

## 游戏操作

1. **选玩家**：一行输入 2~4 位角色编号  
   - 例：`21` = 2 名玩家，阿土伯 + 钱夫人  
   - `1` 钱夫人 / `2` 阿土伯 / `3` 孙小美 / `4` 金贝贝
2. **初始资金**：直接回车默认 10000（范围 1000~50000）
3. **游戏中**：`ROLL` 掷骰子，`QUERY` 查状态，`HELP` 帮助，`QUIT` 退出

## 目录说明

| 路径 | 说明 |
|------|------|
| `build/dist/rich_demo.exe` | CMake 编译生成的可执行文件 |
| `build/dist/map.json` | 游戏地图（编译时自动复制） |
| `spec/map.json` | 地图源文件 |
| `testcases/` | 自动化测试用例 |
