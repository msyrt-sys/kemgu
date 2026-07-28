#!/usr/bin/env bash
# ============================================================================
# ct_bariyer_parite_harness.sh — sabitsüre SPEKÜLASYON BARİYERİ kapısı.
# ----------------------------------------------------------------------------
# NEDEN AYRI BİR KAPI: bariyer programın SONUCUNU değiştirmez. Düşse de her
# test doğru cevabı verir → codegen_diff, kripto_kosum ve diğer exit-kodu
# tabanlı kapıların HİÇBİRİ görmez; `sabitsüre`nin vaat ettiği constant-time
# sertliği sessizce kaybolur (Spectre v1). Bu kapı IR/ASSEMBLY İÇERİĞİNE bakar.
# (D-256'nın "çıplak işlev sıfır region-symbol" kapısıyla aynı gerekçe.)
#
# İKİ BOYUT:
#   A) x86_64 C↔self-host PARİTESİ — sayılar eşit VE > 0.
#      (self-host x86_64 triple'a sabit, `--mimari` desteği YOK → arm64 boyutu
#       yalnız C tarafında ölçülür. Bilinçli: self-host hedef seçmiyor.)
#   B) arm64 DOĞRULUĞU — x86 intrinsic kalıntısı YOK, csdb VAR, ve IR
#      aarch64'e GERÇEKTEN derleniyor. (B) olmadan D-346 regresyonu sessizce
#      geri gelirdi: x86 intrinsic'i aarch64 backend'inde "Cannot select" ile
#      ÇÖKÜYOR, yani sabitsüre kullanan hiçbir program ARM64'e derlenemiyordu.
#      ASSEMBLY'de `csdb` SAYMAK şart — `llvm.aarch64.sb` denemesi derleniyor
#      ama `bl llvm.aarch64.sb` (FONKSİYON ÇAĞRISI) üretiyordu; IR'a bakmak
#      bunu yakalamaz, assembly'ye bakmak yakalar.
#
# Kullanım: bash test/ct_bariyer_parite_harness.sh (veya make calistir_ct_bariyer)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
RT=${RT:-build/kdl_runtime.o}
# CODEGEN dışarıdan VERİLMEDİYSE her zaman TAZE derlenir. Mevcut
# build/codegen.exe'ye düşmek BAYAT-OBJE tuzağıdır (yaşandı: "self=0 sapma").
CODEGEN=${CODEGEN:-}
TMP="build/_ctbar_$$"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

[ -x "$KEMGU" ] || { echo "FAIL: $KEMGU yok"; exit 1; }
[ -f "$RT" ]    || { echo "FAIL: $RT yok"; exit 1; }

if [ -z "$CODEGEN" ]; then
    "$KEMGU" --llvm selfhost/codegen.kem > "$TMP/cg.ll" 2>/dev/null || {
        echo "FAIL: selfhost/codegen.kem IR"; exit 1; }
    clang -x ir "$TMP/cg.ll" -x none "$RT" -o "$TMP/codegen.exe" 2>/dev/null || {
        echo "FAIL: codegen.exe baglama"; exit 1; }
    CODEGEN="$TMP/codegen.exe"
fi

# Korpus: sabitsüre kullanan programlar. Kripto bundle GERÇEK yük (onlarca
# sabitsüre_olustur/ifşa) — tek dosyalık korpus yanlış güven verirdi.
cat stdlib/kripto.kem stdlib/kripto/*.kem test/stdlib/test_kripto_kosum.kem \
    > "$TMP/kripto_bundle.kem" 2>/dev/null
KORPUS="test/cg_korpus/cg_isaretsiz_sabitsure.kem $TMP/kripto_bundle.kem"

# NOT: `grep -c` eşleşme yoksa "0" BASAR ve exit 1 döner → `|| echo 0`
# eklemek "0\n0" üretir ve [ ] karşılaştırmasını bozar (yaşandı).
say() { local n; n=$(grep -c "$1" "$2" 2>/dev/null); echo "${n:-0}"; }

hata=0
echo "=== sabitsüre CT bariyeri — (A) x86_64 C↔self paritesi ==="
for src in $KORPUS; do
    [ -f "$src" ] || continue
    ad=$(basename "$src")
    "$KEMGU"   --llvm "$src" > "$TMP/c.ll" 2>/dev/null
    "$CODEGEN" --llvm "$src" > "$TMP/s.ll" 2>/dev/null
    c=$(say "llvm.x86.sse2.lfence" "$TMP/c.ll")
    s=$(say "llvm.x86.sse2.lfence" "$TMP/s.ll")
    if [ "$c" -eq "$s" ] && [ "$c" -gt 0 ]; then
        echo "  ✅ $ad — lfence C=$c self=$s"
    elif [ "$c" -eq 0 ]; then
        echo "  ❌ $ad — C=0: korpus sabitsüre içermiyor (kapı vacuous olurdu)"; hata=1
    else
        echo "  ❌ $ad — C=$c self=$s SAPMA (self-host CT bariyeri eksik/fazla)"; hata=1
    fi
done

echo "=== (B) arm64 doğruluğu (C tarafı; self-host x86_64'e sabit) ==="
for src in $KORPUS; do
    [ -f "$src" ] || continue
    ad=$(basename "$src")
    "$KEMGU" --llvm "$src" > "$TMP/x.ll" 2>/dev/null
    bek=$(say "llvm.x86.sse2.lfence" "$TMP/x.ll")
    "$KEMGU" --mimari arm64 --llvm "$src" > "$TMP/a.ll" 2>/dev/null
    hint=$(say "llvm.aarch64.hint" "$TMP/a.ll")
    kalinti=$(say "llvm.x86" "$TMP/a.ll")
    # IR sayısı doğru olsa bile intrinsic SEÇİLEMEYEBİLİR → assembly'de doğrula.
    if clang -target aarch64-unknown-none -x ir "$TMP/a.ll" -S -o "$TMP/a.s" 2>/dev/null; then
        csdb=$(say "csdb" "$TMP/a.s")
    else
        csdb=-1
    fi
    if [ "$csdb" -lt 0 ]; then
        echo "  ❌ $ad — aarch64 DERLENMEDİ (Cannot select? x86 intrinsic sızmış olabilir)"; hata=1
    elif [ "$kalinti" -ne 0 ]; then
        echo "  ❌ $ad — arm64 IR'ında $kalinti adet x86 intrinsic KALINTISI"; hata=1
    elif [ "$hint" -eq "$bek" ] && [ "$csdb" -eq "$bek" ] && [ "$bek" -gt 0 ]; then
        echo "  ✅ $ad — hint=$hint, assembly csdb=$csdb (x86 lfence=$bek ile birebir)"
    else
        echo "  ❌ $ad — hint=$hint csdb=$csdb beklenen=$bek SAPMA"; hata=1
    fi
done

echo "=== (C) arm64 + --ct-bariyer sb (FEAT_SB opt-in) ==="
for src in $KORPUS; do
    [ -f "$src" ] || continue
    ad=$(basename "$src")
    "$KEMGU" --llvm "$src" > "$TMP/x.ll" 2>/dev/null
    bek=$(say "llvm.x86.sse2.lfence" "$TMP/x.ll")
    "$KEMGU" --mimari arm64 --ct-bariyer sb --llvm "$src" > "$TMP/sb.ll" 2>/dev/null
    # ASSEMBLY/OBJE seviyesinde doğrula: `llvm.aarch64.sb` intrinsic'i derleniyor
    # AMA `bl llvm.aarch64.sb` (fonksiyon çağrısı) üretiyordu — IR sayımı bunu
    # YAKALAMAZ. Bu yüzden gerçek komut + encoding + `bl` SIZINTISI ölçülür.
    if clang -target aarch64-unknown-none -x ir "$TMP/sb.ll" -c -o "$TMP/sb.o" 2>/dev/null; then
        llvm-objdump -d "$TMP/sb.o" > "$TMP/sb.dis" 2>/dev/null
        enc=$(say "d50330ff" "$TMP/sb.dis")          # SB encoding
        blsz=$(say "bl.*sb" "$TMP/sb.dis")           # çağrı sızıntısı (OLMAMALI)
    else
        enc=-1; blsz=0
    fi
    if [ "$enc" -lt 0 ]; then
        echo "  ❌ $ad — aarch64 DERLENMEDİ (--ct-bariyer sb)"; hata=1
    elif [ "$blsz" -ne 0 ]; then
        echo "  ❌ $ad — $blsz adet 'bl ...sb' ÇAĞRI SIZINTISI (bariyer değil!)"; hata=1
    elif [ "$enc" -eq "$bek" ] && [ "$bek" -gt 0 ]; then
        echo "  ✅ $ad — gerçek sb komutu ×$enc (encoding d50330ff), bl sızıntısı 0"
    else
        echo "  ❌ $ad — sb encoding sayısı $enc, beklenen $bek SAPMA"; hata=1
    fi
done

if [ "$hata" -eq 0 ]; then
    echo "=== CT bariyeri: x86_64 paritesi + arm64 csdb + arm64 sb GEÇTİ ✓ ==="
    exit 0
fi
echo "=== CT bariyeri kapısı BAŞARISIZ ==="
exit 1
