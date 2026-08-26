#!/usr/bin/env bash
# ============================================================================
# lexer_bootstrap_harness.sh — M6 SELF-HOST BOOTSTRAP doğruluk kanıtı.
# ----------------------------------------------------------------------------
# KEMGU'da yazılmış lexer'ı (selfhost/lexer.kem) GERÇEK dünya KEMGU korpusunun
# TAMAMINA karşı C lexer (oracle) ile satır-satır diff'ler. Kapsam:
#   - selfhost/lexer.kem'in KENDİSİ (SELF-LEXING — bootstrap'ın asıl ispatı)
#   - tüm stdlib/, drivers/, kütüphane/, test/**/*.kem (build/ üretilmiş → hariç)
# Tek bir fark = lexer eksiği. Sıfır fark = KEMGU-lexer C lexer'ı TAM İKAME EDER.
#
# Kullanım: bash test/lexer_bootstrap_harness.sh  (veya make calistir_lexer_bootstrap)
# Çıkış: 0 = tüm korpus sıfır-diff; 1 = en az bir fark.
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/lexboot); mkdir -p "$TMP"

# KEMGU-lexer'ı derle
if ! "$KEMGU" --llvm selfhost/lexer.kem > "$TMP/lex.ll" 2>/dev/null; then
    echo "🔴 KEMGU-lexer --llvm üretemedi"; exit 1
fi
if ! clang -x ir "$TMP/lex.ll" -x none "$RT" -o "$TMP/kemlex.exe" 2>/dev/null; then
    echo "🔴 KEMGU-lexer link edilemedi"; exit 1
fi

pass=0; fail=0; fail_list=""
# Gerçek korpus: build/ (üretilmiş temp) hariç tüm .kem
while IFS= read -r f; do
    [ -f "$f" ] || continue
    "$KEMGU" --token "$f" 2>/dev/null > "$TMP/oracle.txt"
    "$TMP/kemlex.exe" "$f" > "$TMP/aday.txt" 2>/dev/null
    if diff -q "$TMP/oracle.txt" "$TMP/aday.txt" >/dev/null 2>&1; then
        pass=$((pass+1))
    else
        fail=$((fail+1)); fail_list="$fail_list $f"
        if [ "$fail" -le 5 ]; then
            echo "🔴 FARK: $f"
            diff "$TMP/oracle.txt" "$TMP/aday.txt" | head -10
        fi
    fi
done < <(find . -name '*.kem' -not -path './build/*' -not -path './.git/*' | sort)

echo "=== lexer bootstrap sıfır-diff: $pass/$((pass+fail)) gerçek .kem dosyası ==="
if [ "$fail" -ne 0 ]; then
    echo "FARKLI dosyalar:$fail_list"
fi
[ "$fail" -eq 0 ]
