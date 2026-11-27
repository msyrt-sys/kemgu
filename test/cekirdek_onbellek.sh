# x86 çekirdeği için içerik adresli önbellek. Birleşik kaynağın tüm yamaları çağrıdan önce biter.
cekirdek_urunleri=(ham.ll k.ll k.o k.elf)
cekirdek_ir=(clang -target x86_64-unknown-none -ffreestanding -nostdlib -mgeneral-regs-only -O2 -Wno-override-module)

cekirdek_anahtari() (
    set -o pipefail
    local clang_surumu lld_surumu moduller
    clang_surumu=$(clang --version) || return 1
    lld_surumu=$(ld.lld --version) || return 1
    moduller=$(python3 test/cekirdek_modulleri.py "$2") || return 1
    {
        printf '%s\n' "$moduller"
        printf '%s\n' "${cekirdek_ir[*]}" "${clang_surumu%%$'\n'*}" "${lld_surumu%%$'\n'*}"
        sha256sum -- "$@" | cut -d' ' -f1
    } | sha256sum | cut -c1-32
)

cekirdek_kopyala() {
    local kaynak=$1 hedef=$2 urun
    local -a yollar=()
    for urun in "${cekirdek_urunleri[@]}"; do yollar+=("$kaynak/$urun"); done
    cp -- "${yollar[@]}" "$hedef/"
}

cekirdek_onbellek() {
    local w=$1 derleyici=$2 baglayici=$3
    shift 3
    local suzgec=test/strip_defined_declares.awk kok anahtar kayit ara
    kok=${KEMGU_CEKIRDEK_ONBELLEK_KOK:-build/cekirdek_onbellek}
    if [[ ${KEMGU_CEKIRDEK_ONBELLEK:-1} == 1 ]]; then
        anahtar=$(cekirdek_anahtari "$derleyici" "$w/comb.kem" "$suzgec" "$baglayici" "$@") || return 1
        kayit="$kok/$anahtar"
        if [[ -d $kayit ]] && cekirdek_kopyala "$kayit" "$w" 2>/dev/null; then
            touch -c -- "$kayit" || return 1
            echo "CEKIRDEK_ONBELLEK isabet ${anahtar:0:12} ($derleyici)"
            return 0
        fi
    fi
    "$derleyici" --llvm --mimari x86_64-none "$w/comb.kem" > "$w/ham.ll" || return 1
    awk -f "$suzgec" "$w/ham.ll" > "$w/k.ll" || return 1
    "${cekirdek_ir[@]}" -x ir "$w/k.ll" -c -o "$w/k.o" || return 1
    ld.lld -m elf_x86_64 -T "$baglayici" -o "$w/k.elf" "$w/k.o" "$@" || return 1
    if [[ ${KEMGU_CEKIRDEK_ONBELLEK:-1} == 1 ]]; then
        mkdir -p -- "$kok" || return 1
        ara=$(mktemp -d "$kok/.yaz.XXXXXX") || return 1
        if ! cekirdek_kopyala "$w" "$ara"; then rm -rf -- "$ara"; return 1; fi
        # Atomik yayın. Rakip aynı kaydı tamamladıysa iki ürün de aynı girdilerden gelir.
        if ! mv -T -- "$ara" "$kayit" 2>/dev/null; then
            rm -rf -- "$ara"
            [[ -d $kayit ]] || return 1
        fi
        # Yalnız geçerli anahtarlı dizinleri buda. Kopyalarken budanan isabet yeniden derlenir.
        local sira=0 yol
        while IFS= read -r yol; do
            [[ ${yol##*/} =~ ^[0-9a-f]{32}$ ]] || continue
            (( ++sira <= 64 )) || rm -rf -- "$yol" || return 1
        done < <(ls -1dt -- "$kok"/* 2>/dev/null)
        echo "CEKIRDEK_ONBELLEK derlendi ${anahtar:0:12} ($derleyici)"
    fi
}

bm_calisma_baslat() {
    if [[ -n ${BM_CALISMA_DIZINI:-} ]]; then
        W=$BM_CALISMA_DIZINI
        mkdir -p -- "$W" || return 1
    else
        W=$(calisma_ac "$1") || return 1
        calisma_izle "$W"
    fi
}
