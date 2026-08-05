#!/usr/bin/env bash
# ============================================================================
# checker_diff_harness.sh — SELF-HOST tip denetleyici doğruluk kanıtı (Aşama 2).
# ----------------------------------------------------------------------------
# KEMGU'da yazılmış checker'ın (selfhost/checker.kem) ürettiği DÜZ hata dump'ını,
# C checker'ın `--checkdump` oracle'ına (D-051) karşı diff'ler.
#   Format: <KOD>\t<satır>\t<sütün> (traversal sırası) veya "OK"
# Korpus: test/check_korpus/*.kem (TC milestone'ları büyüdükçe genişler).
# TC1 = temel kapsam/ad çözümü (T002). Korpus TEMIZ parse eder (yalnız T/L/M).
#
# Kullanım: bash test/checker_diff_harness.sh  (veya make calistir_checker_diff)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/checkdiff); mkdir -p "$TMP"

if ! "$KEMGU" --llvm selfhost/checker.kem > "$TMP/c.ll" 2>/dev/null; then
    echo "🔴 KEMGU-checker --llvm üretemedi"; exit 1
fi
if ! clang -x ir "$TMP/c.ll" -x none "$RT" -o "$TMP/kemcheck.exe" 2>/dev/null; then
    echo "🔴 KEMGU-checker link edilemedi"; exit 1
fi

# D-361: modül fikstürleri de kapıya dâhil (çapraz-dosya import yüzeyi).
# MUAF (0) — liste BOŞ. Modül yüzeyi D-361/362/363'te tamamen kapandı:
#   ana_secili/ana_belirsiz → D-361 (seçili import + T042)
#   ana_kutuphane           → D-362 (runtime UTF-8 yol + T040 + T016)
#   ana_gizli               → D-363 (T041, private-by-default)
# Yeni bir muafiyet eklemek gerekiyorsa GEREKÇESİ DECISIONS_LOG'a yazılmalı.
MUAF=""
muaf_mi() { case " $MUAF " in *" $1 "*) return 0;; esac; return 1; }

pass=0; fail=0; muaf=0
for f in test/check_korpus/*.kem test/moduller/*.kem; do
    [ -f "$f" ] || continue
    if muaf_mi "$(basename "$f")"; then muaf=$((muaf+1)); continue; fi
    "$KEMGU" --checkdump "$f" 2>/dev/null > "$TMP/oracle.txt"
    "$TMP/kemcheck.exe" "$f" > "$TMP/aday.txt" 2>/dev/null
    if diff -q "$TMP/oracle.txt" "$TMP/aday.txt" >/dev/null 2>&1; then
        echo "  ✅ $(basename "$f")"; pass=$((pass+1))
    else
        echo "  🔴 $(basename "$f") — C checker (oracle) vs KEMGU checker farkı:"
        diff "$TMP/oracle.txt" "$TMP/aday.txt" | head -10
        fail=$((fail+1))
    fi
done
echo "=== checker --checkdump sıfır-diff: $pass/$((pass+fail)) korpus ($muaf muaf) ==="
[ "$fail" -eq 0 ]
