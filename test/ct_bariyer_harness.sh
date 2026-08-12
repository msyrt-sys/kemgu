#!/usr/bin/env bash
# ============================================================================
# ct_bariyer_harness.sh — SABİT-SÜRE spekülasyon bariyeri kapısı (D-417).
# ----------------------------------------------------------------------------
# NEDEN AYRI BİR KAPI — ÖLÇÜLDÜ, VARSAYILMADI:
# `sabitsüre_olustur` / `ifşa` yerleşiklerinin yaydığı `llvm.x86.sse2.lfence`
# bariyerini SİLDİM (sabotaj S159) ve `codegen_diff` **139/139 YEŞİL KALDI**;
# bariyer sayısı 10 → 0 düştü. Yani davranışsal kapılar bu kusuru GÖREMEZ:
# bariyerin yokluğu link hatası vermez, IR geçerli kalır, program aynı sonucu
# üretir — kaybolan tek şey SABİT-SÜRE GARANTİSİDİR.
#
# > Güvenlik özelliklerinde "program doğru çalıştı" YETERSİZ KANITTIR.
# > Bu kapı davranışı değil YAPIYI ölçer: bariyer SAYISI C oracle ile birebir mi.
#
# Kullanım: bash test/ct_bariyer_harness.sh  (veya make calistir_ct_bariyer)
# ============================================================================
set -u
KEMGU=${KEMGU:-build/kemgu.exe}
CODEGEN=${CODEGEN:-build/codegen.exe}

if [ ! -x "$CODEGEN" ]; then
    echo "ℹ codegen.exe yok — kapı atlandı (önce build/codegen.exe kurulmalı)."
    exit 0
fi

pass=0; fail=0; atla=0
# `sabitsüre` kullanan HER .kem dosyası — korpus elle seçilmez, taranır ki
# yeni bir dosya eklendiğinde kapı kendiliğinden kapsasın.
for f in $(grep -rl "sabitsüre_olustur\|ifşa(" --include=*.kem test stdlib kütüphane 2>/dev/null | sort); do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)

    # D-424: oracle IR üretemiyorsa sebep TİP HATASIDIR → "atla" DEĞİL, POZİTİF
    # İDDİA: self-host da reddetmeli. Bu kapının 6 atlamasının TAMAMI buydu ve
    # altısında da `--check` C ile BİREBİR paritedeydi — yani checker hazırdı,
    # eksik olan yalnız `--llvm`in onu çağırmasıydı (D-424 onardı).
    c_ir=$("$KEMGU" --llvm "$f" 2>/dev/null)
    case "$c_ir" in
        ""|hata*)
            if s_red=$("$CODEGEN" --llvm "$f" 2>/dev/null) && [ -n "$s_red" ]; then
                echo "  🔴 $b — C tip hatasıyla REDDEDİYOR, KEMGU IR ÜRETİYOR (loud→silent)"
                fail=$((fail+1)); continue
            fi
            pass=$((pass+1)); continue      # iki taraf da reddediyor → PARİTE
            ;;
    esac

    s_ir=$("$CODEGEN" --llvm "$f" 2>/dev/null) || {
        echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue; }

    cn=$(printf '%s' "$c_ir" | grep -c "llvm.x86.sse2.lfence")
    sn=$(printf '%s' "$s_ir" | grep -c "llvm.x86.sse2.lfence")

    if [ "$cn" -eq "$sn" ]; then
        pass=$((pass+1))
    else
        echo "  🔴 $b — bariyer sayısı: C=$cn ≠ KEMGU=$sn"
        fail=$((fail+1))
    fi
done
echo "=== sabit-süre bariyer paritesi: $pass/$((pass+fail)) dosya ($atla atlandı) ==="
[ "$fail" -eq 0 ]
