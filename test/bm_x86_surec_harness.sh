#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ -z "${SUREC_USB_CIKTI:-}" ]] && { ! command -v qemu-system-x86_64 >/dev/null || [[ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]]; }; then
    echo 'ATLANDI (QEMU veya OVMF yok)'
    exit 0
fi
for derleyici in ${SUREC_DERLEYICILER:-build/kemgu build/codegen}; do
    w=$(mktemp -d build/surec.XXXXXX)
    mkdir -p "$w/kullanici"
    for kaynak in test/kullanici/*.kem; do
        ad="${kaynak##*/}"
        python3 tests/qemu/surec_elf.py "$w/kullanici/${ad%.kem}.elf" \
            --kaynak "$kaynak" --derleyici "$derleyici"
    done
    if [[ " ${SUREC_SENARYO:-} " == *" buyukelf "* ]]; then
        python3 tests/qemu/surec_elf.py "$w/kullanici/eski.elf" --eski \
            --kaynak test/kullanici/merhaba.kem --derleyici "$derleyici"
    fi
    UEFI_CALISMA_DIZINI="$w" UEFI_YALNIZ_DERLE=1 UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem \
        UEFI_DERLEYICI="$derleyici" bash test/bm_x86_uefi_harness.sh
    if [[ -n "${SUREC_USB_CIKTI:-}" ]]; then
        python3 tests/qemu/usb14_imaj.py "$w" "$SUREC_USB_CIKTI"
        exit 0
    fi
    echo "Süreç derleyicisi: $derleyici; ölçüm dizini: $w"
    if [[ -n "${SUREC_KAHIN:-}" ]]; then
        SUREC_KAHIN_DERLEYICI="$derleyici" python3 "tests/qemu/$SUREC_KAHIN.py" "$w"
        continue
    fi
    for senaryo in ${SUREC_SENARYO:-matris}; do
        python3 tests/qemu/surec_kahin.py "$w" --tekrar "${SUREC_TEKRAR:-10}" --senaryo "$senaryo"
    done
done
