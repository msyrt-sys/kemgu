#!/usr/bin/env bash
# Gerçek CPU + xHCI; bağımsız FAT32 yazıcı/okuyucu ve fsck.
set -u
cd "$(dirname "$0")/.."
QEMU=qemu-system-x86_64
if [[ ${BM_YALNIZ_DERLE:-0} != 1 ]] && ! command -v "$QEMU" >/dev/null 2>&1; then
  echo "  ⚠ bm_x86_kabuk: $QEMU YOK — kapı ATLANDI (CI ortamı; Spark'ta koşar)"; exit 0
fi
TEKRAR="${TEKRAR:-20}"
source test/calisma_dizini.sh
source test/cekirdek_onbellek.sh
bm_calisma_baslat build/_bm_kabuk || exit 1
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem"
RT="$RT runtime/mimari/x86_64/kem_ps2_hw.kem runtime/mimari/x86_64/kem_gc_yurut.kem runtime/mimari/x86_64/kem_ekran_bga.kem"
OBJ=("${BM_BASLANGIC_NESNESI:-build/bm_x86_start.o}" build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o)

RT="$RT runtime/mimari/x86_64/kem_uefi.kem runtime/mimari/x86_64/kem_ekran_gop.kem runtime/mimari/x86_64/kem_bellek_harita.kem runtime/mimari/x86_64/kem_acpi_uefi.kem runtime/mimari/x86_64/kem_apic_hw.kem runtime/mimari/x86_64/kem_guc_hw.kem test/ornekler/kem_os_x86_blok_ortak.kem runtime/mimari/x86_64/kem_rtc_hw.kem runtime/mimari/x86_64/kem_surec_hw.kem test/ornekler/kem_os_x86_vfs_ortak.kem"
cat $RT test/ornekler/kem_os_x86_kabuk.kem > "$W/comb.kem" || exit 2
cekirdek_onbellek "$W" "${BM_DERLEYICI:-build/kemgu}" linker/bare-metal-x86_64.ld "${OBJ[@]}" || exit 2
grep -q 'target triple = "x86_64-unknown-none-elf"' "$W/ham.ll" || { echo "  🔴 üçlü bare-metal değil"; exit 2; }
if [[ ${BM_YALNIZ_DERLE:-0} == 1 ]]; then exit 0; fi

python3 tests/qemu/bm_kabuk.py "$W/k.elf" "$W"
