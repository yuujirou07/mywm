#!/bin/bash

set -e

# どこから実行してもプロジェクト直下(my_txt_editor/)を基準にする
cd "$(dirname "$0")/.."

TEST_SRC="tests/editor_log_test.c src/editor_log.c src/path_util.c"
FLAGS="-Wall -g3 -O0 -fsanitize=address,undefined -Wextra -Werror -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -I./include -I./include/lsp_src"
# 出力先はtest_build/直下に置く。path_util.cが実行ファイルの2つ上を基準にするため
OUT="test_build/editor_log_test"

mkdir -p test_build log_data

gcc $FLAGS $TEST_SRC -lncursesw -o $OUT
echo "Build OK: $OUT"
