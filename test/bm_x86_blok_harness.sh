#!/usr/bin/env bash
# Üç aygıt birlikte; C ve self-host için taze imajlar ve ayrı QEMU.
set -euo pipefail
cd "$(dirname "$0")/.."
source test/calisma_dizini.sh
if ! command -v qemu-system-x86_64 >/dev/null || [ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]; then
 echo "ATLANDI (QEMU veya OVMF yok)"
 exit 0
fi
for c in ${BLOK_DERLEYICILER:-build/kemgu build/codegen}; do
 # [D-772] Kendi çalışma dizini (paralel koşumda build/_bm_uefi paylaşılıyordu); başarıda silinir.
 w=$(mktemp -d build/blok.XXXXXX)
 trap 'calisma_hatasi "$?" "$w" || exit "$?"' EXIT
 UEFI_CALISMA_DIZINI="$w" UEFI_YALNIZ_DERLE=1 UEFI_KAYNAK=test/ornekler/kem_os_x86_blok.kem UEFI_DERLEYICI="$c" bash test/bm_x86_uefi_harness.sh
 python3 tests/qemu/blok_kahin.py "$w"
 rm -rf "$w"
done
