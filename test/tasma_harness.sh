#!/usr/bin/env bash
# ============================================================================
# tasma_harness.sh — [D-524] Aritmetik taşma: SARMA TANIMLI mı, KARARLI mı?
# ----------------------------------------------------------------------------
# D-513'te iki davranış BİRBİRİNDEN AYRILDI ve karıştırmak tehlikelidir:
#
#   KAYDIRMA (>= bit genişliği)  -> LLVM POISON = UB. Ölçüldü: -O0 exit=0,
#                                   -O2 exit=1 (AYNI PROGRAM). D-514'te
#                                   `kdl_panik` ile kapatıldı.
#   ARİTMETİK TAŞMA              -> `add/sub/mul` IR'ında `nsw`/`nuw` YOK,
#                                   yani iki'nin tümleyeni sarması TANIMLI.
#                                   -O0 ve -O2 AYNI sonucu verir.
#
# Bu kapı İKİNCİSİNİ SABİTLER. Dil değişikliği YOK — amaç, sarmanın ileride
# sessizce UB'ye kaymasını engellemek: biri `nsw`/`nuw` eklerse (ya da bir
# optimizasyon o varsayımı sokarsa) davranış -O2'de değişir ve BU KAPI kırmızı
# olur. Davranışsal kapıların bu sınıfa KÖR olduğu ölçülmüştür (D-417).
#
# İKİ AYRI İDDİA ÖLÇÜLÜR:
#   (1) DAVRANIŞ: her fikstür -O0 ve -O2'de AYNI çıkışı verir (ve 42'dir).
#   (2) YAPI:     üretilen IR'da `nsw`/`nuw` YOKTUR — sarma varsayımının
#                 kaynağı budur. Yalnız (1) ölçülseydi, biri `nsw` eklediğinde
#                 bugünkü clang hâlâ aynı sonucu üretebilir ve kapı sessiz
#                 kalırdı; iddia YAPIDA yaşıyor.
#
# ⚠ HER İKİ DERLEYİCİDE koşar (D-407: aynı soruyu iki yerde ayrı yanıtlayan
#   kod ayrışır).
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

# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/tasma.XXXXXX" 2>/dev/null || echo "build/tasma.$$")
mkdir -p "$TMP"
hata=0; olculen=0

for c in "$KEMGU:C" "$CODEGEN:SELF"; do
    bin="${c%%:*}"; et="${c##*:}"
    for f in test/tasma/*.kem; do
        b=$(basename "$f" .kem)
        if ! "$bin" --llvm "$f" > "$TMP/a.ll" 2>/dev/null; then
            echo "  🔴 $et $b: IR üretilemedi"; hata=1; continue
        fi

        # (2) YAPI: nsw/nuw sızmış mı?
        nsw=$(grep -cE '\b(add|sub|mul) (nsw|nuw)' "$TMP/a.ll")
        if [ "$nsw" -ne 0 ]; then
            echo "  🔴 $et $b: IR'da $nsw adet nsw/nuw — sarma artık TANIMLI DEĞİL (UB)"
            echo "     → Taşma davranışı sessizce değişti. Kasıtlıysa bu kapı ve"
            echo "       CLAUDE.md D-513/D-524 kaydı birlikte güncellenmeli."
            hata=1
        fi

        # (1) DAVRANIŞ: -O0 ve -O2 aynı mı?
        prev=""
        for O in O0 O2; do
            if ! clang -$O -x ir "$TMP/a.ll" -x none "$RT" -o "$TMP/a_$O.exe" 2>/dev/null; then
                echo "  🔴 $et $b: -$O LINK-RED"; hata=1; prev=""; break
            fi
            timeout 30 "$TMP/a_$O.exe" >/dev/null 2>&1; rc=$?
            olculen=$((olculen+1))
            if [ "$rc" -ne 42 ]; then
                echo "  🔴 $et $b: -$O exit=$rc (beklenen 42 — sarma beklenen değeri vermedi)"
                hata=1
            fi
            if [ -n "$prev" ] && [ "$prev" != "$rc" ]; then
                echo "  🔴 $et $b: -O0 exit=$prev ≠ -O2 exit=$rc — UB BELİRTİSİ"
                echo "     → Aynı program, aynı IR, farklı cevap. Kaydırma (D-513) tam"
                echo "       böyle görünüyordu; taşmanın da UB'ye kaymış olması muhtemel."
                hata=1
            fi
            prev=$rc
        done
    done
done

if [ "$olculen" -eq 0 ]; then
    echo "🔴 HATA: hiç ölçüm yapılmadı — kapı boşa koştu"; exit 1
fi
if [ "$hata" -ne 0 ]; then
    echo "=== aritmetik taşma kapısı: BAŞARISIZ ($olculen ölçüm) ==="; exit 1
fi
echo "=== aritmetik taşma (sarma tanımlı + nsw/nuw yok): $olculen ölçüm (C + SELF) ==="
