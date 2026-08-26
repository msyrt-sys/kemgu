#!/usr/bin/env bash
# ============================================================================
# parser_diff_harness.sh — SELF-HOST parser doğruluk kanıtı (--ast sıfır-diff).
# ----------------------------------------------------------------------------
# KEMGU'da yazılmış parser'ın (selfhost/parser.kem) ürettiği DÜZ AST dump'ını,
# C parser'ının `--ast` oracle'ına (D-043) karşı satır-satır diff'ler.
#   Format: <derinlik>\t<TIP_ADI>\t<deger>\t<satır>\t<sütün>
# Aynı format → fark = parser hatası.
#
# Korpus: test/parse_korpus/*.kem (milestone'lar büyüdükçe genişler).
# P1 = ifade (Pratt) — `işlev f() -> tip { ver İFADE; }` sarmalayıcı.
#
# Kullanım: bash test/parser_diff_harness.sh  (veya make calistir_parser_diff)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/parsediff); mkdir -p "$TMP"

if ! "$KEMGU" --llvm selfhost/parser.kem > "$TMP/p.ll" 2>/dev/null; then
    echo "🔴 KEMGU-parser --llvm üretemedi"; exit 1
fi
if ! clang -x ir "$TMP/p.ll" -x none "$RT" -o "$TMP/kemparse.exe" 2>/dev/null; then
    echo "🔴 KEMGU-parser link edilemedi"; exit 1
fi

pass=0; fail=0
for f in test/parse_korpus/*.kem; do
    [ -f "$f" ] || continue
    "$KEMGU" --ast "$f" 2>/dev/null > "$TMP/oracle.txt"
    "$TMP/kemparse.exe" "$f" > "$TMP/aday.txt" 2>/dev/null
    if diff -q "$TMP/oracle.txt" "$TMP/aday.txt" >/dev/null 2>&1; then
        echo "  ✅ $(basename "$f")"; pass=$((pass+1))
    else
        echo "  🔴 $(basename "$f") — C parser (oracle) vs KEMGU parser farkı:"
        diff "$TMP/oracle.txt" "$TMP/aday.txt" | head -10
        fail=$((fail+1))
    fi
done
echo "=== parser --ast sıfır-diff: $pass/$((pass+fail)) korpus ==="
[ "$fail" -eq 0 ]
