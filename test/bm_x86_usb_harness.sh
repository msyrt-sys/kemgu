#!/usr/bin/env bash
# Gerçek CPU + xHCI; bağımsız FAT32 yazıcı/okuyucu ve fsck.
set -u
cd "$(dirname "$0")/.."
QEMU=qemu-system-x86_64
if [[ ${BM_YALNIZ_DERLE:-0} != 1 ]] && ! command -v "$QEMU" >/dev/null 2>&1; then
  echo "  ⚠ bm_x86_usb: $QEMU YOK — kapı ATLANDI (CI ortamı; Spark'ta koşar)"; exit 0
fi
TEKRAR="${TEKRAR:-20}"
source test/calisma_dizini.sh
source test/cekirdek_onbellek.sh
bm_calisma_baslat build/_bm_usb || exit 1
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem"
RT="$RT runtime/mimari/x86_64/kem_gc_yurut.kem"
OBJ=("${BM_BASLANGIC_NESNESI:-build/bm_x86_start.o}" build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o)

cat $RT test/ornekler/kem_os_x86_usb.kem > "$W/comb.kem" || exit 2
cekirdek_onbellek "$W" "${BM_DERLEYICI:-build/kemgu}" linker/bare-metal-x86_64.ld "${OBJ[@]}" || exit 2
grep -q 'target triple = "x86_64-unknown-none-elf"' "$W/ham.ll" || { echo "  🔴 üçlü bare-metal değil"; exit 2; }
if [[ ${BM_YALNIZ_DERLE:-0} == 1 ]]; then exit 0; fi

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
