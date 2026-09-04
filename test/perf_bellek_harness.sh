#!/usr/bin/env bash
# ============================================================================
# perf_bellek_harness.sh — [D-528] D-506'nin 17x bellek kazanci REGRESYON kapisi
# ----------------------------------------------------------------------------
# NE OLCULUYOR: `test/perf/bench{1,2}.kem` calistirilir ve ZIRVE RSS (max
# resident set) okunur. D-506 `dizi_olustur(N)`i ρ_caller'dan ρ_yerel'e
# tasidiginda bench2'nin zirvesi 19968 KB -> 1152 KB dusmustu (17x). O kazanc
# bir YONLENDIRME kararina baglidir ve SESSIZCE kaybolabilir:
#
#   ⚠ DAVRANISSAL KAPILAR BU SINIFA KORDUR (D-417/D-488). Yonlendirme bozulsa
#     program yine `exit 42` verir, `codegen_diff` yesil kalir, ASan susar —
#     yalniz bellek buyur. `bolge_operand` IR'daki ρ SINIFINI olcer (yapisal);
#     bu kapi GERCEK BELLEK TUKETIMINI olcer (davranissal). Ikisi birbirini
#     tamamlar, biri digerinin yerine gecmez.
#
# ESIK 4096 KB: bugunku olcum 1152 KB (her iki derleyicide de). Esik ~3.5x
# bassluk birakir ama ρ_caller'a donusu (19968 KB) KESIN yakalar. Dar bir esik
# ortam gurultusunden aralikli kirmizi verirdi; genis bir esik regresyonu
# kacirirdi.
#
# ⚠ EXIT KODU DA DENETLENIR (42): yalniz bellek olcmek yetmez — program hic
#   calismadan da dusuk RSS verir (D-506'da rc=127 ile 0.00 sn olculmustu).
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

# Zirve RSS olcumu ortam yetenegidir. YOKSA SESSIZCE GECME — acikca bildir ve
# atla (calistir_qemu_cekirdek'in QEMU icin yaptigi gibi, D-453). D-486'nin
# yasakladigi sey SESSIZ atlamadir, bildirilmis atlama degil.
if [ ! -x /usr/bin/time ]; then
    echo "⚠ /usr/bin/time YOK — zirve RSS ölçülemiyor, kapı ATLANDI (ortam yeteneği)"
    exit 0
fi

ESIK_KB=4096
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/perfbellek.XXXXXX" 2>/dev/null || echo "build/perfbellek.$$")
mkdir -p "$TMP"
hata=0; olculen=0

for c in "$KEMGU:C" "$CODEGEN:SELF"; do
    bin="${c%%:*}"; et="${c##*:}"
    for b in bench1 bench2; do
        f="test/perf/$b.kem"
        [ -f "$f" ] || { echo "  🔴 $f YOK — kapı boşa koşamaz"; hata=1; continue; }
        if ! "$bin" --llvm "$f" > "$TMP/p.ll" 2>/dev/null; then
            echo "  🔴 $et $b: IR üretilemedi"; hata=1; continue
        fi
        if ! clang -O2 -x ir "$TMP/p.ll" -x none "$RT" -o "$TMP/p_$b" 2>/dev/null; then
            echo "  🔴 $et $b: LINK-RED"; hata=1; continue
        fi
        # %M = zirve RSS (KB). Program exit 42 dondurdugu icin `time` stderr'e
        # bir uyari satiri da basar -> SON satiri al.
        cikti=$(/usr/bin/time -f "%M" "$TMP/p_$b" 2>&1 >/dev/null)
        rc=$?
        kb=$(printf '%s\n' "$cikti" | tail -1)
        olculen=$((olculen+1))
        if [ "$rc" -ne 42 ]; then
            echo "  🔴 $et $b: exit=$rc (beklenen 42 — program çalışmadıysa RSS anlamsızdır)"
            hata=1; continue
        fi
        case "$kb" in ''|*[!0-9]*) echo "  🔴 $et $b: RSS okunamadı ('$kb')"; hata=1; continue;; esac
        if [ "$kb" -gt "$ESIK_KB" ]; then
            echo "  🔴 $et $b: zirve RSS ${kb} KB > eşik ${ESIK_KB} KB"
            echo "     → D-506'nın ρ_yerel yönlendirmesi düşmüş olabilir (ρ_caller'da"
            echo "       bench2 19968 KB ölçülmüştü). `bolge_operand` kapısını da koştur:"
            echo "       o IR'daki ρ SINIFINI ölçer, bu kapı GERÇEK belleği."
            hata=1
        fi
    done
done

if [ "$olculen" -eq 0 ]; then
    echo "🔴 HATA: hiç ölçüm yapılmadı — kapı boşa koştu"; exit 1
fi
if [ "$hata" -ne 0 ]; then
    echo "=== perf bellek kapısı: BAŞARISIZ ($olculen ölçüm) ==="; exit 1
fi
echo "=== perf zirve bellek (eşik ${ESIK_KB} KB): $olculen ölçüm (C + SELF) ==="
