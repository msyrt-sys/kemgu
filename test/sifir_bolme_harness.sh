#!/usr/bin/env bash
# ============================================================================
# sifir_bolme_harness.sh — [D-502] Sıfıra bölme TEMİZ DURUYOR mu?
# ----------------------------------------------------------------------------
# ÖLÇÜLEN (D-501'de bulunan açık): korumasız `sdiv`/`srem` süreci SIGFPE ile
# ÖLDÜRÜYORDU (exit 136), tanı YOK, panik mesajı YOK, ve düz literal `10 / 0`
# bile `--check`ten temiz geçiyordu. Bu, dilin manşet iddiasının ihlaliydi:
# "Çökmezlik: Exception yok (sonuç<T,H>)".
#
# BEKLENEN: bölen sıfırsa `PANIK: sifira bolme` + exit 134 (kdl_panik →
# abort → SIGABRT). Dizi sınır ihlaliyle AYNI mekanizma (D-069).
# exit 136 (SIGFPE) = GERİLEME: koruyucu düşmüş.
#
# ⚠ HER İKİ DERLEYİCİDE ölçülür — koruyucu C'de olup self-host'ta olmazsa
# üretilen program yine çöker (D-407: aynı soruyu iki yerde ayrı yanıtlayan
# kod ayrışır).
#
# ⚠ NORMAL BÖLME AYRICA ÖLÇÜLÜR (`normal.kem` → 42): koruyucunun her bölmeyi
# panikletmediğini kanıtlar. Yalnız negatif şekiller olsaydı "her bölmeyi
# reddet" sabotajı kapıdan GEÇERDİ (D-425).
# ============================================================================
set -u
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU="${KEMGU:-build/kemgu${EXE}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
[ -x "$KEMGU" ] || { echo "🔴 HATA: kemgu ikilisi YOK ($KEMGU) — kapı KOŞMADI"; exit 1; }
CODEGEN="${CODEGEN:-build/codegen${EXE}}"
[ -x "$CODEGEN" ] || CODEGEN="build/codegen"
[ -x "$CODEGEN" ] || { echo "🔴 HATA: codegen ikilisi YOK ($CODEGEN) — kapı KOŞMADI"; exit 1; }
RT=build/kdl_runtime.o
[ -f "$RT" ] || { echo "🔴 HATA: $RT yok — kapı KOŞMADI"; exit 1; }

TMP=$(mktemp -d 2>/dev/null || echo /tmp/sifir_bolme); mkdir -p "$TMP"
hata=0; olculen=0

kos() {   # $1=derleyici $2=etiket $3=dosya $4=beklenen_exit
    "$1" --llvm "$3" > "$TMP/a.ll" 2>/dev/null || { echo "  🔴 $2 $(basename $3): IR üretilemedi"; hata=1; return; }
    clang -x ir "$TMP/a.ll" -x none "$RT" -o "$TMP/a.exe" 2>/dev/null \
        || { echo "  🔴 $2 $(basename $3): LINK-RED"; hata=1; return; }
    out=$(timeout 30 "$TMP/a.exe" 2>&1); rc=$?
    olculen=$((olculen+1))
    if [ "$rc" -ne "$4" ]; then
        echo "  🔴 $2 $(basename $3): exit=$rc (beklenen $4)"
        [ "$rc" -eq 136 ] && echo "     → SIGFPE: sıfır-bölme koruyucusu DÜŞMÜŞ (D-502 gerilemesi)"
        hata=1
        return
    fi
    if [ "$4" -eq 134 ] && ! echo "$out" | grep -q "PANIK: sifira bolme"; then
        echo "  🔴 $2 $(basename $3): exit doğru ama PANİK MESAJI yok"
        hata=1
    fi
}

for c in "$KEMGU:C" "$CODEGEN:SELF"; do
    bin="${c%%:*}"; et="${c##*:}"
    for f in test/bolme/sifir_*.kem; do kos "$bin" "$et" "$f" 134; done
    kos "$bin" "$et" test/bolme/normal.kem 42
done

if [ "$olculen" -eq 0 ]; then
    echo "🔴 HATA: hiç ölçüm yapılmadı — kapı boşa koştu"; exit 1
fi
if [ "$hata" -ne 0 ]; then
    echo "=== sıfıra bölme kapısı: BAŞARISIZ ($olculen ölçüm) ==="; exit 1
fi
echo "=== sıfıra bölme: $olculen ölçüm (C + SELF), hepsi temiz durdu ==="
