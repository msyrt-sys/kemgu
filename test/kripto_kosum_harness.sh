#!/usr/bin/env bash
# ============================================================================
# kripto_kosum_harness.sh — stdlib/kripto BİLİNEN-CEVAP KOŞUM kapısı.
# ----------------------------------------------------------------------------
# `calistir_kripto_check`ten FARKI: o yalnız `--check` (tip kontrolü) yapar.
# Bu kapı bundle'ı DERLER, ÇALIŞTIRIR ve NIST/RFC vektörleriyle karşılaştırır.
#
# Neden gerekli: stdlib/kripto tamamen `sabitsüre<dtamN>` üzerine kurulu.
# İmzasızlık kaybı `>>`yi `ashr`e çevirir ve SHA-256/ChaCha20 rotasyonları
# yüksek bit set olan girdilerde SESSİZCE yanlış sonuç verir. Tip kontrolü
# bunu göremez — yalnız koşum görebilir. (Bu kapı kurulduğu gün 2 gerçek
# kusur buldu: `sabitsüre` imzasızlık kaybı + SHA-256 W dizisinin 62 öğe
# olması, ki ikincisi çalışma-anı dizi-sınır paniğiydi.)
#
# EXIT KODU = bit maskesi:  +1 KONTROL (sabit tablolar) · +2 ChaCha20 QR
#                           +4 SHA-256("abc")           · tam geçiş = 7
# KONTROL biti kapının kendisini doğrular: 0 ise kapı bozuk (derleme/bağlama),
# 1 ise kapı SAĞLAM ve kripto çekirdeği yanlış.
#
# Kullanım: bash test/kripto_kosum_harness.sh  (veya make calistir_kripto_kosum)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
RT=${RT:-build/kdl_runtime.o}
TEST=${TEST:-test/stdlib/test_kripto_kosum.kem}
# Paralel koşumda birbirini ezmesin (D-297 dersi): PID ile benzersiz yol.
TMP="build/_kripto_kosum_$$"
mkdir -p "$TMP"
temizle() { rm -rf "$TMP"; }
trap temizle EXIT

BEKLENEN=7

if [ ! -x "$KEMGU" ]; then echo "FAIL: $KEMGU yok"; exit 1; fi
if [ ! -f "$RT" ]; then echo "FAIL: $RT yok (make build/kdl_runtime.o)"; exit 1; fi

# Import sistemi yok → bundle (calistir_kripto_check ile aynı yaklaşım).
cat stdlib/kripto.kem stdlib/kripto/*.kem "$TEST" > "$TMP/bundle.kem"

if ! "$KEMGU" --check "$TMP/bundle.kem" > "$TMP/check.log" 2>&1; then
    echo "FAIL: kripto koşum bundle --check geçmedi"; tail -20 "$TMP/check.log"; exit 1
fi
if ! "$KEMGU" --llvm "$TMP/bundle.kem" > "$TMP/bundle.ll" 2>"$TMP/gen.log"; then
    echo "FAIL: --llvm IR üretimi"; tail -20 "$TMP/gen.log"; exit 1
fi
if ! clang -x ir "$TMP/bundle.ll" -x none "$RT" -o "$TMP/bundle.exe" 2>"$TMP/link.log"; then
    echo "FAIL: clang bağlama"; grep -v "overriding the module target" "$TMP/link.log" | head -20; exit 1
fi

CIKTI=$("$TMP/bundle.exe" 2>&1); RC=$?

# Win11: taze .exe ilk exec'te Defender taramasında 127 verebilir (ortamsal).
# Gerçek panik de 127 döner AMA stdout'a "PANIK:" yazar → ikisini AYIR.
if [ "$RC" -eq 127 ] && [ -z "$CIKTI" ]; then
    sleep 2; CIKTI=$("$TMP/bundle.exe" 2>&1); RC=$?
fi

echo "=== stdlib/kripto bilinen-cevap koşumu ==="
[ -n "$CIKTI" ] && echo "  çalışma-anı çıktısı: $CIKTI"
bit() { if [ $(( RC & $1 )) -ne 0 ]; then echo "✅"; else echo "❌"; fi; }
echo "  $(bit 1) KONTROL  — sabit tablolar (K[0], K[63], H0[0])"
echo "  $(bit 2) ChaCha20 quarter-round — RFC 8439 §2.1.1"
echo "  $(bit 4) SHA-256(\"abc\")        — NIST FIPS 180-4 App. B.1"

if [ "$RC" -eq "$BEKLENEN" ]; then
    echo "=== kripto koşum kapısı: 3/3 vektör GEÇTİ ✓ ==="
    exit 0
fi

if [ $(( RC & 1 )) -eq 0 ]; then
    echo "=== KAPI BOZUK: KONTROL biti düştü (exit=$RC) — derleme/bağlama sorunu, kripto değil ==="
else
    echo "=== KIRMIZI: kripto çekirdeği YANLIŞ (exit=$RC, beklenen=$BEKLENEN) ==="
    echo "    KONTROL geçtiği için kapı sağlam; sapma kripto hesabında."
fi
exit 1
