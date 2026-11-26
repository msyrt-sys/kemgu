#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source test/calisma_dizini.sh
if ! command -v qemu-system-x86_64 >/dev/null || [[ ! -r /usr/share/OVMF/OVMF_CODE_4M.fd ]]; then
    echo 'ATLANDI (QEMU veya OVMF yok): NIC gidiş dönüşü'
    exit 0
fi
for derleyici in ${NIC_DERLEYICILER:-build/kemgu build/codegen}; do
    w=$(mktemp -d build/nic.XXXXXX)
    trap 'calisma_hatasi "$?" "$w" || exit "$?"' EXIT
    echo "NIC derleyicisi $derleyici ($w)"
    kaynak=test/ornekler/kem_os_x86_kabuk.kem
    if [[ "${1:-e1000}" == saglamlik_ag ]]; then
        kaynak="$w/kem_os_x86_kabuk.kem"
        python3 tests/qemu/ag_tablo_imaj.py "$kaynak"
    fi
    UEFI_CALISMA_DIZINI="$w" UEFI_DERLEYICI="$derleyici" UEFI_YALNIZ_DERLE=1 \
        UEFI_KAYNAK="$kaynak" bash test/bm_x86_uefi_harness.sh
    if [[ "${1:-e1000}" == usb_ag ]]; then
        for program in ping indir paket; do
            python3 tests/qemu/surec_elf.py "$w/$program.elf" --kaynak "test/kullanici/$program.kem" --derleyici "$derleyici"
        done
        python3 tests/qemu/ag_usb_kahin.py "$w"
        python3 tests/qemu/ag_usb_kahin.py "$w" --ham
        python3 tests/qemu/ag_usb_kahin.py "$w" --hub
        python3 tests/qemu/ag_usb_kahin.py "$w" --hub --ham
    elif [[ "${1:-e1000}" == ag_pc || "${1:-e1000}" == iki_konuk ]]; then
        for program in ping indir paket; do
            python3 tests/qemu/surec_elf.py "$w/$program.elf" --kaynak "test/kullanici/$program.kem" --derleyici "$derleyici"
        done
        if [[ "$1" == ag_pc ]]; then
            python3 tests/qemu/ag_pc_matris.py "$w"
        else
            for program in sunucu nc tftp; do
                python3 tests/qemu/surec_elf.py "$w/$program.elf" --kaynak "test/kullanici/$program.kem" --derleyici "$derleyici"
            done
            python3 tests/qemu/surec_elf.py "$w/alicix.elf" --kaynak tests/qemu/ag_tftp_alici.kem --derleyici "$derleyici"
            python3 tests/qemu/ag_iki_konuk.py "$w" x86
            if command -v qemu-system-aarch64 >/dev/null; then
                imaj=build/kem_os.img
                if [[ "$derleyici" == build/codegen ]]; then
                    PYTHONPATH=tests/qemu python3 -c 'from pathlib import Path; from ag_arm_derle import derle; import sys; derle(Path(sys.argv[1]),Path("build/codegen"),Path("build/kem_os_comb.kem"))' "$w/arm-self"
                    imaj="$w/arm-self/kem_os.img"
                fi
                python3 tests/qemu/ag_iki_konuk.py "$w" "$imaj"
            else
                echo 'ATLANDI (qemu-system-aarch64 yok): x86 ile ARM arasındaki TCP aktarımı'
            fi
            mkdir -p "$w/arp-kapali"
            kaynak="$w/arp-kapali/kem_os_x86_kabuk.kem"
            python3 tests/qemu/ag_arp_sessiz.py "$kaynak"
            UEFI_CALISMA_DIZINI="$w/arp-kapali" UEFI_DERLEYICI="$derleyici" UEFI_YALNIZ_DERLE=1 \
                UEFI_KAYNAK="$kaynak" bash test/bm_x86_uefi_harness.sh
            python3 tests/qemu/ag_iki_konuk.py "$w" arp
        fi
    elif [[ "${1:-e1000}" == saglamlik_ag ]]; then
        python3 tests/qemu/surec_elf.py "$w/sunucu.elf" --kaynak test/kullanici/sunucu.kem --derleyici "$derleyici"
        for senaryo in ${NIC_SAGLAMLIK:-cerceve aktarim zaman}; do
            python3 tests/qemu/ag_saglamlik_tel.py "$w" "$senaryo"
        done
    elif [[ "${1:-e1000}" == paket ]]; then
        python3 tests/qemu/surec_elf.py "$w/paket.elf" --kaynak test/kullanici/paket.kem --derleyici "$derleyici"
        python3 tests/qemu/ag_paket_kahin.py "$w"
        python3 tests/qemu/ag_paket_buyuk.py "$w"
    elif [[ "${1:-e1000}" == tftp ]]; then
        for program in tftp teshis; do
            python3 tests/qemu/surec_elf.py "$w/$program.elf" --kaynak "test/kullanici/$program.kem" --derleyici "$derleyici"
        done
        python3 tests/qemu/ag_tftp_kahin.py "$w"
    elif [[ "${1:-e1000}" == ntp ]]; then
        python3 tests/qemu/surec_elf.py "$w/saat.elf" --kaynak test/kullanici/saat.kem --derleyici "$derleyici"
        python3 tests/qemu/ag_ntp_kahin.py "$w"
    elif [[ "${1:-e1000}" == http ]]; then
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
    # [D-771] Başarılı turun çalışma dizini silinir (her tam koşum ~25 GB bırakıp diski dolduruyordu);
    # başarısız turda set -e erken çıkar ve dizin teşhis için kalır.
    rm -rf "$w"
done
