#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source test/calisma_dizini.sh
if ! command -v qemu-system-x86_64 >/dev/null || [[ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]]; then
    echo 'ATLANDI (QEMU veya OVMF yok)'
    exit 0
fi
for c in build/kemgu build/codegen; do
    for yoklama in 0 1; do
        w=$(mktemp -d build/msi.XXXXXX)
        trap 'calisma_hatasi "$?" "$w" || exit "$?"' EXIT
        UEFI_CALISMA_DIZINI="$w" UEFI_DERLEYICI="$c" UEFI_YALNIZ_DERLE=1 UEFI_AHCI_MSI_YOK="$yoklama" \
            UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem bash test/bm_x86_uefi_harness.sh
        secenek=()
        if [[ $yoklama == 1 ]]; then secenek=(--yoklama); fi
        echo "MSI derleyicisi $c, yoklama $yoklama"
        python3 tests/qemu/msi_kahin.py "$w" "${secenek[@]}"
        # [D-771] Başarılı turun çalışma dizini silinir; başarısızlıkta set -e erken çıkar, dizin kalır.
        rm -rf "$w"
    done
done
