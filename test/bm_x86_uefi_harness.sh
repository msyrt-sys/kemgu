#!/usr/bin/env bash
# UEFI gerçek firmware kapısı; çekirdek içerik anahtarlı önbellekten gelir (D-772), stub her koşumda üretilir.
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "${UEFI_YALNIZ_DERLE:-0}" != 1 ] && [ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]; then
 echo "ATLANDI (OVMF yok)"; exit 0
fi
if [ "${UEFI_YALNIZ_DERLE:-0}" != 1 ] && ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
 echo "ATLANDI (QEMU yok)"; exit 0
fi
# [D-772] Açılış (kapı) kipinde varsayılan çalışma dizini KOŞUM BAŞINA benzersizdir: paralel `make -j`
# altında uefi/uefi_kabuk/apic/ps2_irq aynı build/_bm_uefi'yi yarışarak yazıp okuyordu (yedi kapı kırmızı).
# Başarılı açılıştan sonra silinir; başarısızlıkta teşhis için kalır. Yalnız-derle kipinde (dışarıdan
# tüketen araçlar: blok harness'ı öncesi, tools/codex/*_sabotaj.py) eski sabit yol korunur.
# Eski araçlar derleme-yalnız çıktısını ve yayımlanan PC imajını sabit adla açar.
# Okuyucu QEMU da bitene kadar bu iki dış ürün aynı kilidin altında kalır.
if [[ ${UEFI_KILIT_ICINDE:-0} != 1 ]] && {
    [[ ${UEFI_YALNIZ_DERLE:-0} == 1 && -z ${UEFI_CALISMA_DIZINI:-} ]] ||
    [[ ${UEFI_PC_URET:-0} == 1 && ${UEFI_PC_GECICI:-0} != 1 ]];
}; then
    export UEFI_KILIT_ICINDE=1
    exec python3 tools/kapi/kilitli.py build/kilitler/uefi.lock bash test/bm_x86_uefi_harness.sh "$@"
fi
KENDI_W=""
if [ -n "${UEFI_CALISMA_DIZINI:-}" ]; then W="$UEFI_CALISMA_DIZINI"
elif [ "${UEFI_YALNIZ_DERLE:-0}" = 1 ]; then W=build/_bm_uefi
else mkdir -p build; W=$(mktemp -d build/uefi.XXXXXX); KENDI_W=1; fi
if [[ -n "$KENDI_W" ]]; then
    source test/calisma_dizini.sh
    calisma_izle "$W"
fi
mkdir -p "$W/esp/EFI/BOOT" "$W/esp/KEMGU"
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem runtime/mimari/x86_64/kem_gc_yurut.kem runtime/mimari/x86_64/kem_ekran_bga.kem runtime/mimari/x86_64/kem_uefi.kem runtime/mimari/x86_64/kem_ekran_gop.kem runtime/mimari/x86_64/kem_bellek_harita.kem runtime/mimari/x86_64/kem_acpi_uefi.kem runtime/mimari/x86_64/kem_apic_hw.kem runtime/mimari/x86_64/kem_kilit_hw.kem runtime/mimari/x86_64/kem_guc_hw.kem runtime/mimari/x86_64/kem_ps2_hw.kem"
KAYNAK="${UEFI_KAYNAK:-test/ornekler/kem_os_x86_uefi.kem}"
case "${KAYNAK##*/}" in
 kem_os_x86_kabuk.kem) RT="$RT runtime/mimari/x86_64/kem_surec_hw.kem runtime/mimari/x86_64/kem_rtc_hw.kem test/ornekler/kem_os_x86_blok_ortak.kem test/ornekler/kem_os_x86_vfs_ortak.kem" ;;
 kem_os_x86_blok.kem) RT="$RT test/ornekler/kem_os_x86_blok_ortak.kem" ;;
esac
rm -f "$W/k.elf" "$W/k.o" "$W/esp/EFI/BOOT/BOOTX64.EFI"
clang --target=x86_64-unknown-windows -ffreestanding -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Werror -O2 -c boot/efi/stub.c -o "$W/stub.obj"
# [D-768] `-` önekli bayraklar: MSYS (Windows CI) `/nodefaultlib` gibi `/` önekli
# argümanları POSIX yolu sanıp `D:/.../msys64/nodefaultlib`e çeviriyordu.
lld-link -subsystem:efi_application -entry:efi_main -nodefaultlib -out:"$W/esp/EFI/BOOT/BOOTX64.EFI" "$W/stub.obj"
cat $RT "$KAYNAK" > "$W/comb.kem"
python3 test/x86_heap_bagla.py "$W/comb.kem" || exit 2
# QEMU 8.2 ich9-ahci MSI yeteneğini kapatamıyor. Ayrı test çekirdeğinde yalnız keşfi atla.
if [[ "${UEFI_AHCI_MSI_YOK:-0}" == 1 ]]; then
    python3 - "$W/comb.kem" <<'KEM_MSI'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text(encoding='utf-8')
arama = 'değişken k: tam32 = bd_pci_yetenek(bdf,5);'
assert s.count(arama) == 1
p.write_text(s.replace(arama, 'değişken k: tam32 = 0;'), encoding='utf-8')
KEM_MSI
fi
# Konuk deneyi yalnız ayrıca bağlanan test görüntüsünde mevcuttur.
if [[ ${KEMGU_KILIT_DENETIM:-0} == 1 ]]; then
    python3 - "$W/comb.kem" <<'KEM_KILIT'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text(encoding="utf-8")
ayar = "sabit KEMGU_KILIT_DENETIM: mantıksal = yanlış;"
assert s.count(ayar) == 1
p.write_text(s.replace(ayar, ayar.replace("yanlış", "doğru")), encoding="utf-8")
KEM_KILIT
fi
if [[ -n ${UEFI_SMP_DENEY:-} ]]; then
    case "$UEFI_SMP_DENEY" in
        tlb) cat tests/qemu/smp_tlb_konuk.kem >> "$W/comb.kem" ;;
        bellek) cat tests/qemu/smp_bellek_konuk.kem >> "$W/comb.kem" ;;
        ipi) cat tests/qemu/smp_ipi_konuk.kem >> "$W/comb.kem" ;;
        kilit) cat tests/qemu/smp_kilit_konuk.kem >> "$W/comb.kem" ;;
        *) echo "Bilinmeyen SMP deneyi: $UEFI_SMP_DENEY" >&2; exit 1 ;;
    esac
fi
DERLEYICI="${UEFI_DERLEYICI:-build/kemgu}"
OBJ=("${BM_BASLANGIC_NESNESI:-build/bm_x86_start.o}" build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o)
source test/cekirdek_onbellek.sh
cekirdek_onbellek "$W" "$DERLEYICI" linker/bare-metal-uefi-x86_64.ld "${OBJ[@]}"
cp "$W/k.elf" "$W/esp/KEMGU/CEKIRDEK.ELF"
if [ "${UEFI_PC_URET:-0}" = 1 ]; then
 if [[ ${UEFI_PC_GECICI:-0} == 1 ]]; then export UEFI_PC_IMG="$W/pc.img"
 else export UEFI_PC_IMG="${UEFI_PC_IMG:-build/kem_os_pc.img}"; fi
 # [D-772] Paralel kapılar aynı imajı ezmesin: hedef UEFI_PC_IMG (bm_uefi.py de onu açar).
 python3 tests/qemu/uefi_imaj.py "$UEFI_PC_IMG" "$W/esp/EFI/BOOT/BOOTX64.EFI" "$W/k.elf"
fi
if [ "${UEFI_YALNIZ_DERLE:-0}" = 1 ]; then exit 0; fi
python3 tests/qemu/bm_uefi.py "$W"
