#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v qemu-system-x86_64 >/dev/null || [[ ! -r /usr/share/OVMF/OVMF_CODE_4M.fd ]]; then
    echo 'ATLANDI (QEMU veya OVMF yok): NIC gidiş dönüşü'
    exit 0
fi
for derleyici in build/kemgu build/codegen; do
    w=$(mktemp -d build/nic.XXXXXX)
    echo "NIC derleyicisi $derleyici ($w)"
    UEFI_CALISMA_DIZINI="$w" UEFI_DERLEYICI="$derleyici" UEFI_YALNIZ_DERLE=1 \
        UEFI_KAYNAK=test/ornekler/kem_os_x86_kabuk.kem bash test/bm_x86_uefi_harness.sh
    if [[ "${1:-e1000}" == http ]]; then
        for program in indir sunucu; do
            python3 tests/qemu/surec_elf.py "$w/$program.elf" --kaynak "test/kullanici/$program.kem" --derleyici "$derleyici"
        done
        python3 tests/qemu/ag_http_kahin.py "$w"
    elif [[ "${1:-e1000}" == arac ]]; then
        for program in ag cozumle nc agakim echo; do
            python3 tests/qemu/surec_elf.py "$w/$program.elf" --kaynak "test/kullanici/$program.kem" --derleyici "$derleyici"
        done
        python3 tests/qemu/ag_arac_kahin.py "$w"
        python3 tests/qemu/ag_arac_es.py "$w"
    elif [[ "${1:-e1000}" == soket ]]; then
        python3 tests/qemu/surec_abi.py
        python3 tests/qemu/surec_elf.py "$w/soketdeney.elf" --kaynak test/kullanici/soketdeney.kem --derleyici "$derleyici"
        python3 tests/qemu/ag_soket_kahin.py "$w"
    elif [[ "${1:-e1000}" == secenek ]]; then
        python3 tests/qemu/ag_tcp_secenek.py "$w" secenek
    elif [[ "${1:-e1000}" == verim ]]; then
        python3 tests/qemu/ag_tcp_secenek.py "$w" olcekli
        python3 tests/qemu/ag_tcp_secenek.py "$w" olceksiz
    elif [[ "${1:-e1000}" == kayip ]]; then
        python3 tests/qemu/ag_tcp_kayip.py "$w"
    elif [[ "${1:-e1000}" == tcp ]]; then
        python3 tests/qemu/ag_tcp_kahin.py "$w"
    elif [[ "${1:-e1000}" == parca ]]; then
        python3 tests/qemu/ag_parca_kahin.py "$w"
    elif [[ "${1:-e1000}" == udp ]]; then
        python3 tests/qemu/ag_udp_kahin.py "$w"
        python3 tests/qemu/ag_udp_kahin.py "$w" --slirp
    elif [[ "${1:-e1000}" == l3 ]]; then
        python3 tests/qemu/surec_elf.py "$w/ping.elf" --kaynak test/kullanici/ping.kem --derleyici "$derleyici"
        python3 tests/qemu/ag_l3_kahin.py "$w"
        python3 tests/qemu/ag_l3_kahin.py "$w" --slirp
    elif [[ "${1:-e1000}" == matris ]]; then
        for model in e1000 e1000e igb virtio-net-pci; do
            python3 tests/qemu/e1000_kahin.py "$w" --model "$model"
            ram=512M
            if [[ "$model" == e1000e ]]; then ram=4G; fi
            python3 tests/qemu/e1000_kahin.py "$w" --model "$model" --ram "$ram"
        done
        python3 tests/qemu/e1000_kahin.py "$w" --model rtl8139
    else
        python3 tests/qemu/e1000_kahin.py "$w"
        python3 tests/qemu/e1000_kahin.py "$w" --kopru
    fi
done
