#!/usr/bin/env bash
# ============================================================================
# sifir_uyari_harness.sh — [D-493] "SIFIR UYARI HEDEFİ" gerçekten tutuyor mu?
# ----------------------------------------------------------------------------
# CLAUDE.md'nin CALISMA KURALI: "-Wall -Wextra -Wpedantic — sıfır uyarı hedefi".
# OLCULDU: bayraklar VAR ama `-Werror` YOK ve HICBIR KAPI bunu zorlamiyordu ->
# bir uyari sessizce sizabilir ve yapim YESIL kalir. Kural ilan edilmis ama
# olculmuyordu (bu depoda D-446/D-486/D-488'de tekrarlanan sinif).
#
# ⚠ NEDEN `-Werror` DEGIL, AYRI KAPI: `-Werror`u normal yapima koymak,
# FARKLI DERLEYICI SURUMLERINDE (GCC 16 / clang 15 — bu depo IKISINI de
# kullanir) yeni bir uyari cikinca YAPIMI TAMAMEN KIRAR. Kapi ise yalnizca
# KIRMIZI olur; gelistirici calismaya devam edebilir. Loud ama bloklamayan.
#
# ⚠ `build/`E DOKUNMAZ: nesneler gecici dizine yazilir. Paylasilan `build/`e
# yazan kapilar es zamanli kosumda birbirini ezer (D-297/D-414 — bu depoda
# defalarca SAHTE KIRMIZI uretti).
#
# ⚠ KAPSAM: `src/` + `runtime/` (gonderilen C kodu). `test/*.c` DISARIDA —
# orada bilerek sinir zorlayan kod var ve kapiyi gurultuye cevirirdi.
# ============================================================================
set -u
CC_UYARI=${CC_UYARI:-gcc}
BAYRAK="-Wall -Wextra -Wpedantic -std=c11"
command -v "$CC_UYARI" >/dev/null 2>&1 || { echo "🔴 HATA: $CC_UYARI yok — kapı KOŞMADI"; exit 1; }

# ⚠ DORT `runtime/` DOSYASI `-DKEMGU_BARE_METAL` ya da `-DKEMGU_UART_MOCK`
# GEREKTIRIR ve olmadan BILEREK `#error` verir (panik · uart_16550 ·
# uart_pl011 · yazdir_bare). Ilk surumumde bunlari "DERLEME HATASI" saydim —
# kusur KODDA DEGIL, KAPIDAYDI. Makefile host tarafinda `-DKEMGU_UART_MOCK`
# kullanir (satir 859); kapi ayni tanimi verir.
EK_TANIM="-DKEMGU_UART_MOCK -Iruntime"

# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/sifir_uyari.XXXXXX" 2>/dev/null || echo "build/sifir_uyari.$$")
mkdir -p "$TMP"
LOG="$TMP/uyari.log"; : > "$LOG"

dosya=0
for f in src/*.c runtime/*.c; do
    [ -f "$f" ] || continue
    dosya=$((dosya+1))
    # 2>&1: uyarilar stderr'e gider. Hata da olsa yakalanir (asagida ayrilir).
    $CC_UYARI $BAYRAK -Isrc $EK_TANIM -c "$f" -o "$TMP/o.o" >>"$LOG" 2>&1 || echo "DERLEME-HATASI: $f" >> "$LOG"
done

if [ "$dosya" -eq 0 ]; then
    echo "🔴 HATA: hiç .c dosyası bulunamadı — kapı boşa koştu"; exit 1
fi

hata=$(grep -c "DERLEME-HATASI:" "$LOG" || true)
uyari=$(grep -c "warning:" "$LOG" || true)

if [ "${hata:-0}" -ne 0 ]; then
    echo "  🔴 $hata dosya DERLENEMEDİ:"
    grep "DERLEME-HATASI:" "$LOG" | head -5 | sed 's/^/     /'
    grep -E "error:" "$LOG" | head -3 | sed 's/^/     /'
    echo "=== sıfır uyarı kapısı: DERLEME BAŞARISIZ ==="
    exit 1
fi
if [ "${uyari:-0}" -ne 0 ]; then
    echo "  🔴 $uyari UYARI ($dosya dosya tarandı, derleyici: $CC_UYARI)"
    grep "warning:" "$LOG" | head -6 | sed 's/^/     /'
    echo "=== sıfır uyarı kapısı: KURAL İHLALİ ==="
    exit 1
fi
echo "=== sıfır uyarı: $dosya dosya, 0 uyarı ($CC_UYARI $BAYRAK) ==="
