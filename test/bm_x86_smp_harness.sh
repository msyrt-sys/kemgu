#!/usr/bin/env bash
# Her derleyiciye ayrı gerçek UEFI imajı, her topolojiye özel QMP ve seri günlüğü.
set -euo pipefail
cd "$(dirname "$0")/.."
kip=${1:-kesif}
if ! command -v qemu-system-x86_64 >/dev/null || [[ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]]; then
    echo "ATLANDI (SMP için QEMU x86 veya OVMF yok)"
    exit 0
fi
source test/calisma_dizini.sh
w=$(calisma_ac build/smp)
calisma_izle "$w"
for derleyici in kemgu codegen; do
    tur="$w/$derleyici"
    deney=""
    if [[ $kip == kilit || $kip == ipi ]]; then deney=$kip; fi
    UEFI_SMP_DENEY="$deney" UEFI_KAYNAK=test/ornekler/kem_os_x86_uefi.kem UEFI_DERLEYICI="build/$derleyici" \
        UEFI_CALISMA_DIZINI="$tur" UEFI_PC_URET=1 UEFI_PC_GECICI=1 UEFI_YALNIZ_DERLE=1 \
        bash test/bm_x86_uefi_harness.sh
    echo "SMP $kip ($derleyici)"
    python3 tests/qemu/smp_kahin.py "$tur" "$kip"
done
