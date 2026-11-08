#!/usr/bin/env bash
# BARE-METAL x86 NVMe kapısı (D-745): sans-IO NVMe sürücüsü gerçek x86 çekirdeğinde,
# QEMU q35 + NVMe. qtest kapısından (test/qtest_harness.sh) FARKI: CPU gerçekten
# koşar, firmware (SeaBIOS) BAR'ları atar, aygıt modeli AYRI iş parçacığında DMA
# yapar → eşzamanlılık yarışları görünür (ölçüldü: bayt bayt okuma tamamlama
# girdisini yırtıyordu; qtest bunu GÖREMEZ, her istek ayrı turdur).
# Kanıt üç katlı: seri çıktı "BM_NVME OK" + QEMU çıkış kodu 33 (isa-debug-exit 16)
# + QEMU kapandıktan SONRA disk imajı Python ile (yazılan alan + dokunulmayan alan).
# Yarışlar ARALIKLIDIR. Ölçüm (D-745): bayt bayt okuma 30 açılışta 2 kez yırtıldı
# (~%7); düzeltilmiş okuma 100/100 geçti. Varsayılan TEKRAR=20 → o gerilemeyi
# yakalama olasılığı ~%77 (1 - 0.93^20). Bu kapı bu sınıf için OLASILIKSALDIR;
# belirlenimci koruma sürücü düzeyinde faz-önce yeniden okuma + yırtık yazan sahte
# model testidir (LOOP E4j).
set -u
cd "$(dirname "$0")/.."
QEMU=qemu-system-x86_64
if ! command -v "$QEMU" >/dev/null 2>&1; then
  echo "  ⚠ bm_x86_nvme: $QEMU YOK — kapı ATLANDI (CI ortamı; Spark'ta koşar)"; exit 0
fi
TEKRAR="${TEKRAR:-20}"
W="build/_bm_nvme_$$"; mkdir -p "$W"; trap 'rm -rf "$W"' EXIT
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem"
BM="clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only"
OBJ="build/bm_x86_start.o build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o"

cat $RT test/ornekler/kem_os_x86_nvme.kem > "$W/comb.kem"
build/kemgu --llvm --mimari x86_64-none "$W/comb.kem" > "$W/ham.ll" 2> "$W/err" || { echo "  🔴 --llvm"; head "$W/err"; exit 2; }
grep -q 'target triple = "x86_64-unknown-none-elf"' "$W/ham.ll" || { echo "  🔴 üçlü bare-metal değil"; exit 2; }
awk -f test/strip_defined_declares.awk "$W/ham.ll" > "$W/k.ll"
$BM -O2 -Wno-override-module -x ir "$W/k.ll" -c -o "$W/k.o" 2> "$W/cerr" || { echo "  🔴 derleme"; head "$W/cerr"; exit 2; }
ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o "$W/k.elf" "$W/k.o" $OBJ 2> "$W/lerr" || { echo "  🔴 link"; head "$W/lerr"; exit 2; }

hata=0
for r in $(seq 1 "$TEKRAR"); do
  python3 - "$W/disk.img" <<'PY'
import sys
b = bytearray(4 * 1024 * 1024)
for s in range(len(b) // 512):
    for j in range(512):
        b[s * 512 + j] = (s * 7 + j * 13 + 5) & 255
open(sys.argv[1], "wb").write(b)
PY
  timeout 120 "$QEMU" -M q35 -m 256M -display none -no-reboot -serial stdio \
     -kernel "$W/k.elf" -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
     -drive "file=$W/disk.img,if=none,id=d0,format=raw" -device nvme,drive=d0,serial=kemgu01 \
     > "$W/seri.txt" 2>&1
  rc=$?
  if [ $rc -ne 33 ] || ! grep -q "BM_NVME OK" "$W/seri.txt"; then
    echo "  🔴 açılış $r: QEMU rc=$rc (beklenen 33)"; tail -3 "$W/seri.txt"; hata=1; continue
  fi
  if ! python3 - "$W/disk.img" <<'PY'
import sys
b = open(sys.argv[1], "rb").read()
for j in range(9 * 512):
    if b[200 * 512 + j] != (j * 31 + 11) & 255:
        print(f"  imaj: LBA 200 + bayt {j} YANLIS"); sys.exit(1)
for s in list(range(0, 200)) + list(range(209, 300)):
    for j in range(512):
        if b[s * 512 + j] != (s * 7 + j * 13 + 5) & 255:
            print(f"  imaj: sektor {s} BOZULDU"); sys.exit(1)
PY
  then echo "  🔴 açılış $r: imaj kâhini"; hata=1; fi
done
[ $hata -eq 0 ] && echo "=== bm_x86_nvme: bare-metal x86 NVMe $TEKRAR/$TEKRAR açılış GEÇTİ ($(grep -o 'kapasite=[0-9]*' "$W/seri.txt")) ===" || { echo "=== bm_x86_nvme: BAŞARISIZ ==="; exit 2; }
