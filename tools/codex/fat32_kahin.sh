#!/usr/bin/env bash
# B8: her iki derleyici, bağımsız dosfstools ve Linux'tan gelen fikstür.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
python3 tools/codex/b8.py hazirla
bash otomasyon/chatgpt/dogrula.sh tests/drivers/fs/fat32_kahin_test.kem tests/drivers/fs/fat32_linux_test.kem
build/kemgu --llvm tests/drivers/fs/fat32_kahin_test.kem > build/codex-kahin/dok.ll
clang -Wno-override-module -x ir build/codex-kahin/dok.ll -x none build/kdl_runtime.o build/kdl_runtime_mmio.o -o build/codex-kahin/dok
timeout 30 build/codex-kahin/dok > build/codex-kahin/seyrek.txt
python3 tools/codex/b8.py denetle
