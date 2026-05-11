#!/usr/bin/env bash
# =============================================================================
# KEMGU LLVM Backend Entegrasyon Testi
# =============================================================================
#
# Her .kem dosyasi icin:
#   1. kemgu --llvm <dosya> | clang -x ir - -o <exe>
#   2. <exe> calistir
#   3. cikis kodunu beklenen ile karsilastir
#
# Beklenen cikis kodu (test_ornek_listesi) tablosunda.
#
# PATH'te clang ve mingw32-make olmali:
#   export PATH=/c/msys64/clang64/bin:/c/msys64/ucrt64/bin:$PATH
# =============================================================================

set -u

# Renkler
GREEN='\033[0;32m'
RED='\033[0;31m'
RESET='\033[0m'

# Calisma dizini: repo koku (script'in dizini test/, bir ust)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

KEMGU="./build/kemgu.exe"
if [[ ! -x "$KEMGU" ]]; then
    KEMGU="./build/kemgu"
fi

if [[ ! -x "$KEMGU" ]]; then
    echo "HATA: $KEMGU bulunamadi. Once mingw32-make calistirin."
    exit 1
fi

# Test tablosu: "dosya beklenen_cikis"
declare -a TESTLER=(
    "test/ornekler/toplama.kem        42"
    "test/ornekler/mutlak.kem         42"
    "test/ornekler/dongu_toplam.kem   45"
    "test/ornekler/fib_recursive.kem  55"
    "test/ornekler/nokta_yapi.kem     42"
    "test/ornekler/dizi_toplam.kem    42"
    "test/ornekler/fizzbuzz_toplam.kem 49"
)

basarili=0
basarisiz=0
toplam=${#TESTLER[@]}

mkdir -p build/llvm_test

echo "=== KEMGU LLVM Entegrasyon Testi ($toplam test) ==="
echo

for satir in "${TESTLER[@]}"; do
    # Whitespace ile ayir
    read -r dosya beklenen <<< "$satir"
    ad="$(basename "$dosya" .kem)"
    exe="build/llvm_test/${ad}.exe"
    ir="build/llvm_test/${ad}.ll"

    printf "  %-32s -> beklenen %3s ... " "$ad" "$beklenen"

    # IR uret
    if ! "$KEMGU" --llvm "$dosya" > "$ir" 2>/dev/null; then
        printf "${RED}HATA${RESET} (kemgu --llvm basarisiz)\n"
        basarisiz=$((basarisiz + 1))
        continue
    fi

    # Derle
    if ! clang -x ir "$ir" -o "$exe" 2>/dev/null; then
        printf "${RED}HATA${RESET} (clang derleme basarisiz)\n"
        basarisiz=$((basarisiz + 1))
        continue
    fi

    # Calistir
    set +e
    "./$exe"
    actual=$?
    set -e

    if [[ "$actual" == "$beklenen" ]]; then
        printf "${GREEN}gercek %3d ✓${RESET}\n" "$actual"
        basarili=$((basarili + 1))
    else
        printf "${RED}gercek %3d ✗${RESET}\n" "$actual"
        basarisiz=$((basarisiz + 1))
    fi
done

echo
echo "=== Toplam: $toplam | Basarili: $basarili | Basarisiz: $basarisiz ==="

if [[ "$basarisiz" -gt 0 ]]; then
    exit 1
fi
exit 0
