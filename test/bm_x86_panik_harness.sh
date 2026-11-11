#!/usr/bin/env bash
# [D-748] BARE-METAL x86 EKRAN kapısı: QEMU std VGA + Bochs DISPI 640x480x32, PSF Türkçe
# metin. Kanıt: seri "EKRAN_HAZIR" + QMP screendump (PPM) — tests/qemu/bm_ekran.py
# görüntüyü KEMGU'dan BAĞIMSIZ çizer (PSF baytları fikstür kaynağından okunur, PSF
# ayrıştırma ve geri düşme kuralı Python'da yeniden yazılmıştır) ve BİREBİR karşılaştırır.
set -u
cd "$(dirname "$0")/.."
QEMU=qemu-system-x86_64
if ! command -v "$QEMU" >/dev/null 2>&1; then
  echo "  ⚠ bm_x86_panik: $QEMU YOK — kapı ATLANDI (CI ortamı; Spark'ta koşar)"; exit 0
fi
W="build/_bm_panik_$$"; mkdir -p "$W"; trap 'rm -rf "$W"' EXIT
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem runtime/mimari/x86_64/kem_gc_yurut.kem runtime/mimari/x86_64/kem_ekran_bga.kem"
BM="clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only"
OBJ="build/bm_x86_start.o build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o"
KAYNAK="test/ornekler/kem_os_x86_panik.kem"
rm -f "$W/k.elf" "$W/k.o"
cat $RT "$KAYNAK" > "$W/comb.kem"
build/kemgu --llvm --mimari x86_64-none "$W/comb.kem" > "$W/ham.ll" 2> "$W/err" || { echo "  🔴 --llvm"; head "$W/err"; exit 2; }
grep -q 'target triple = "x86_64-unknown-none-elf"' "$W/ham.ll" || { echo "  🔴 üçlü bare-metal değil"; exit 2; }
awk -f test/strip_defined_declares.awk "$W/ham.ll" > "$W/k.ll"
$BM -O2 -Wno-override-module -x ir "$W/k.ll" -c -o "$W/k.o" 2> "$W/cerr" || { echo "  🔴 derleme"; head "$W/cerr"; exit 2; }
ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o "$W/k.elf" "$W/k.o" $OBJ 2> "$W/lerr" || { echo "  🔴 link"; head "$W/lerr"; exit 2; }
python3 tests/qemu/bm_panik.py "$W/k.elf" "$W"
