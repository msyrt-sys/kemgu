#!/usr/bin/env bash
# UEFI gerçek firmware kapısı; ikililer her koşumda yeniden üretilir.
set -euo pipefail
cd "$(dirname "$0")/.."
if [ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]; then
 echo "ATLANDI (OVMF yok)"; exit 0
fi
if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
 echo "ATLANDI (QEMU yok)"; exit 0
fi
W="build/_bm_uefi"; mkdir -p "$W/esp/EFI/BOOT" "$W/esp/KEMGU"
RT="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem runtime/mimari/x86_64/kem_gc_yurut.kem runtime/mimari/x86_64/kem_ekran_bga.kem runtime/mimari/x86_64/kem_uefi.kem runtime/mimari/x86_64/kem_ekran_gop.kem runtime/mimari/x86_64/kem_bellek_harita.kem runtime/mimari/x86_64/kem_acpi_uefi.kem runtime/mimari/x86_64/kem_ps2_hw.kem"
KAYNAK="${UEFI_KAYNAK:-test/ornekler/kem_os_x86_uefi.kem}"
rm -f "$W/k.elf" "$W/k.o" "$W/esp/EFI/BOOT/BOOTX64.EFI"
clang --target=x86_64-unknown-windows -ffreestanding -fno-stack-protector -fno-builtin -mno-red-zone -Wall -Wextra -Werror -O2 -c boot/efi/stub.c -o "$W/stub.obj"
lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib /out:"$W/esp/EFI/BOOT/BOOTX64.EFI" "$W/stub.obj"
cat $RT "$KAYNAK" > "$W/comb.kem"
"${UEFI_DERLEYICI:-build/kemgu}" --llvm --mimari x86_64-none "$W/comb.kem" > "$W/ham.ll"
awk -f test/strip_defined_declares.awk "$W/ham.ll" > "$W/k.ll"
clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only -O2 -Wno-override-module -x ir "$W/k.ll" -c -o "$W/k.o"
ld.lld -m elf_x86_64 -T linker/bare-metal-uefi-x86_64.ld -o "$W/k.elf" "$W/k.o" build/bm_x86_start.o build/bm_x86_kesme.o build/bm_x86_gorev.o build/bm_x86_uart.o build/bm_x86_yazdir.o build/bm_x86_panik.o
cp "$W/k.elf" "$W/esp/KEMGU/CEKIRDEK.ELF"
if [ "${UEFI_YALNIZ_DERLE:-0}" = 1 ]; then exit 0; fi
python3 tests/qemu/bm_uefi.py "$W"
