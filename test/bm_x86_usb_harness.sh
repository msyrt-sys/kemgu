#!/usr/bin/env bash
# Gerçek CPU + xHCI; bağımsız FAT32 yazıcı/okuyucu ve fsck.
set -u
cd "$(dirname "$0")/.."
QEMU=qemu-system-x86_64
if ! command -v "$QEMU" >/dev/null 2>&1; then
  echo "  ⚠ bm_x86_usb: $QEMU YOK — kapı ATLANDI (CI ortamı; Spark'ta koşar)"; exit 0
fi
TEKRAR="${TEKRAR:-20}"
W="build/_bm_usb_$$"; mkdir -p "$W"; trap 'rm -rf "$W"' EXIT
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem"
RT="$RT runtime/mimari/x86_64/kem_gc_yurut.kem"
BM="clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only"
OBJ="build/bm_x86_start.o build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o"

cat $RT test/ornekler/kem_os_x86_usb.kem > "$W/comb.kem"
build/kemgu --llvm --mimari x86_64-none "$W/comb.kem" > "$W/ham.ll" 2> "$W/err" || { echo "  🔴 --llvm"; head "$W/err"; exit 2; }
grep -q 'target triple = "x86_64-unknown-none-elf"' "$W/ham.ll" || { echo "  🔴 üçlü bare-metal değil"; exit 2; }
awk -f test/strip_defined_declares.awk "$W/ham.ll" > "$W/k.ll"
$BM -O2 -Wno-override-module -x ir "$W/k.ll" -c -o "$W/k.o" 2> "$W/cerr" || { echo "  🔴 derleme"; head "$W/cerr"; exit 2; }
ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o "$W/k.elf" "$W/k.o" $OBJ 2> "$W/lerr" || { echo "  🔴 link"; head "$W/lerr"; exit 2; }

hata=0
for r in $(seq 1 "$TEKRAR"); do
  python3 tests/qemu/fat_yazici.py yaz "$W/disk.img" || exit 2
  timeout 60 "$QEMU" -M q35 -m 256M -display none -no-reboot -serial stdio \
    -kernel "$W/k.elf" -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -device qemu-xhci,id=x -drive "file=$W/disk.img,if=none,id=u0,format=raw" \
    -device usb-storage,bus=x.0,drive=u0 > "$W/seri.txt" 2>&1
  rc=$?
  if [ "$rc" -ne 33 ] || ! grep -qx 'BM_USB OK' "$W/seri.txt"; then
    echo "🔴 açılış $r: QEMU rc=$rc"; cat "$W/seri.txt"; hata=$((hata+1)); continue
  fi
  fsck.fat -n "$W/disk.img" || hata=$((hata+1))
  python3 tests/qemu/fat_yazici.py denetle "$W/disk.img" || hata=$((hata+1))
done
echo "=== bm_x86_usb: $TEKRAR açılış, $hata hata ==="
[ "$hata" -eq 0 ]
