#!/usr/bin/env bash
# F2: KEMGU onarımı ve fsck.fat küme karşılaştırması.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
python3 tools/codex/f2.py
bash otomasyon/chatgpt/dogrula.sh tests/drivers/fs/fat32_onar_test.kem tests/drivers/fs/fat32_onar_kahin_test.kem
mkdir -p build/codex-f2
build/kemgu --llvm tests/drivers/fs/fat32_onar_kahin_test.kem > build/codex-f2/dok.ll
clang -Wno-override-module -x ir build/codex-f2/dok.ll -x none build/kdl_runtime.o build/kdl_runtime_mmio.o -o build/codex-f2/dok
timeout 30 build/codex-f2/dok > build/codex-f2/seyrek.txt
python3 tools/codex/f2_kahin.py
