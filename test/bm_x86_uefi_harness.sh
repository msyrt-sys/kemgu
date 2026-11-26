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
KENDI_W=""
if [ -n "${UEFI_CALISMA_DIZINI:-}" ]; then W="$UEFI_CALISMA_DIZINI"
elif [ "${UEFI_YALNIZ_DERLE:-0}" = 1 ]; then W=build/_bm_uefi
else mkdir -p build; W=$(mktemp -d build/uefi.XXXXXX); KENDI_W=1; fi
if [[ -n "$KENDI_W" ]]; then
    source test/calisma_dizini.sh
    calisma_izle "$W"
fi
mkdir -p "$W/esp/EFI/BOOT" "$W/esp/KEMGU"
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem runtime/mimari/x86_64/kem_gc_yurut.kem runtime/mimari/x86_64/kem_ekran_bga.kem runtime/mimari/x86_64/kem_uefi.kem runtime/mimari/x86_64/kem_ekran_gop.kem runtime/mimari/x86_64/kem_bellek_harita.kem runtime/mimari/x86_64/kem_acpi_uefi.kem runtime/mimari/x86_64/kem_apic_hw.kem runtime/mimari/x86_64/kem_guc_hw.kem runtime/mimari/x86_64/kem_ps2_hw.kem"
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
DERLEYICI="${UEFI_DERLEYICI:-build/kemgu}"
NESNELER="build/bm_x86_start.o build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o"
CLANG_IR="clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only -O2 -Wno-override-module"
# [D-772] Çekirdek önbelleği. Aynı derleyici + aynı birleşik kaynak tam takımda onlarca kez
# derleniyordu (self-host 74,5 sn, C 10,7 sn; bir koşumda ≥73 derleme). Anahtar ÇIKTIYI
# belirleyen her girdinin içeriğidir: derleyici ikilisi, birleşik kaynak (MSI yamasından SONRA),
# awk süzgeci, bağlayıcı betiği, altı başlangıç nesnesi, IR derleme komutu ve araç sürümleri.
# Herhangi biri değişirse anahtar değişir → yeniden derlenir. KEMGU_CEKIRDEK_ONBELLEK=0 kapatır.
cekirdek_derle() {
    "$DERLEYICI" --llvm --mimari x86_64-none "$W/comb.kem" > "$W/ham.ll"
    awk -f test/strip_defined_declares.awk "$W/ham.ll" > "$W/k.ll"
    $CLANG_IR -x ir "$W/k.ll" -c -o "$W/k.o"
    # shellcheck disable=SC2086
    ld.lld -m elf_x86_64 -T linker/bare-metal-uefi-x86_64.ld -o "$W/k.elf" "$W/k.o" $NESNELER
}
if [ "${KEMGU_CEKIRDEK_ONBELLEK:-1}" = 1 ]; then
    OB_KOK="${KEMGU_CEKIRDEK_ONBELLEK_KOK:-build/cekirdek_onbellek}"
    # shellcheck disable=SC2086
    ANAHTAR=$( { printf '%s\n' "$CLANG_IR" "$(clang --version | head -1)" "$(ld.lld --version | head -1)";
                sha256sum "$DERLEYICI" "$W/comb.kem" test/strip_defined_declares.awk \
                          linker/bare-metal-uefi-x86_64.ld $NESNELER | cut -d' ' -f1; } | sha256sum | cut -c1-32)
    OB="$OB_KOK/$ANAHTAR"
    # Paralel koşumda başka bir süreç kaydı tam kopyalama sırasında silebilir (eskime); kopya
    # düşerse sessizce yanlış ürün kullanılmaz, yeniden derlenir.
    if [ -f "$OB/k.elf" ] && cp "$OB/ham.ll" "$OB/k.ll" "$OB/k.o" "$OB/k.elf" "$W/" 2>/dev/null; then
        touch "$OB"
        echo "CEKIRDEK_ONBELLEK isabet ${ANAHTAR:0:12} ($DERLEYICI)"
    else
        cekirdek_derle
        mkdir -p "$OB_KOK"; ara=$(mktemp -d "$OB_KOK/.yaz.XXXXXX")
        cp "$W/ham.ll" "$W/k.ll" "$W/k.o" "$W/k.elf" "$ara/"
        # Yarışta başka bir koşum aynı anahtarı yazdıysa ikinci kopya atılır (içerik aynı).
        mv -T "$ara" "$OB" 2>/dev/null || rm -rf "$ara"
        # Disk sınırı: en son KULLANILAN 64 kayıt kalır (kayıt başına ~20 MB; tek tam koşum ~18 ayrı anahtar).
        ls -1dt "$OB_KOK"/*/ 2>/dev/null | tail -n +65 | xargs -r rm -rf
        echo "CEKIRDEK_ONBELLEK derlendi ${ANAHTAR:0:12} ($DERLEYICI)"
    fi
else
    cekirdek_derle
fi
cp "$W/k.elf" "$W/esp/KEMGU/CEKIRDEK.ELF"
if [ "${UEFI_PC_URET:-0}" = 1 ]; then
 # [D-772] Paralel kapılar aynı imajı ezmesin: hedef UEFI_PC_IMG (bm_uefi.py de onu açar).
 python3 tests/qemu/uefi_imaj.py "${UEFI_PC_IMG:-build/kem_os_pc.img}" "$W/esp/EFI/BOOT/BOOTX64.EFI" "$W/k.elf"
fi
if [ "${UEFI_YALNIZ_DERLE:-0}" = 1 ]; then exit 0; fi
python3 tests/qemu/bm_uefi.py "$W"
