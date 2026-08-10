#!/usr/bin/env bash
# ============================================================================
# baremetal_diff_harness.sh — BARE-METAL / ARM64 codegen kapısı (D-418).
# ----------------------------------------------------------------------------
# NEDEN AYRI BİR KAPI: `runtime/*.kem` (OS/sürücü kodu) host'ta LİNKLENMEZ —
# aarch64 sysreg/MMIO içerir. Exit-kodu karşılaştırması imkânsız, bu yüzden
# davranışsal kapıların HEPSİ bu yüzeyi atlıyordu ve **self-host `--mimari`
# bayrağını hiç tanımadığı için bare-metal kodu DERLEYEMİYORDU BİLE** — bunu
# hiçbir kapı görmüyordu (ölçüldü: 0 define üretiyordu).
#
# Linkleyemediğimiz için DAVRANIŞ değil YAPI karşılaştırılır: emit edilen
# `define` kümesi (ad + DÖNÜŞ TİPİ) C oracle ile birebir mi. Bu, eksik işlevi,
# yanlış adı ve yanlış dönüş tipini yakalar — D-418'de üçü de gerçekten çıktı
# (void işlevler `i32` olarak yayılıyordu).
#
# ⚠ Dönüş tipini KESMEYİN: `sed 's/(.*//'` imzayı atar ama `define void @f` ile
# `define i32 @f` ayrımını KORUR — kusur tam da oradaydı.
#
# Kullanım: bash test/baremetal_diff_harness.sh (veya make calistir_baremetal_diff)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
CODEGEN=${CODEGEN:-build/codegen.exe}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/bmdiff); mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
    echo "ℹ codegen.exe yok — kapı atlandı (önce build/codegen.exe kurulmalı)."
    exit 0
fi

pass=0; fail=0; atla=0
for f in runtime/*.kem; do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)

    # Oracle IR üretemiyorsa (bu kapının işi olmayan bir sebep) atla.
    "$KEMGU" --llvm --mimari aarch64 "$f" > "$TMP/c.ll" 2>/dev/null || { atla=$((atla+1)); continue; }
    head -1 "$TMP/c.ll" 2>/dev/null | grep -q "hata\[" && { atla=$((atla+1)); continue; }
    [ -s "$TMP/c.ll" ] || { atla=$((atla+1)); continue; }

    "$CODEGEN" --llvm --mimari aarch64 "$f" > "$TMP/s.ll" 2>/dev/null || {
        echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue; }

    # Hedef üçlüsü de karşılaştırılır: --mimari yok sayılırsa burası yakalar.
    ct=$(grep -m1 "target triple" "$TMP/c.ll")
    st=$(grep -m1 "target triple" "$TMP/s.ll")
    if [ "$ct" != "$st" ]; then
        echo "  🔴 $b — hedef üçlüsü farklı: C=[$ct] KEMGU=[$st]"; fail=$((fail+1)); continue
    fi

    grep "^define" "$TMP/c.ll" | sed 's/(.*//' | sort > "$TMP/c.d"
    grep "^define" "$TMP/s.ll" | sed 's/(.*//' | sort > "$TMP/s.d"
    if diff -q "$TMP/c.d" "$TMP/s.d" >/dev/null 2>&1; then
        pass=$((pass+1))
    else
        echo "  🔴 $b — define kümesi farklı (C=$(wc -l <"$TMP/c.d") KEMGU=$(wc -l <"$TMP/s.d")):"
        diff "$TMP/c.d" "$TMP/s.d" 2>/dev/null | head -5 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done
echo "=== bare-metal (aarch64) yapı paritesi: $pass/$((pass+fail)) dosya ($atla atlandı) ==="
[ "$fail" -eq 0 ]
