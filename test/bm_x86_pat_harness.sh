#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source test/calisma_dizini.sh
if ! command -v qemu-system-x86_64 >/dev/null || [[ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]]; then
    echo 'ATLANDI (QEMU veya OVMF yok)'
    exit 0
fi
for derleyici in build/kemgu build/codegen; do
    w=$(mktemp -d build/pat.XXXXXX)
    trap 'calisma_hatasi "$?" "$w" || exit "$?"' EXIT
    UEFI_CALISMA_DIZINI="$w" UEFI_DERLEYICI="$derleyici" UEFI_YALNIZ_DERLE=1 bash test/bm_x86_uefi_harness.sh
    PYTHONPATH=tests/qemu python3 -c 'import bm_uefi; bm_uefi.boot()' "$w"
    rm -rf -- "$w"
done
