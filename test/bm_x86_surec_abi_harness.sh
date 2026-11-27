#!/usr/bin/env bash
# Her derleyici için ayrı PVH/QEMU açılışı, tam seri çıktısı.
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v qemu-system-x86_64 >/dev/null; then
    echo 'ATLANDI (QEMU yok)'
    exit 0
fi
W=$(mktemp -d build/surec_abi.XXXXXX) || exit 1
source test/calisma_dizini.sh
calisma_izle "$W"
mkdir -p "$W"
RT=$(python3 - <<'PY'
from pathlib import Path
import re
print(re.search(r'^RT="([^"]+)"', Path("test/bm_x86_istisna_harness.sh").read_text(encoding="utf-8"), re.M)[1])
PY
)
cat $RT test/ornekler/kem_os_x86_surec_abi.kem > "$W/comb.kem"
"${KEMGU:-build/kemgu}" --check --mimari x86_64-none "$W/comb.kem"
for derleyici in ${SUREC_ABI_DERLEYICILER:-build/kemgu build/codegen}; do
    rm -f "$W/k.elf" "$W/k.o" "$W/seri"
    "$derleyici" --llvm --mimari x86_64-none "$W/comb.kem" > "$W/ham.ll"
    awk -f test/strip_defined_declares.awk "$W/ham.ll" > "$W/k.ll"
    clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only \
        -O2 -Wno-override-module -x ir "$W/k.ll" -c -o "$W/k.o"
    ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o "$W/k.elf" "$W/k.o" \
        build/bm_x86_start.o build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o \
        build/bm_x86_yazdir.o build/bm_x86_panik.o
    for ((acilis=1; acilis<=${SUREC_ABI_TEKRAR:-10}; acilis++)); do
    rm -f "$W/seri"
    rc=0
    timeout 20 qemu-system-x86_64 -M q35 -m 256M -display none -no-reboot \
        -serial "file:$W/seri" -device isa-debug-exit,iobase=0xf4,iosize=0x04 -kernel "$W/k.elf" || rc=$?
    if [[ "$rc" != 1 ]] || ! cmp -s "$W/seri" <(printf 'SUREC_ABI_BOOT\nSUREC_ABI_OK v2\n'); then
        cat "$W/seri"
        echo "Süreç ABI kapısı başarısız ($derleyici, QEMU rc=$rc)"
        exit 1
    fi
    echo "✓ Süreç ABI v2 ($derleyici, açılış $acilis, tek boot)"
    done
done
