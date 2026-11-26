#!/usr/bin/env bash
# Gerçek harness girişlerinde hem anahtar geçersizleşmesini hem de ürün içeriğini ölç.
set -euo pipefail
cd "$(dirname "$0")/.."
source test/calisma_dizini.sh
source test/cekirdek_onbellek.sh
D=${KEMGU:-build/kemgu}
T=$(mktemp -d build/obkapi.XXXXXX)
calisma_izle "$T"
export KEMGU_CEKIRDEK_ONBELLEK_KOK="$T/ob" UEFI_YALNIZ_DERLE=1 BM_YALNIZ_DERLE=1
cp "$D" "$T/derleyici"
printf '\0' >> "$T/derleyici"
cp build/bm_x86_start.o "$T/baslangic.o"
printf '\0' >> "$T/baslangic.o"

derle() {
    local ad=$1 tur=$2 beklenen=$3 kaynak=$4 derleyici=$5 nesne=$6 yama=${7:-0}
    local w="$T/$ad/$tur"
    mkdir -p "$w"
    local etkin=1 sonuc=kapali
    if [[ $beklenen == kapali ]]; then etkin=0; fi
    if ! env UEFI_KAYNAK="$kaynak" EKRAN_KAYNAK="$kaynak" PANIK_KAYNAK="$kaynak" \
        UEFI_DERLEYICI="$derleyici" BM_DERLEYICI="$derleyici" BM_BASLANGIC_NESNESI="$nesne" \
        UEFI_CALISMA_DIZINI="$w" BM_CALISMA_DIZINI="$w" UEFI_AHCI_MSI_YOK="$yama" \
        KEMGU_CEKIRDEK_ONBELLEK="$etkin" bash "test/bm_x86_${ad}_harness.sh" > "$w/log" 2>&1; then
        cat "$w/log" >&2
        return 2
    fi
    if grep -q '^CEKIRDEK_ONBELLEK isabet ' "$w/log"; then sonuc=isabet
    elif grep -q '^CEKIRDEK_ONBELLEK derlendi ' "$w/log"; then sonuc=derlendi; fi
    echo "$ad/$tur beklenen=$beklenen gelen=$sonuc"
    [[ $sonuc == "$beklenen" ]]
}
esit() {
    local urun
    for urun in "${cekirdek_urunleri[@]}"; do cmp "$1/$urun" "$2/$urun" || return 1; done
    echo "Bayt eşitliği: $1 ve $2 (${cekirdek_urunleri[*]})"
}
nesne=build/bm_x86_start.o
for ad in uefi panik ekran; do
    mkdir -p "$T/$ad/kaynak"
    if [[ $ad == uefi ]]; then kaynak=test/ornekler/kem_os_x86_kabuk.kem
    else kaynak=test/ornekler/kem_os_x86_${ad}.kem; fi
    degisen="$T/$ad/kaynak/${kaynak##*/}"
    if [[ $ad == ekran ]]; then
        # Ekran kaynağı için gerçek koordinat yaması; yalnız-derle yolunda ölçülür.
        awk '{sub(/f.yukseklik \+ 4;/,"f.yukseklik + 5;"); print}' "$kaynak" > "$degisen"
        grep -Fq 'f.yukseklik + 5;' "$degisen"
        once=$kaynak; kaynak=$degisen; degisen=$once
    else
        { cat "$kaynak"; echo '// Önbellek kaynak deneyi'; } > "$degisen"
    fi
    derle "$ad" ilk derlendi "$kaynak" "$D" "$nesne"
    derle "$ad" tekrar isabet "$kaynak" "$D" "$nesne"
    derle "$ad" kapali kapali "$kaynak" "$D" "$nesne"
    esit "$T/$ad/tekrar" "$T/$ad/kapali"
    derle "$ad" kaynak-farki derlendi "$degisen" "$D" "$nesne"
    derle "$ad" derleyici-farki derlendi "$kaynak" "$T/derleyici" "$nesne"
    derle "$ad" nesne-farki derlendi "$kaynak" "$D" "$T/baslangic.o"
    if [[ $ad == ekran ]]; then
        # Derleyici önce birleşik kaynağın dizinine bakar; yalnız bu turun modülünü değiştir.
        modul=drivers/ekran/psf.kem
        mkdir -p "$T/$ad/modul-farki/${modul%/*}"
        { cat "$modul"; echo '// İçe aktarılan modül değişti'; } > "$T/$ad/modul-farki/$modul"
        derle "$ad" modul-farki derlendi "$kaynak" "$D" "$nesne"
    fi
    if [[ $ad == uefi ]]; then
        derle "$ad" msi-yamali derlendi "$kaynak" "$D" "$nesne" 1
        derle "$ad" msi-tekrar isabet "$kaynak" "$D" "$nesne" 1
        derle "$ad" msi-kapali kapali "$kaynak" "$D" "$nesne" 1
        esit "$T/$ad/msi-tekrar" "$T/$ad/msi-kapali"
        if cmp -s "$T/$ad/ilk/k.elf" "$T/$ad/msi-tekrar/k.elf"; then
            echo 'MSI yaması ELF ürününde görünmedi' >&2; exit 2
        fi
    fi
done
