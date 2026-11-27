#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source test/calisma_dizini.sh
if [[ -z "${SUREC_USB_CIKTI:-}" ]] && { ! command -v qemu-system-x86_64 >/dev/null || [[ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ]]; }; then
    echo 'ATLANDI (QEMU veya OVMF yok)'
    exit 0
fi
# [D-772] Bir derleyicinin turu: kullanıcı ELF'leri + çekirdek + kâhin. Dizin turun kendisine aittir.
tur() (
    local derleyici=$1 w ad kaynak senaryo
    w=$(mktemp -d build/surec.XXXXXX)
    calisma_izle "$w"
    mkdir -p "$w/kullanici"
    local -a kaynaklar=(test/kullanici/*.kem)
    if [[ ${SUREC_KAHIN:-} == program15_kahin && ${PROGRAM15_TEK:-} == saat ]]; then
        kaynaklar=(test/kullanici/saat.kem)
    fi
    for kaynak in "${kaynaklar[@]}"; do
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
        return 0
    fi
    echo "Süreç derleyicisi: $derleyici; ölçüm dizini: $w"
    if [[ -n "${SUREC_KAHIN:-}" ]]; then
        SUREC_KAHIN_DERLEYICI="$derleyici" python3 "tests/qemu/$SUREC_KAHIN.py" "$w"
        return 0
    fi
    for senaryo in ${SUREC_SENARYO:-matris}; do
        SUREC_KAHIN_DERLEYICI="$derleyici" python3 tests/qemu/surec_kahin.py "$w" --ilk "${SUREC_ILK:-1}" --tekrar "${SUREC_TEKRAR:-10}" --senaryo "$senaryo"
    done
    # [D-771] Başarılı turun çalışma dizini silinir (her tam koşum ~25 GB bırakıp diski dolduruyordu);
    # başarısız turda set -e erken çıkar ve dizin teşhis için kalır.
)

DERLEYICILER=(${SUREC_DERLEYICILER:-build/kemgu build/codegen})
# USB imaj üretimi tek derleyiciyle tek iş (eski davranış: ilk turdan sonra çıkış).
if [[ -n "${SUREC_USB_CIKTI:-}" ]]; then tur "${DERLEYICILER[0]}"; exit 0; fi
# [D-772] İki derleyicinin turları BİRBİRİNDEN BAĞIMSIZDIR (ayrı mktemp dizini, ayrı QEMU, ayrı seri/QMP
# dosyası) → eşzamanlı koşar; en uzun kapılar (kaynak/kabuk14/v1/cok_surec…) yarıya iner. SUREC_PARALEL=0 sıralı.
# Çıktı karışmasın diye her tur kendi günlüğüne yazar, sonunda sırayla basılır; biri düşerse kapı düşer.
if [[ "${SUREC_PARALEL:-1}" != 1 || ${#DERLEYICILER[@]} -lt 2 ]]; then
    for d in "${DERLEYICILER[@]}"; do tur "$d"; done
    exit 0
fi
g=$(mktemp -d build/surec_gunluk.XXXXXX); pidler=(); kod=0
for d in "${DERLEYICILER[@]}"; do
    ( set -euo pipefail; tur "$d" ) > "$g/${d##*/}.log" 2>&1 & pidler+=($!)
done
for k in "${!pidler[@]}"; do
    d=${DERLEYICILER[$k]}
    if wait "${pidler[$k]}"; then r=0; else r=$?; fi
    echo "── süreç turu: $d (rc=$r)"; cat "$g/${d##*/}.log"
    [[ $r -eq 0 ]] || kod=$r
done
calisma_bitir "$kod" "$g" || exit "$?"
