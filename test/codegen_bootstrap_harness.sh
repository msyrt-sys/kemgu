#!/usr/bin/env bash
# ============================================================================
# codegen_bootstrap_harness.sh — AŞAMA 5 SELF-HOST BOOTSTRAP FIXPOINT (D-084/085).
# ----------------------------------------------------------------------------
# KEMGU-yazılı codegen (selfhost/codegen.kem) gerçekten self-host eden bir
# derleyici mi? Üç bağımsız kanıt:
#   1) LEXER bootstrap: codegen.exe ile derlenen lexer == C-codegen lexer (çıktı).
#   2) PARSER bootstrap: codegen.exe ile derlenen parser == C-codegen parser (--ast).
#   3) CODEGEN FIXPOINT: codegen.exe codegen.kem'i derler → codegen2.exe;
#      codegen2.exe codegen.kem'i derler → stage2 IR == stage1 IR (byte-identik).
#      = derleyici kendini sabit-nokta olarak yeniden üretiyor.
#
# Kullanım: bash test/codegen_bootstrap_harness.sh  (veya make calistir_codegen_bootstrap)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/cgboot); mkdir -p "$TMP"
hata=0

link() {  # $1=ll $2=exe ; Win11 .exe yeniden-yazım yarışına 3 deneme
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    return 1
}
# C-codegen ile bir self-host aracı derle (referans).
ref_derle() {  # $1=kaynak $2=exe-out
    "$KEMGU" --llvm "$1" > "$TMP/ref.ll" 2>/dev/null && link "$TMP/ref.ll" "$2"
}
# KEMGU-codegen (codegen.exe) ile bir self-host aracı derle.
cg_derle() {   # $1=kaynak $2=exe-out
    "$TMP/codegen.exe" "$1" > "$TMP/cg.ll" 2>/dev/null && link "$TMP/cg.ll" "$2"
}
# İki aracın aynı korpusta çıktısını diff'le.
ciktilari_diff() {  # $1=ref-exe $2=cg-exe $3=etiket
    local a=0 f=0
    for src in selfhost/*.kem test/ornekler/*.kem; do
        [ -f "$src" ] || continue
        "$1" "$src" > "$TMP/r.txt" 2>/dev/null
        "$2" "$src" > "$TMP/c.txt" 2>/dev/null
        if diff -q "$TMP/r.txt" "$TMP/c.txt" >/dev/null 2>&1; then a=$((a+1));
        else echo "    🔴 $3: $(basename "$src")"; f=$((f+1)); fi
    done
    echo "  $3: $a birebir, $f fark"
    [ "$f" -ne 0 ] && hata=1
}

# 0) C-codegen ile codegen.exe'yi derle (stage0 — KEMGU codegen'in C-build'i)
"$KEMGU" --llvm selfhost/codegen.kem > "$TMP/stage0.ll" 2>/dev/null
if ! link "$TMP/stage0.ll" "$TMP/codegen.exe"; then echo "🔴 codegen.exe derlenemedi"; exit 1; fi

# 1) LEXER bootstrap
ref_derle selfhost/lexer.kem "$TMP/lexer_ref.exe" || { echo "🔴 ref lexer"; exit 1; }
cg_derle  selfhost/lexer.kem "$TMP/lexer_cg.exe"  || { echo "🔴 cg lexer link"; exit 1; }
ciktilari_diff "$TMP/lexer_ref.exe" "$TMP/lexer_cg.exe" "LEXER"

# 2) PARSER bootstrap
ref_derle selfhost/parser.kem "$TMP/parser_ref.exe" || { echo "🔴 ref parser"; exit 1; }
cg_derle  selfhost/parser.kem "$TMP/parser_cg.exe"  || { echo "🔴 cg parser link"; exit 1; }
ciktilari_diff "$TMP/parser_ref.exe" "$TMP/parser_cg.exe" "PARSER"

# 2b) CHECKER bootstrap (--checkdump)
ref_derle selfhost/checker.kem "$TMP/checker_ref.exe" || { echo "🔴 ref checker"; exit 1; }
cg_derle  selfhost/checker.kem "$TMP/checker_cg.exe"  || { echo "🔴 cg checker link"; exit 1; }
ciktilari_diff "$TMP/checker_ref.exe" "$TMP/checker_cg.exe" "CHECKER"

# 3) CODEGEN self-compile FIXPOINT
#    stage1 = codegen.exe (KEMGU-codegen) codegen.kem'i derler
#    stage2 = codegen2.exe (kendisi codegen.exe ile derlendi) codegen.kem'i derler
#    İkisi de KEMGU-codegen çıktısı → byte-identik olmalı (sabit-nokta).
"$TMP/codegen.exe" selfhost/codegen.kem > "$TMP/stage1.ll" 2>/dev/null
if ! link "$TMP/stage1.ll" "$TMP/codegen2.exe"; then echo "🔴 codegen2 link"; exit 1; fi
"$TMP/codegen2.exe" selfhost/codegen.kem > "$TMP/stage2.ll" 2>/dev/null
if diff -q "$TMP/stage1.ll" "$TMP/stage2.ll" >/dev/null 2>&1; then
    echo "  CODEGEN FIXPOINT: stage1 IR == stage2 IR ($(wc -l < "$TMP/stage1.ll") satır) — BİREBİR ✓"
else
    echo "  🔴 CODEGEN FIXPOINT farkı:"; diff "$TMP/stage1.ll" "$TMP/stage2.ll" | head -4; hata=1
fi

echo "=== SELF-HOST BOOTSTRAP: $([ "$hata" -eq 0 ] && echo 'FIXPOINT ✓ (lexer+parser+checker+codegen)' || echo 'BAŞARISIZ') ==="
[ "$hata" -eq 0 ]
