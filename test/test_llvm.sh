#!/usr/bin/env bash
# =============================================================================
# KEMGU LLVM Backend Entegrasyon Testi
# =============================================================================
#
# Her .kem dosyasi icin:
#   1. kemgu --llvm <dosya> > <ll>
#   2. clang <ll> runtime/kdl_runtime.c -o <exe>
#   3. <exe> calistir, cikis kodu + stdout karsilastir
#
# Beklenen cikis kodu test tablosunda. Beklenen stdout var ise
# test/ornekler/<ad>.out dosyasinda.
#
# PATH: export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH
# =============================================================================

set -u

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
RESET='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

KEMGU="./build/kemgu.exe"
[[ -x "$KEMGU" ]] || KEMGU="./build/kemgu"
if [[ ! -x "$KEMGU" ]]; then
    echo "HATA: $KEMGU bulunamadi. Once mingw32-make calistirin."
    exit 1
fi

RUNTIME="runtime/kdl_runtime.c"
if [[ ! -f "$RUNTIME" ]]; then
    echo "HATA: $RUNTIME bulunamadi."
    exit 1
fi

# Test tablosu — her satir: "dosya beklenen_cikis"
# Beklenen stdout var ise test/ornekler/<ad>.out otomatik kullanilir.
declare -a TESTLER=(
    "test/ornekler/toplama.kem         42"
    "test/ornekler/mutlak.kem          42"
    "test/ornekler/dongu_toplam.kem    45"
    "test/ornekler/fib_recursive.kem   55"
    "test/ornekler/nokta_yapi.kem      42"
    "test/ornekler/dizi_toplam.kem     42"
    "test/ornekler/fizzbuzz_toplam.kem 49"
    "test/ornekler/hello.kem            0"
    "test/ornekler/say.kem              0"
    "test/ornekler/stdlib_karisim.kem   0"
    "test/ornekler/fib_yazdir.kem       0"
    "test/ornekler/dizi_yazdir.kem      0"
    "test/ornekler/tip_alias.kem        0"
    "test/ornekler/kisitli_generic.kem  0"
    "test/ornekler/esles_basit.kem      0"
    "test/ornekler/icin_dongu.kem       0"
)

basarili=0
basarisiz=0
toplam=${#TESTLER[@]}

mkdir -p build/llvm_test

echo "=== KEMGU LLVM + Stdlib Entegrasyon Testi ($toplam test) ==="
echo

for satir in "${TESTLER[@]}"; do
    read -r dosya beklenen <<< "$satir"
    ad="$(basename "$dosya" .kem)"
    exe="build/llvm_test/${ad}.exe"
    ir="build/llvm_test/${ad}.ll"
    out_actual="build/llvm_test/${ad}.out.actual"
    out_expected="test/ornekler/${ad}.out"

    printf "  %-22s -> exit %3s " "$ad" "$beklenen"

    # IR uret
    if ! "$KEMGU" --llvm "$dosya" > "$ir" 2>/dev/null; then
        printf "${RED}HATA${RESET} (kemgu --llvm basarisiz)\n"
        basarisiz=$((basarisiz + 1))
        continue
    fi

    # Derle (runtime ile)
    if ! clang "$ir" "$RUNTIME" -o "$exe" 2>/dev/null; then
        printf "${RED}HATA${RESET} (clang derleme basarisiz)\n"
        basarisiz=$((basarisiz + 1))
        continue
    fi

    # Calistir
    set +e
    "./$exe" > "$out_actual" 2>/dev/null
    actual=$?
    set -e

    # Exit code karsilastir
    if [[ "$actual" != "$beklenen" ]]; then
        printf "${RED}gercek %3d ✗${RESET} (exit)\n" "$actual"
        basarisiz=$((basarisiz + 1))
        continue
    fi

    # Stdout karsilastir (varsa). CRLF/LF farklilarini tolere et.
    if [[ -f "$out_expected" ]]; then
        if diff -q --strip-trailing-cr "$out_actual" "$out_expected" \
                > /dev/null 2>&1; then
            printf "${GREEN}✓${RESET} (stdout esit)\n"
            basarili=$((basarili + 1))
        else
            printf "${RED}✗${RESET} (stdout farkli)\n"
            echo "    --- beklenen ($out_expected) ---"
            sed 's/^/    /' "$out_expected"
            echo "    --- gercek ---"
            sed 's/^/    /' "$out_actual"
            basarisiz=$((basarisiz + 1))
        fi
    else
        printf "${GREEN}✓${RESET}\n"
        basarili=$((basarili + 1))
    fi
done

echo
echo "=== Toplam: $toplam | Basarili: $basarili | Basarisiz: $basarisiz ==="

[[ "$basarisiz" -gt 0 ]] && exit 1
exit 0
