#!/usr/bin/env bash
# [D-777] Çekirdek dizi büyütmesinde YERİNDE UZATMA kapısı (iki derleyici, gerçek QEMU).
# Deterministik: yoklama döngüsü YOK. Dosya oturumunun heap kullanımı zamanlamaya bağlı
# (aygıt yoklaması her turda tahsis eder; aynı kaynağın iki derlemesi %33 ayrıştı) → o
# metrik AYIRT EDİCİ DEĞİL. Bu program 64 diziyi sırayla 1024 elemana büyütür ve malloc
# bump işaretçisinin ilerlemesini basar. Ölçüm: açık C 262336 / self 327952 bayt,
# kapalı (S271) C 524672 / self 590288 bayt → eşik 400000. [D-778] Dar eleman: 64 x 1024
# Dizi<dtam8> bump 65584 (iki derleyici) → eşik 150000.
set -u
cd "$(dirname "$0")/.."
if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
  echo "  ⚠ bm_x86_heap_uzat: qemu-system-x86_64 YOK — kapı ATLANDI"; exit 0
fi
ESIK="${HEAP_UZAT_ESIK:-400000}"
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem runtime/mimari/x86_64/kem_gc_yurut.kem runtime/mimari/x86_64/kem_ekran_bga.kem"
OBJ="build/bm_x86_start.o build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o"
BM="clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only -O2 -Wno-override-module"
kaldi=0
for c in build/kemgu build/codegen; do
  W=$(mktemp -d build/heap_uzat.XXXXXX)
  cat $RT test/ornekler/kem_os_x86_heap_uzat.kem > "$W/comb.kem"
  "$c" --llvm --mimari x86_64-none "$W/comb.kem" > "$W/ham.ll" 2> "$W/err" || { echo "  🔴 $c --llvm"; head "$W/err"; exit 2; }
  awk -f test/strip_defined_declares.awk "$W/ham.ll" > "$W/k.ll"
  $BM -x ir "$W/k.ll" -c -o "$W/k.o" 2> "$W/cerr" || { echo "  🔴 $c derleme"; head "$W/cerr"; exit 2; }
  ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o "$W/k.elf" "$W/k.o" $OBJ 2> "$W/lerr" || { echo "  🔴 $c link"; head "$W/lerr"; exit 2; }
  timeout 120 qemu-system-x86_64 -M q35 -m 256M -display none -vga std -no-reboot -serial file:"$W/seri" \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 -kernel "$W/k.elf" >/dev/null 2>&1
  rc=$?
  satir=$(grep -a "HEAP_UZAT" "$W/seri" | tail -1)
  bump=$(echo "$satir" | sed -n 's/.*bump \([0-9]*\) hata \([0-9]*\).*/\1/p')
  yk=$(echo "$satir" | sed -n 's/.*bump \([0-9]*\) hata \([0-9]*\).*/\2/p')
  # kem_qemu_cik(0) → QEMU çıkış kodu (0<<1)|1 = 1; zaman aşımı 124.
  if [ "$rc" != 1 ] || [ -z "$bump" ]; then echo "  🔴 $c: QEMU rc=$rc satır='$satir'"; tail -c 400 "$W/seri"; kaldi=$((kaldi+1)); continue; fi
  if [ "$yk" != 0 ]; then echo "  🔴 $c: içerik bozuk (yanlis_kod=$yk)"; kaldi=$((kaldi+1)); continue; fi
  # [D-778] dar eleman: 64 x 1024 Dizi<dtam8>. 1 baytlık yuva → tek 64 KB blok (ölçüm 65584
  # iki derleyicide); eski 4 baytlık yuva ~4x. Eşik 150000.
  bayt=$(grep -a "HEAP_UZAT bayt" "$W/seri" | tail -1 | sed -n 's/.*bayt \([0-9]*\).*/\1/p')
  if [ -z "$bayt" ] || [ "$bayt" -gt "${HEAP_BAYT_ESIK:-150000}" ]; then echo "  🔴 $c: dar eleman bump '$bayt' > ${HEAP_BAYT_ESIK:-150000} (bayt dizisi 1 baytlık yuvada değil)"; kaldi=$((kaldi+1)); continue; fi
  echo "  ✅ $c: Dizi<dtam8> 64x1024 → bump $bayt (1 baytlık yuva)"
  if [ "$bump" -gt "$ESIK" ]; then echo "  🔴 $c: bump $bump > eşik $ESIK (yerinde uzatma çalışmıyor)"; kaldi=$((kaldi+1)); continue; fi
  echo "  ✅ $c: bump $bump ≤ $ESIK, içerik doğru"
  # OOM: büyütme tahsisi başarısızsa TEMİZ PANİK (kem_panik sonsuz döngüde bekler → mesaj
  # görülünce QEMU kapatılır). Koruma yokken tampon NULL'a kurulup sessizce yazılıyordu.
  rm -f "$W/seri_oom"
  qemu-system-x86_64 -M q35 -m 256M -display none -vga std -no-reboot -serial file:"$W/seri_oom" \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 -kernel "$W/k.elf" -append oom >/dev/null 2>&1 &
  qp=$!; son=$((SECONDS+120)); oom=""
  while [ $SECONDS -lt $son ] && kill -0 "$qp" 2>/dev/null; do
    if grep -aq "PANIK: dizi buyutulemedi" "$W/seri_oom" 2>/dev/null; then oom=ok; break; fi
    sleep 0.5
  done
  kill "$qp" 2>/dev/null; wait "$qp" 2>/dev/null
  if [ "$oom" != ok ]; then echo "  🔴 $c: OOM'da temiz panik YOK"; tail -c 300 "$W/seri_oom"; kaldi=$((kaldi+1)); continue; fi
  echo "  ✅ $c: OOM → temiz panik"
  rm -rf "$W"
done
[ "$kaldi" = 0 ] && echo "=== heap yerinde uzatma + OOM paniği: 2/2 ===" || { echo "=== heap yerinde uzatma: BAŞARISIZ ($kaldi) ==="; exit 1; }
