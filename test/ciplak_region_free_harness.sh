#!/usr/bin/env bash
# D-256: çıplak işlev "sıfır region-symbol" invaryantı — IR-İÇERİK parite gate'i.
#
# NEDEN AYRI HARNESS: codegen_diff exit-KODU karşılaştırır; çıplak allocator host'ta
# runtime'a linklendiği için region-prologue sızıntısı exit'te GÖRÜNMEZ (host maskeler).
# Kusur IR/link seviyesindedir (bare-metal circularity). Bu gate IR-İÇERİĞİ denetler:
# çıplak fn gövdesinde @kdl_bolge_olustur / @kdl_global_bolge_al / @kdl_bolge_serbest = 0,
# HEM C-codegen (build/kemgu.exe) HEM self-host (build/codegen.exe) için.
#
# Audit bulgusu (Bulgu #1): self-host IKEN/İÇİN handler'ı çıplak fn'de per-iterasyon
# ρ_iter (@kdl_bolge_olustur) emit ediyordu → çıplak+döngü invaryantı ihlali. Fix:
# codegen.kem ρ_iter create sitelerine ciplak_aktif guard'ı.
set -u
cd "$(dirname "$0")/.."

KEMGU=build/kemgu.exe
SELF=build/codegen.exe
TMP=$(mktemp -d 2>/dev/null || echo /c/tmp/ciplak_rf_$$)
mkdir -p "$TMP"
gecti=0; kaldi=0

# self-host binary'yi HER ZAMAN güncel codegen.kem'den üret (stale binary = yanlış PASS).
"$KEMGU" --llvm selfhost/codegen.kem > "$TMP/self.ll" 2>/dev/null
clang -x ir "$TMP/self.ll" -x none build/kdl_runtime.o -o "$SELF" -Wno-override-module 2>/dev/null
if [ ! -x "$SELF" ]; then echo "  ❌ self-host codegen.exe üretilemedi"; exit 1; fi

# çıplak fn gövdesindeki region-symbol sayısını dön (fn TANIMI → sonraki '}' arası).
# NOT: "define" ile bağla — aksi halde başka fn'deki `call @fn(` çağrısı da eşleşip
# yanlış bloğa (örn. çağıran main'in prologue'una) kayar.
region_say() {
    local ll="$1" imza="$2"
    awk -v im="$imza" '$0 ~ ("^define.*" im) {f=1} f{print} /^}/{if(f)f=0}' "$ll" \
        | grep -cE 'kdl_bolge_olustur|kdl_global_bolge_al|kdl_bolge_serbest'
}

kontrol() {
    local ad="$1" beklenen="$2" gercek="$3"
    if [ "$gercek" = "$beklenen" ]; then
        echo "  ✅ $ad ($gercek)"; gecti=$((gecti+1))
    else
        echo "  ❌ $ad (beklenen=$beklenen, gerçek=$gercek)"; kaldi=$((kaldi+1))
    fi
}

# --- Test 1: çıplak + DÖNGÜ → 0 region-symbol (HER İKİ derleyici) ---
cat > "$TMP/dongu.kem" <<'EOF'
küresel değişken g_top: tam64 = 0;
çıplak işlev topla(n: tam32) -> tam64 {
    değişken i: tam32 = 0;
    iken i < n { g_top = g_top + 1; i = i + 1; }
    ver g_top;
}
işlev main() -> tam32 { güvensiz { topla(5); ver 42; } }
EOF
"$KEMGU" --llvm "$TMP/dongu.kem" > "$TMP/c_dongu.ll" 2>/dev/null
"$SELF"  --llvm "$TMP/dongu.kem" > "$TMP/s_dongu.ll" 2>/dev/null
kontrol "çıplak+iken: C-codegen region-symbol"    0 "$(region_say "$TMP/c_dongu.ll" '@topla[(]')"
kontrol "çıplak+iken: self-host region-symbol"     0 "$(region_say "$TMP/s_dongu.ll" '@topla[(]')"

# --- Test 2: çıplak + İÇİN (for) döngü → 0 region-symbol (self-host) ---
cat > "$TMP/icin.kem" <<'EOF'
küresel değişken g_s: tam64 = 0;
çıplak işlev bas(d: Dizi<tam32>) -> tam64 {
    için e içinde d { g_s = g_s + 1; }
    ver g_s;
}
işlev main() -> tam32 { güvensiz { değişken d: Dizi<tam32> = [1,2,3]; bas(d); ver 42; } }
EOF
"$SELF" --llvm "$TMP/icin.kem" > "$TMP/s_icin.ll" 2>/dev/null
kontrol "çıplak+için: self-host region-symbol"     0 "$(region_say "$TMP/s_icin.ll" '@bas[(]')"

# --- Test 3: NORMAL (çıplak-olmayan) döngü → self-host ρ_iter KORUNUR (F4.3 regresyon) ---
cat > "$TMP/normal.kem" <<'EOF'
işlev say(n: tam32) -> tam32 {
    değişken i: tam32 = 0;
    iken i < n { i = i + 1; }
    ver i;
}
işlev main() -> tam32 { ver say(5); }
EOF
"$SELF" --llvm "$TMP/normal.kem" > "$TMP/s_normal.ll" 2>/dev/null
nr=$(region_say "$TMP/s_normal.ll" '@say[(]')
if [ "$nr" -ge 1 ]; then
    echo "  ✅ normal döngü: self-host ρ_iter korundu ($nr)"; gecti=$((gecti+1))
else
    echo "  ❌ normal döngü: self-host ρ_iter KAYBOLDU ($nr) — F4.3 regresyonu"; kaldi=$((kaldi+1))
fi

rm -rf "$TMP"
echo "=== çıplak region-free IR parite: $gecti/$((gecti+kaldi)) ==="
[ "$kaldi" -eq 0 ]
