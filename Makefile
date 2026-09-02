# gcc-only build (Windows MinGW / Linux / macOS)
# Usage:
#   mingw32-make          # Windows MinGW
#   make                  # Linux / macOS / MSYS2
#   ./build.sh            # Linux / macOS shell script

CC      ?= gcc
CFLAGS  ?= -std=c17 -O2 -Wall -Wextra -Iinclude -Ithird_party/cJSON
LDFLAGS ?=

SRCS = \
	src/main.c \
	src/game.c \
	src/game_mine.c \
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
