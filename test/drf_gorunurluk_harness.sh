#!/usr/bin/env bash
# ============================================================================
# drf_gorunurluk_harness.sh — [D-489] DRF OPERASYONEL GORUNURLUK KAPISI
# ----------------------------------------------------------------------------
# OLCULEN: kanal gonderiminden ONCE yapilan yazmalar, alimdan SONRA gorunur
# olmali (happens-before kenari). Bu, DRF teoreminin OPERASYONEL icerigidir.
#
# ⚠ NEDEN GEREKLI: Lean modelinde ispatli (proofs/drf-v2-lean, sorry=0) ama
# CALISAN DERLEYICIDE hicbir sey olcmuyordu — `test_gorev_rt.c`de gorunurluk
# testi YOKTU (olculdu). Ispat ile ikili arasindaki koprü olculmemis kaliyordu.
#
# ⚠⚠ BU KAPININ IKI DURUST SINIRI VAR — asiri yorumlanmasin:
#  1. x86-TSO GUCLU bir bellek modelidir. Burada gecmek, ZAYIF bellek modelinde
#     (ARM64) dogruluk KANITLAMAZ. ARM64 tarafinda `gorev`/`kanal` bare-metal
#     kullanimi henuz YOK (olculdu: runtime/*.kem ve os/*.kem'de sifir kullanim),
#     bu yuzden orada kosulamiyor.
#  2. Senkronizasyonsuz karsi-ornek DETERMINISTIK dusuyor (exit 91) — bu bir
#     ZAMANLAMA etkisidir, saf bellek-SIRALAMA etkisi degil. Yani kapi
#     "senkronizasyon var mi"ya duyarlidir; zayif-bellek kusurunu IZOLE ETMEZ.
#
# Teorik dayanak (olculdu): `kdl_gorev_*`/`kdl_kanal_*` declare satirlarinda
# HICBIR nitelik yok -> LLVM onlari her bellegi okuyup yazabilir sayar ve
# etraflarinda yeniden siralama YAPAMAZ; runtime ise gercek OS primitifleri
# kullanir (pthread mutex/condvar · CriticalSection), bunlar spesifikasyon
# geregi tam bellek bariyeridir. Yani IR'a AYRICA fence yaymak GEREKMIYOR.
# ============================================================================
set -u
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU="${KEMGU:-build/kemgu${EXE}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
[ -x "$KEMGU" ] || { echo "🔴 HATA: kemgu ikilisi YOK ($KEMGU) — kapı KOŞMADI"; exit 1; }

RT=build/kdl_runtime.o
[ -f "$RT" ] || { echo "🔴 HATA: $RT yok — kapı KOŞMADI"; exit 1; }
F=test/drf_gorunurluk.kem
[ -f "$F" ] || { echo "🔴 HATA: fikstür YOK ($F)"; exit 1; }

# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/drf_gor.XXXXXX" 2>/dev/null || echo "build/drf_gor.$$")
mkdir -p "$TMP"
TUR=${TUR:-100}

# -O2 BILEREK: optimizasyon yeniden siralama firsatlarini artirir. -O0'da
# gecmek zayif kanittir.
"$KEMGU" --llvm "$F" > "$TMP/d.ll" 2>/dev/null || { echo "🔴 HATA: IR üretilemedi"; exit 1; }
clang -O2 -x ir "$TMP/d.ll" -x none "$RT" -o "$TMP/d.exe" 2>/dev/null \
    || { echo "🔴 HATA: derlenemedi"; exit 1; }

# [D-564] ISINDIRMA: Windows'ta TAZE .exe'nin ILK kosumu 127 dondurebilir
# (Defender/exec yarisi, D-413). Bu kapi 300 turu SAYIYOR; isinmadan
# baslamak o 127'leri 'gorunurluk ihlali' gibi sayardi — yanlis atif.
d127=0
timeout 30 "$TMP/d.exe" >/dev/null 2>&1; wrc=$?
while [ "$wrc" -eq 127 ] && [ "$d127" -lt 12 ]; do
    sleep 0.3; timeout 30 "$TMP/d.exe" >/dev/null 2>&1; wrc=$?; d127=$((d127+1))
done

kotu=0; ilk=""
for i in $(seq 1 "$TUR"); do
    timeout 30 "$TMP/d.exe" >/dev/null 2>&1
    rc=$?
    if [ "$rc" -ne 42 ]; then kotu=$((kotu+1)); [ -z "$ilk" ] && ilk=$rc; fi
done

if [ "$kotu" -ne 0 ]; then
    echo "  🔴 GÖRÜNÜRLÜK İHLALİ: $kotu/$TUR turda başarısız (ilk çıkış=$ilk)"
    echo "     exit 91 = kanal alımından sonra yazmalar GÖRÜNMEDİ (HB kenarı yok)"
    echo "     exit 92 = jeton bozuk · exit 90 = görev başlatılamadı"
    echo "=== DRF görünürlük: BAŞARISIZ ==="
    exit 1
fi
echo "=== DRF görünürlük (kanal HB kenarı): $TUR/$TUR tur — 8 yazma da görünür ==="
