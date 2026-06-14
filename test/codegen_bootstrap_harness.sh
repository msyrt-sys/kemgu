#!/usr/bin/env bash
# ============================================================================
# codegen_bootstrap_harness.sh — AŞAMA 5 (bootstrap) LEXER fixpoint kanıtı (D-084).
# ----------------------------------------------------------------------------
# KEMGU-yazılı codegen (selfhost/codegen.kem → codegen.exe) self-host LEXER'ı
# (selfhost/lexer.kem) derler; çıktısı C-codegen-built lexer ile BYTE-IDENTİK mi?
# Yani: codegen.kem'in ürettiği makine kodu, C derleyiciyle aynı davranan bir lexer
# veriyor mu — codegen self-host'unun uçtan-uca doğruluğu.
#
#   codegen.exe lexer.kem | clang → lexer_cg.exe   (KEMGU-codegen)
#   kemgu --llvm lexer.kem | clang → lexer_ref.exe (C-codegen, oracle)
#   her korpus dosyası: lexer_cg <f>  vs  lexer_ref <f>  → diff
#
# Kullanım: bash test/codegen_bootstrap_harness.sh  (veya make calistir_codegen_bootstrap)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/cgboot); mkdir -p "$TMP"

link() {  # $1=ll $2=exe ; Win11 .exe yeniden-yazım yarışına 3 deneme
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    return 1
}

# 1) KEMGU codegen → codegen.exe
"$KEMGU" --llvm selfhost/codegen.kem > "$TMP/codegen.ll" 2>/dev/null
if ! link "$TMP/codegen.ll" "$TMP/codegen.exe"; then echo "🔴 codegen.exe derlenemedi"; exit 1; fi
# 2) codegen.exe ile lexer.kem → lexer_cg.exe (KEMGU-codegen-built)
"$TMP/codegen.exe" selfhost/lexer.kem > "$TMP/lexer_cg.ll" 2>/dev/null
if ! link "$TMP/lexer_cg.ll" "$TMP/lexer_cg.exe"; then echo "🔴 KEMGU-codegen lexer link edilemedi"; exit 1; fi
# 3) C codegen ile lexer.kem → lexer_ref.exe (oracle)
"$KEMGU" --llvm selfhost/lexer.kem > "$TMP/lexer_ref.ll" 2>/dev/null
if ! link "$TMP/lexer_ref.ll" "$TMP/lexer_ref.exe"; then echo "🔴 C-codegen lexer link edilemedi"; exit 1; fi

ayni=0; fark=0
for f in selfhost/*.kem test/ornekler/*.kem; do
    [ -f "$f" ] || continue
    "$TMP/lexer_ref.exe" "$f" > "$TMP/r.txt" 2>/dev/null
    "$TMP/lexer_cg.exe"  "$f" > "$TMP/c.txt" 2>/dev/null
    if diff -q "$TMP/r.txt" "$TMP/c.txt" >/dev/null 2>&1; then
        ayni=$((ayni+1))
    else
        echo "  🔴 $(basename "$f") — KEMGU-codegen lexer ≠ C-codegen lexer (ref=$(wc -l<"$TMP/r.txt") cg=$(wc -l<"$TMP/c.txt"))"
        fark=$((fark+1))
    fi
done
echo "=== LEXER bootstrap (KEMGU-codegen vs C-codegen): $ayni birebir, $fark fark ==="
[ "$fark" -eq 0 ]
