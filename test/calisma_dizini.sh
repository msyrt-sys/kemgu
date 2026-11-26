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
