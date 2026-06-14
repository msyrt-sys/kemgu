#!/usr/bin/env bash
# ============================================================================
# codegen_diff_harness.sh — AŞAMA 3 (codegen self-host) SEMANTİK oracle (D-072).
# ----------------------------------------------------------------------------
# KEMGU'da yazılmış codegen'in (selfhost/codegen.kem → build/codegen.exe) ürettiği
# IR'ı, C codegen (build/kemgu.exe --llvm) ile EXIT-KODU eşdeğerliği üzerinden
# doğrular (byte-identik IR DEĞİL — SSA/hoist/format uygulama detayı; bkz. D-072).
#   Her korpus programı: C-codegen→clang→çalıştır→exit  vs  KEMGU-codegen→...→exit.
# Korpus: test/cg_korpus/*.kem (CG milestone'ları büyüdükçe genişler; her dosyada main).
#
# Kullanım: bash test/codegen_diff_harness.sh  (veya make calistir_codegen_diff)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
CODEGEN=${CODEGEN:-build/codegen.exe}
RT=${RT:-build/kdl_runtime.o}
KORPUS=${KORPUS:-test/cg_korpus}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/cgdiff); mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
    echo "ℹ codegen.exe yok (selfhost/codegen.kem henüz CG1'de — D-072 ADIM-0)."
    echo "   Oracle hazır; codegen.exe derlenince diff koşar. (test_tumu'ya CG1'de bağlanır.)"
    exit 0
fi

pass=0; fail=0
for f in "$KORPUS"/*.kem; do
    [ -f "$f" ] || continue
    # Win11'de .exe yeniden-yazımı dosya-kilidi yarışına girer → dosya-başı benzersiz ad.
    b=$(basename "$f" .kem)
    # C codegen → exit (oracle)
    "$KEMGU" --llvm "$f" > "$TMP/$b.c.ll" 2>/dev/null
    clang -x ir "$TMP/$b.c.ll" -x none "$RT" -o "$TMP/$b.c.exe" 2>/dev/null
    "$TMP/$b.c.exe" >/dev/null 2>&1; coracle=$?
    # KEMGU codegen → exit (aday)
    "$CODEGEN" "$f" > "$TMP/$b.k.ll" 2>/dev/null
    if ! clang -x ir "$TMP/$b.k.ll" -x none "$RT" -o "$TMP/$b.k.exe" 2>/dev/null; then
        echo "  🔴 $(basename "$f") — KEMGU IR link edilemedi"; fail=$((fail+1)); continue
    fi
    "$TMP/$b.k.exe" >/dev/null 2>&1; kaday=$?
    if [ "$coracle" -eq "$kaday" ]; then
        echo "  ✅ $(basename "$f") (exit=$coracle)"; pass=$((pass+1))
    else
        echo "  🔴 $(basename "$f") — C-codegen exit=$coracle ≠ KEMGU-codegen exit=$kaday"
        fail=$((fail+1))
    fi
done
echo "=== codegen semantik eşdeğerlik: $pass/$((pass+fail)) korpus ==="
[ "$fail" -eq 0 ]
