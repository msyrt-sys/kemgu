# Harness kendi oluşturduğu dizini başarıda siler. Hata incelemesinde aynı yolu kullan.
calisma_ac() {
    local kok=$1 yol
    if command -v mktemp >/dev/null 2>&1; then
        mktemp -d "$kok.XXXXXX"
    else
        # Eski MSYS ortamında da yalnız bu çağrının oluşturduğu dizini sahiplen.
        yol="$kok.${BASHPID:-$$}"
        mkdir -- "$yol" || return 1
        printf '%s\n' "$yol"
    fi
}

calisma_bitir() {
    local kod=$1 dizin=$2
    if (( kod != 0 )); then
        printf 'Tanı dizini: %s\n' "$dizin" >&2
        return "$kod"
    fi
    rm -rf -- "$dizin"
}

calisma_izle() {
    local yol
    printf -v yol '%q' "$1"
    trap "calisma_bitir \$? $yol || exit \$?" EXIT
}

# Döngülerde her turun başarılı çıkışını çağıran yönetir.
calisma_hatasi() {
    local kod=$1 dizin=$2
    if (( kod != 0 )); then printf 'Tanı dizini: %s\n' "$dizin" >&2; fi
    return "$kod"
}

# 127 dışında her çıkış kesin sonuçtur. MSYS ilk çalıştırmayı geçici olarak
# reddedebilir. Dört saniyelik sınırda en son denemenin sonucunu koru.
calisma_127_dene() {
    local son=$((SECONDS+4)) rc
    while :; do
        if "$@"; then return 0; else rc=$?; fi
        if ((rc != 127 || SECONDS >= son)); then return "$rc"; fi
        sleep .1
    done
}

calisma_kosul_bekle() {
    local ad=$1 son=$((SECONDS+$2)) rc
    shift 2
    while :; do
        if "$@"; then return 0; else rc=$?; fi
        if ((SECONDS >= son)); then
            printf '%s: bekleme sınırı doldu, son durum rc=%s\n' "$ad" "$rc" >&2
            return 1
        fi
        sleep .05
    done
}

# Her yürütme denemesinde günlüğü sıfırla; eski başarısız deneme çıktısını taşımama.
calisma_cikti_yaz() {
    local yol=$1
    shift
    "$@" > "$yol" 2>&1
}
