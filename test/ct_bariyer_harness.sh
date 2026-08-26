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
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}

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

    if [ "$cn" -ne "$sn" ]; then
        echo "  🔴 $b — x86 bariyer sayısı: C=$cn ≠ KEMGU=$sn"
        fail=$((fail+1)); continue
    fi

    # [D-468] ARM64 DALI DA ÖLÇÜLMELİ. Bu kapı yalnız x86 `lfence` sayıyordu;
    # ARM64 bariyeri eklendiğinde o dal HİÇBİR ölçümle korunmuyor olurdu —
    # yani `--mimari arm64`de bariyeri düşüren bir değişiklik SESSİZCE geçerdi.
    # Tam olarak bu kapının var oluş gerekçesi (D-417): davranışsal kapılar
    # güvenlik özelliğini göremez, o yüzden YAPIYI sayıyoruz. Aynı mantık yeni
    # mimariye de uygulanmalı, yoksa kapı yarısı kör kalır.
    #
    # ⚠ SAYILAR EŞİT OLMALI: aynı program, aynı sayıda bariyer — yalnız komut
    # değişir (x86 `lfence` ↔ ARM64 `csdb`). Sayı düşerse bariyer kayboluyordur.
    c_arm=$("$KEMGU"   --llvm --mimari arm64 "$f" 2>/dev/null)
    s_arm=$("$CODEGEN" --llvm --mimari arm64 "$f" 2>/dev/null)
    ca=$(printf '%s' "$c_arm" | grep -c "csdb")
    sa=$(printf '%s' "$s_arm" | grep -c "csdb")
    ca_lf=$(printf '%s' "$c_arm" | grep -c "llvm.x86.sse2.lfence")
    sa_lf=$(printf '%s' "$s_arm" | grep -c "llvm.x86.sse2.lfence")

    if [ "$ca" -ne "$sa" ]; then
        echo "  🔴 $b — ARM64 bariyer sayısı: C=$ca ≠ KEMGU=$sa"
        fail=$((fail+1)); continue
    fi
    if [ "$ca" -ne "$cn" ]; then
        echo "  🔴 $b — ARM64 bariyer sayısı x86'dan FARKLI (x86=$cn arm64=$ca)"
        fail=$((fail+1)); continue
    fi
    # ARM64 hedefinde x86 lfence yayılması GEÇERSİZ IR'dır — sızıntı olmamalı.
    if [ "$ca_lf" -ne 0 ] || [ "$sa_lf" -ne 0 ]; then
        echo "  🔴 $b — ARM64 hedefinde x86 lfence SIZDI (C=$ca_lf KEMGU=$sa_lf)"
        fail=$((fail+1)); continue
    fi
    pass=$((pass+1))
done
echo "=== sabit-süre bariyer paritesi: $pass/$((pass+fail)) dosya ($atla atlandı) ==="
[ "$fail" -eq 0 ]
