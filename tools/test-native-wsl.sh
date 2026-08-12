#!/usr/bin/env bash
# 在 WSL 內用 g++ 建置並執行 native core 測試。
# 由 tools/test-native.cmd 在本機沒有可用 MSVC 時自動呼叫，也可單獨執行。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNITY="${ROOT}/.pio/libdeps/native/Unity/src"
OUT="${ROOT}/.pio/build/wsl-native"

if [ ! -f "${UNITY}/unity.c" ]; then
  echo "Unity 尚未下載。請先在 Windows 端執行一次 .\\tools\\pio.cmd test -e native" >&2
  exit 2
fi

mkdir -p "${OUT}"

# 與 platformio.ini [env] 的旗標一致，讓兩條路徑看到同一組警告。
g++ -std=c++17 -Wall -Wextra -Wshadow \
  -I"${ROOT}/include" -I"${UNITY}" \
  "${ROOT}"/src/core/*.cpp \
  "${ROOT}/test/test_core/test_main.cpp" \
  "${UNITY}/unity.c" \
  -o "${OUT}/test_core"

"${OUT}/test_core"
