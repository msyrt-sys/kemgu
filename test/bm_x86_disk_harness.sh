#!/usr/bin/env bash
# C ve self-host için ayrı çekirdek. Kâhin her bağımsız koşumda diskleri yeniden üretir.
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v qemu-system-x86_64 >/dev/null || ! command -v fsck.fat >/dev/null || [ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]; then
 echo "ATLANDI (QEMU, OVMF veya fsck.fat yok)"
 exit 0
fi
W="${DALGA12_CALISMA_DIZINI:-build/_bm_disk}"
for c in ${DALGA12_DERLEYICILER:-build/kemgu build/codegen}; do
 echo "DALGA12 DERLEYICI $c"
 UEFI_CALISMA_DIZINI="$W" UEFI_YALNIZ_DERLE=1 UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem UEFI_DERLEYICI="$c" bash test/bm_x86_uefi_harness.sh
 DALGA12_YAZ=1 DALGA12_COK_USB="${DALGA12_COK_USB:-1}" DALGA12_KESINTI="${DALGA12_KESINTI:-1}" DALGA12_TEKRAR="${DALGA12_TEKRAR:-1}" python3 tests/qemu/dalga12_kahin.py "$W"
done
