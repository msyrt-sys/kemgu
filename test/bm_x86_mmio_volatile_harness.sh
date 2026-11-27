#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source test/calisma_dizini.sh
for derleyici in build/kemgu build/codegen; do
    w=$(mktemp -d build/mmio.XXXXXX)
    trap 'calisma_hatasi "$?" "$w" || exit "$?"' EXIT
    UEFI_CALISMA_DIZINI="$w" UEFI_YALNIZ_DERLE=1 UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem UEFI_DERLEYICI="$derleyici" bash test/bm_x86_uefi_harness.sh
    clang -O2 -target x86_64-unknown-none -mgeneral-regs-only -ffreestanding -nostdlib -Wno-override-module -x ir -S -emit-llvm "$w/k.ll" -o "$w/o2.ll"
    python3 tools/codex/mmio_volatile.py "$w"
    rm -rf -- "$w"
done
