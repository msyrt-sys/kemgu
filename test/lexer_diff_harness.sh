#!/usr/bin/env bash
# ============================================================================
# lexer_diff_harness.sh — SELF-HOST lexer doğruluk kanıtı (sıfır-diff oracle).
# ----------------------------------------------------------------------------
# KEMGU'da yazılmış lexer'ın (selfhost/lexer.kem) çıktısını, C lexer'ının
# `--token` dump'ına (oracle) karşı SATIR-SATIR diff'ler. Aynı format
# (D-035: <TIP>\t<satır>\t<sütün>\t<offset>\t<uzunluk>) → fark = lexer hatası.
#
# Korpus: test/lex_korpus/*.kem (milestone'lar büyüdükçe genişler; eski dosyalar
# regresyon olarak kalır). M1 = ASCII alt-kümesi.
#
# Kullanım: bash test/lexer_diff_harness.sh   (veya make calistir_lexer_diff)
# Çıkış: 0 = tüm korpus sıfır-diff; 1 = en az bir fark.
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
RT=${RT:-build/kdl_runtime.o}
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/lexdiff.XXXXXX" 2>/dev/null || echo "build/lexdiff.$$")
mkdir -p "$TMP"

# KEMGU-lexer'ı derle
if ! "$KEMGU" --llvm selfhost/lexer.kem > "$TMP/lex.ll" 2>/dev/null; then
    echo "🔴 KEMGU-lexer --llvm üretemedi"; exit 1
fi
if ! clang -x ir "$TMP/lex.ll" -x none "$RT" -o "$TMP/kemlex.exe" 2>/dev/null; then
    echo "🔴 KEMGU-lexer link edilemedi"; exit 1
fi

pass=0; fail=0
for f in test/lex_korpus/*.kem; do
    [ -f "$f" ] || continue
    "$KEMGU" --token "$f" 2>/dev/null > "$TMP/oracle.txt"
    "$TMP/kemlex.exe" "$f" > "$TMP/aday.txt" 2>/dev/null
    if diff -q "$TMP/oracle.txt" "$TMP/aday.txt" >/dev/null 2>&1; then
        echo "  ✅ $(basename "$f")"; pass=$((pass+1))
    else
        echo "  🔴 $(basename "$f") — C-lexer (oracle) vs KEMGU-lexer farkı:"
        diff "$TMP/oracle.txt" "$TMP/aday.txt" | head -8
        fail=$((fail+1))
    fi
done
echo "=== lexer sıfır-diff: $pass/$((pass+fail)) korpus ==="
[ "$fail" -eq 0 ]
