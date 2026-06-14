#!/usr/bin/env bash
# ============================================================================
# parser_bootstrap_harness.sh — SELF-HOST parser BOOTSTRAP kanıtı (P6).
# ----------------------------------------------------------------------------
# KEMGU'da yazılmış parser'ı (selfhost/parser.kem) GERÇEK dünya KEMGU korpusunun
# TAMAMINA karşı C parser `--ast` oracle'ı (D-043) ile sıfır-diff'ler. Kapsam:
#   - selfhost/parser.kem'in KENDİSİ (SELF-PARSING — bootstrap'ın asıl ispatı)
#   - tüm stdlib/ drivers/ kütüphane/ test/**/*.kem
# Hariç: build/ (üretilmiş), test/lex_korpus/ (lexer), test/ornekler/eski/
#   (deprecated — C parser da hata verir; `tip` alias v1'de YOK).
#
# Kullanım: bash test/parser_bootstrap_harness.sh (veya make calistir_parser_bootstrap)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/parseboot); mkdir -p "$TMP"

if ! "$KEMGU" --llvm selfhost/parser.kem > "$TMP/p.ll" 2>/dev/null; then
    echo "🔴 KEMGU-parser --llvm üretemedi"; exit 1
fi
if ! clang -x ir "$TMP/p.ll" -x none "$RT" -o "$TMP/kemparse.exe" 2>/dev/null; then
    echo "🔴 KEMGU-parser link edilemedi"; exit 1
fi

pass=0; fail=0; fail_list=""
while IFS= read -r f; do
    [ -f "$f" ] || continue
    "$KEMGU" --ast "$f" 2>/dev/null > "$TMP/oracle.txt"
    "$TMP/kemparse.exe" "$f" > "$TMP/aday.txt" 2>/dev/null
    if diff -q "$TMP/oracle.txt" "$TMP/aday.txt" >/dev/null 2>&1; then
        pass=$((pass+1))
    else
        fail=$((fail+1)); fail_list="$fail_list $f"
        if [ "$fail" -le 5 ]; then
            echo "🔴 FARK: $f"; diff "$TMP/oracle.txt" "$TMP/aday.txt" | head -8
        fi
    fi
done < <(find . -name '*.kem' -not -path './build/*' -not -path './.git/*' \
              -not -path './test/lex_korpus/*' -not -path './test/ornekler/eski/*' | sort)

echo "=== parser bootstrap sıfır-diff: $pass/$((pass+fail)) gerçek .kem (self-parse dahil) ==="
[ "$fail" -ne 0 ] && echo "FARKLI:$fail_list"
[ "$fail" -eq 0 ]
