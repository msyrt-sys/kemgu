#!/usr/bin/env bash
# F1: çok dosyalı imajı iki derleyicide ve bağımsız dış okuyucuyla doğrula.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
python3 tools/codex/f1.py hazirla
bash otomasyon/chatgpt/dogrula.sh tests/drivers/fs/fat32_coklu_test.kem
mkdir -p build/codex-f1
build/kemgu --llvm tests/drivers/fs/fat32_coklu_test.kem > build/codex-f1/dok.ll
clang -Wno-override-module -x ir build/codex-f1/dok.ll -x none build/kdl_runtime.o build/kdl_runtime_mmio.o -o build/codex-f1/dok
timeout 30 build/codex-f1/dok > build/codex-f1/seyrek.txt
python3 tools/codex/f1.py denetle
