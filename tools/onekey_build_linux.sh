#!/usr/bin/env bash
# 一键克隆 FINAL+TEST 并编译（Linux / macOS，仅需 gcc）
set -euo pipefail
cd "$(dirname "$0")"

REPO_URL="https://github.com/WireWhisperer/Richman_play.git"
BRANCH="FINAL+TEST"
DIR="Richman_play"

echo "============================================"
echo " 一键克隆 + 编译 ${BRANCH}"
echo "============================================"

if ! command -v git >/dev/null 2>&1; then
    echo "[错误] 未找到 git。安装：sudo apt install git"
    exit 1
fi

if [[ ! -f CMakeLists.txt ]]; then
    if [[ -d "$DIR/.git" ]]; then
        echo "[1/3] 目录已存在，更新到最新代码..."
        (cd "$DIR" && git checkout "$BRANCH" && git pull --ff-only) || \
            echo "[警告] 更新失败（可能有本地改动），继续编译当前代码。"
    else
        echo "[1/3] 克隆 ${BRANCH} 分支..."
        if ! git clone -b "$BRANCH" "$REPO_URL" "$DIR"; then
            echo "[错误] 克隆失败。若无法直连 GitHub，请先配置代理，例如："
            echo "  git config --global http.proxy http://127.0.0.1:7890"
            echo "  git config --global https.proxy http://127.0.0.1:7890"
            exit 1
        fi
    fi
    cd "$DIR"
else
    echo "[1/3] 当前已在工程目录内，跳过克隆，直接编译。"
fi

chmod +x build.sh run-game.sh

echo "[2/3] 编译（gcc）..."
./build.sh

echo "[3/3] 编译完成！"
echo "  运行游戏: ./dist/rich_demo"
echo "  运行测试: ./dist/rich_demo test testcases"
if [[ "${1:-}" == "test" ]]; then
    echo "运行测试..."
    ./dist/rich_demo test testcases
fi
echo "============================================"
