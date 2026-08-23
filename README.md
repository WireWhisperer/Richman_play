# Richman Play

命令行版大富翁游戏，使用 C17 和 CMake 构建。

## 构建和测试

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Windows 的 Visual Studio 生成器通常输出到 `build/Debug/rich_demo.exe`；Linux
和单配置生成器通常输出到 `build/rich_demo`。

## 当前功能：US03 玩家设置

程序启动后：

1. 输入 2～4 的玩家数量；
2. 每名玩家输入 1～4 选择不重复角色；
3. 程序按选择顺序显示玩家、角色、颜色和标识；
4. 该顺序保存在 `Game.players` 中，作为后续回合顺序。

角色映射：钱夫人（红色/Q）、阿土伯（绿色/A）、孙小美（蓝色/S）、
金贝贝（黄色/J）。
