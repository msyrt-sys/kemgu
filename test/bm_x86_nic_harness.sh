#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v qemu-system-x86_64 >/dev/null || [[ ! -r /usr/share/OVMF/OVMF_CODE_4M.fd ]]; then
    echo 'ATLANDI (QEMU veya OVMF yok): NIC gidiş dönüşü'
    exit 0
fi
for derleyici in build/kemgu build/codegen; do
    w=$(mktemp -d build/nic.XXXXXX)
    echo "NIC derleyicisi $derleyici ($w)"
    UEFI_CALISMA_DIZINI="$w" UEFI_DERLEYICI="$derleyici" UEFI_YALNIZ_DERLE=1 \
        UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem bash test/bm_x86_uefi_harness.sh
    python3 tests/qemu/e1000_kahin.py "$w"
    python3 tests/qemu/e1000_kahin.py "$w" --kopru
done
