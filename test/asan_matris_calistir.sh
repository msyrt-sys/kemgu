#!/usr/bin/env bash
# ============================================================================
# asan_matris_calistir.sh — KEMGU codegen bellek güvenliği MATRİSİ (kalıcı)
# ----------------------------------------------------------------------------
# D-029 (yapı alan-adı çözümü) + D-030 (dizi_olustur element_byte heap-overflow)
# hatalarının yaşadığı EKSENLERİ sınır-noktalarında zorlayan temsili program seti.
# Her program kendini doğrular (başarı = exit 42). Hem SANITIZER'SIZ (değer
# doğruluğu) hem ASan/UBSan altında (bellek güvenliği) çalıştırılır.
#
# Eksenler (test/asan_matris/m*.kem):
#   - Eleman tipi: tam8/16/32 (4-byte) vs tam64/metin/&T (8-byte) sınırı
#   - Kapasite: olustur(N)+ekle yarı-kapasite üstü (D-030) + realloc büyüme
#   - İşlemler: dizi_ekle / dizi_al / dizi_yaz (D-025 in-place) / dizi_boyut
#   - Yapı: karışık eleman-byte koleksiyonlar · aynı alan adı farklı tip · &Yapi param
#
# Kullanım:  bash test/asan_matris_calistir.sh   (veya: make calistir_asan_matris)
# Çıkış: 0 = tüm matris hem değer-doğru (42) hem ASan/UBSan-temiz; 1 = ihlal.
# ============================================================================
set -u
KEMGU=${KEMGU:-./build/kemgu.exe}
RT_OBJ="build/kdl_runtime.o build/kdl_runtime_mmio.o"
RT_SRC="runtime/kdl_runtime.c runtime/kdl_runtime_mmio.c"
DIR="test/asan_matris"
TMP=$(mktemp -d 2>/dev/null || echo /tmp/asan_matris); mkdir -p "$TMP"
pass=0; fail=0

for f in "$DIR"/m*.kem; do
    b=$(basename "$f" .kem)
    if ! "$KEMGU" --check "$f" >/dev/null 2>&1; then
        echo "  ✗ $b: --check HATA"; fail=$((fail+1)); continue
    fi
    if ! "$KEMGU" --llvm "$f" > "$TMP/m.ll" 2>/dev/null; then
        echo "  ✗ $b: --llvm HATA"; fail=$((fail+1)); continue
    fi
    # 1) Sanitizer'sız: değer doğruluğu (exit 42)
    clang -x ir "$TMP/m.ll" -x none $RT_OBJ -o "$TMP/n.exe" 2>/dev/null
    "$TMP/n.exe" >/dev/null 2>&1; nrc=$?
    # 2) ASan/UBSan: bellek güvenliği (exit 42 + 0 ihlal)
    clang -fsanitize=address,undefined -x ir "$TMP/m.ll" -x none $RT_SRC -o "$TMP/a.exe" 2>/dev/null
    aout=$("$TMP/a.exe" 2>&1); arc=$?
    viol=$(echo "$aout" | grep -ciE "AddressSanitizer|runtime error|SUMMARY:.*[Ss]anitizer")
    if [ "$nrc" = "42" ] && [ "$arc" = "42" ] && [ "$viol" = "0" ]; then
        echo "  ✓ $b"; pass=$((pass+1))
    else
        echo "  ✗ $b: normal=$nrc asan=$arc ihlal=$viol"
        echo "$aout" | grep -iE "ERROR|overflow|misalign|SUMMARY" | head -2
        fail=$((fail+1))
    fi
done
echo "=== ASan matris: $pass/$((pass+fail)) (değer-doğru + ASan/UBSan-temiz) ==="
[ "$fail" -eq 0 ]
