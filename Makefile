# gcc-only build (Windows MinGW / Linux / macOS)
# Usage:
#   mingw32-make          # Windows MinGW
#   make                  # Linux / macOS / MSYS2
#   ./build.sh            # Linux / macOS shell script

# Windows 上强制用 cmd.exe 执行配方：即使在 Git Bash / MSYS2 里调用
# mingw32-make，下面的 copy / del 等 CMD 语法也不会被 sh 拿去解析。
ifeq ($(OS),Windows_NT)
  SHELL = cmd.exe
  .SHELLFLAGS = /c
endif

# GNU make 内建 CC=cc，用 ?= 覆盖不了它，所以只在 CC 仍是内建值时改成 gcc。
# 命令行（make CC=clang）和环境变量仍然优先。
ifeq ($(origin CC),default)
  CC := gcc
endif

# 默认 c11：兼容性最好，老 MinGW / 旧 clang 也能编。
# 想用 C17（需要 gcc 8+ / clang 6+）：  make STD=-std=c17
STD     ?= -std=c11
CFLAGS  ?= $(STD) -O2 -Wall -Wextra -Iinclude -Ithird_party/cJSON
LDFLAGS ?=

SRCS = \
	src/main.c \
	src/game.c \
	src/game_mine.c \
	src/game_jail.c \
	src/game_toolShop.c \
	src/game_property.c \
	src/game_giftShop.c \
	src/usr_action.c \
	src/usr_judge.c \
	src/file_utils.c \
	src/path_utils.c \
	src/case_loader.c \
	src/action_executor.c \
	src/actual_writer.c \
	src/expected_checker.c \
	src/test_runner.c \
	src/manual_ui.c \
	src/player_setup.c \
	src/console.c \
	third_party/cJSON/cJSON.c

OUT_DIR = dist
ifeq ($(OS),Windows_NT)
  EXE = $(OUT_DIR)/rich_demo.exe
  MKDIR = if not exist $(OUT_DIR) mkdir $(OUT_DIR)
  CP_MAP = copy /Y spec\map.json dist\map.json
  RM = del /Q
else
  EXE = $(OUT_DIR)/rich_demo
  MKDIR = mkdir -p $(OUT_DIR)
  CP_MAP = cp -f spec/map.json $(OUT_DIR)/map.json
  RM = rm -f
endif

.PHONY: all clean run

all: $(EXE) $(OUT_DIR)/map.json

$(OUT_DIR):
	$(MKDIR)

$(EXE): $(SRCS) | $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

$(OUT_DIR)/map.json: spec/map.json | $(OUT_DIR)
	$(CP_MAP)

run: all
	./$(EXE)

clean:
	-$(RM) $(EXE) $(OUT_DIR)/map.json
