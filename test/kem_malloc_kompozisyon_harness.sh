#!/usr/bin/env bash
# F3 (D-259): SAF-.kem çıplak+küresel allocator KOMPOZİSYON KANITI gate'i.
# ============================================================================
# İki bootstrap-primitifi (küresel değişken + çıplak işlev) bir arada ÇALIŞAN
# allocator verir + circularity'yi kırar. HER İKİ derleyici (C + self-host):
#   (A) IR-KANIT: kem_malloc'ta @kdl_bolge_olustur/@kdl_global_bolge_al/@malloc
#       self-call = 0 → tahsis kendini tetiklemez (sonsuz recursion imkansız).
#   (B) KOMPOZİSYON: C-harness .kem çıplak malloc'u 2× çağırır → 2 FARKLI adres,
#       yazılabilir (inttoptr+deref), bitişik → exit 42 + "A!=B EVET".
set -u
cd "$(dirname "$0")/.."

KEMGU=build/kemgu.exe
SELF=build/codegen.exe
KEM=test/ornekler/kem_malloc.kem
HARNESS=test/kem_malloc_kompozisyon.c
TMP=$(mktemp -d 2>/dev/null || echo /c/tmp/kemmalloc_$$)
mkdir -p "$TMP"
gecti=0; kaldi=0

# self-host binary'yi güncel codegen.kem'den üret (stale = yanlış PASS).
"$KEMGU" --llvm selfhost/codegen.kem > "$TMP/self.ll" 2>/dev/null
clang -x ir "$TMP/self.ll" -x none build/kdl_runtime.o -o "$SELF" -Wno-override-module 2>/dev/null
if [ ! -x "$SELF" ]; then echo "  ❌ self-host codegen.exe üretilemedi"; exit 1; fi

kontrol() {
    local ad="$1" beklenen="$2" gercek="$3"
    if [ "$gercek" = "$beklenen" ]; then echo "  ✅ $ad ($gercek)"; gecti=$((gecti+1))
    else echo "  ❌ $ad (beklenen=$beklenen, gerçek=$gercek)"; kaldi=$((kaldi+1)); fi
}

# --- Her iki codegen için: IR-circularity + kompozisyon ---
for cg in C SELF; do
    if [ "$cg" = "C" ]; then BIN="$KEMGU"; else BIN="$SELF"; fi
    "$BIN" --llvm "$KEM" > "$TMP/$cg.ll" 2>/dev/null

    # (A) IR-KANIT: circularity sembolleri = 0 (kem_malloc gövdesi + tüm modül).
    circ=$(grep -cE 'call .*@kdl_bolge_olustur|call .*@kdl_global_bolge_al|call .*@malloc' "$TMP/$cg.ll")
    kontrol "$cg: kem_malloc IR circularity-sembol"  0 "$circ"

    # (B) KOMPOZİSYON: .o link + harness + çalıştır → exit 42.
    clang -c -x ir "$TMP/$cg.ll" -o "$TMP/$cg.o" -Wno-override-module 2>/dev/null
    clang "$HARNESS" "$TMP/$cg.o" -o "$TMP/$cg.exe" 2>/dev/null
    "$TMP/$cg.exe" > "$TMP/$cg.out" 2>&1
    rc=$?
    kontrol "$cg: kompozisyon exit"  42 "$rc"
    # "A!=B EVET" satırı (2 farklı adres) doğrula.
    if grep -q 'A!=B EVET' "$TMP/$cg.out"; then
        echo "  ✅ $cg: $(grep 'ALLOC:' "$TMP/$cg.out")"; gecti=$((gecti+1))
    else
        echo "  ❌ $cg: 2-adres kanıtı yok"; cat "$TMP/$cg.out"; kaldi=$((kaldi+1))
    fi
done

rm -rf "$TMP"
echo "=== kem_malloc kompozisyon (circularity kırıldı): $gecti/$((gecti+kaldi)) ==="
[ "$kaldi" -eq 0 ]
