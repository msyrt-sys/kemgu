#!/usr/bin/env bash
# ============================================================================
# surucu_kosum_harness.sh — VirtIO host-mock sürücü testleri DAVRANIŞ kapısı
# (D-736).
# ----------------------------------------------------------------------------
# NEDEN VAR: `calistir_surucu_diff` yalnız YAPIYI ölçer (`--check` dump +
# `define` AD kümesi) — LİNKLEMEZ, ÇALIŞTIRMAZ. Self-host `mmio_okuN/yazN` ve
# `delege` yerleşiklerini checker'da tanıyıp codegen'de EŞLEMİYORDU: IR
# `call i32 @mmio_oku32(ptr %rho, %kdl_yetki %y, i64 %a)` içeriyordu, define
# kümesi aynı kaldığı için surucu_diff YEŞİLDİ, link `use of undefined value`
# ile düşüyordu. Davranışsal kıyas olmadan bu sınıf görünmez (D-422/D-584).
#
# Ölçüm `otomasyon/chatgpt/dogrula.sh`e devredilir (ikinci bir ölçüm yazılmaz):
# C --check · C --llvm|link|çalıştır (exit 0) · self --check (OK) ·
# self --llvm|link|çalıştır (exit 0) · stdout C == self.
#
# KAPSAM: `tests/drivers/virtio/*_test.kem`. C oracle'ın --check'te
# REDDETTİĞİ dosyalar (tek başına derlenemeyen parçalar) ADIYLA basılıp
# atlanır; oracle'ın KABUL ettiği her dosya ZORUNLU olarak koşar — yeni test
# kendiliğinden kapsanır. Alt sınır: en az ASGARI dosya koşmalı (kapı kendini
# sessizce boşaltamasın — D-486).
# ============================================================================
set -u
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
ASGARI=${ASGARI:-3}

kosacak=()
for f in tests/drivers/virtio/*_test.kem; do
    [ -f "$f" ] || continue
    if "$KEMGU" --check "$f" >/dev/null 2>&1; then
        kosacak+=("$f")
    else
        echo "  ⚠ atlandı (C oracle --check reddediyor): $(basename "$f")"
    fi
done
if [ "${#kosacak[@]}" -lt "$ASGARI" ]; then
    echo "🔴 HATA: yalnız ${#kosacak[@]} host-mock test koşabiliyor (asgari $ASGARI) — kapı boşaldı"
    exit 1
fi
bash otomasyon/chatgpt/dogrula.sh "${kosacak[@]}" || exit 1
echo "=== sürücü host-mock koşumu (C + self link+çalıştır+stdout): ${#kosacak[@]}/${#kosacak[@]} ==="
