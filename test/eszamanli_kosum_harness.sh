#!/usr/bin/env bash
# [D-645] EŞZAMANLILIK İLKELLERİ — KOŞUM KAPISI (kilit, semafor)
#
# NEDEN VAR: `stdlib/kilit.kem` ve `stdlib/semafor.kem` bugüne dek YALNIZ
# tip-denetleniyordu (`calistir_stdlib_check`) ve depoda onları kullanan TEK
# dosya yoktu — ölçüldü. Tip denetimi ÇALIŞMAYI KANITLAMAZ (D-425). README de
# bu ikisini "yapılacak" diye sayıyordu; ölçüm bunu yalanladı (D-639).
#
# ⚠ ÇOK TURLU OLMASI ZORUNLU, SÜS DEĞİL. Sabotajla ÖLÇÜLDÜ (S147: `kilitle`
# kilidi almıyor): tek koşumda hata oranı ~%70 (20 turun 14'ü). Yani TEK turluk
# bir kapı, bozuk bir kilidi turların ~%30'unda YEŞİL geçirirdi — flaky ve
# yanlış-yeşil. TUR sayısıyla yakalama olasılığı 1-0.3^N'e gider.
# Aynı gerekçe `drf_gorunurluk`ta da uygulanmıştı (100 tur).
#
# Birleştirme (`cat stdlib/X.kem test/...`) `calistir_stdlib_check`in idiomudur:
# stdlib modülleri `genel` DEĞİL, bu yüzden seçili import ile tüketilemezler.
set -u

TUR=${TUR:-20}

: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU="${KEMGU:-build/kemgu${EXE}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
[ -x "$KEMGU" ] || { echo "🔴 HATA: kemgu ikilisi YOK ($KEMGU) — kapı KOŞMADI"; exit 1; }

RT=build/kdl_runtime.o
[ -f "$RT" ] || { echo "🔴 HATA: $RT yok — kapı KOŞMADI"; exit 1; }

source test/calisma_dizini.sh
TMP=$(calisma_ac build/eszamanli) || exit 1
calisma_izle "$TMP"


pass=0; fail=0
# [D-668] `modul:test` ciftleri. `semafor_n` AYNI modulu (semafor) kullanir ama
# n>1 semantigini olcer: "en fazla n" (guvenlik) VE "n kadar gercekten" (anlam).
# Cikis kodlari test icinde ayri: 1=n asildi, 2=kilit gibi, 3=olcum guvenilmez.
# [D-669] `bariyer` "herkes gelmeden kimse gecmiyor" + YENIDEN KULLANIM (5 tur).
# Cikis: 1=bariyer erken gecirdi, 3=sayac dengesi bozuk.
for cift in kilit:kilit semafor:semafor semafor:semafor_n bariyer:bariyer; do
    lib="${cift%%:*}"; m="${cift#*:}"
    LIB="stdlib/$lib.kem"
    TST="test/eszamanli/${m}_kosum.kem"
    if [[ $m == semafor_n ]]; then TST=test/olcum/semafor_n_kosum.kem; fi
    for f in "$LIB" "$TST"; do
        [ -f "$f" ] || { echo "🔴 HATA: $f YOK — kapı KOŞMADI"; exit 1; }
    done

    cat "$LIB" "$TST" > "$TMP/$m.kem"
    if ! "$KEMGU" --llvm "$TMP/$m.kem" > "$TMP/$m.ll" 2>"$TMP/$m.err"; then
        echo "  🔴 $m — IR üretilemedi:"; head -3 "$TMP/$m.err" | sed 's/^/       /'
        fail=$((fail+1)); continue
    fi
    if ! clang -x ir "$TMP/$m.ll" -x none "$RT" -o "$TMP/$m.out" 2>"$TMP/$m.clang"; then
        echo "  🔴 $m — link edilemedi:"; head -3 "$TMP/$m.clang" | sed 's/^/       /'
        fail=$((fail+1)); continue
    fi

    kotu=0
    for tur in $(seq 1 "$TUR"); do
        if [[ $m == semafor_n ]]; then
            # Tek izin veya takılan işçi çift buluşmasını sonsuza dek beklemesin.
            timeout 10 "$TMP/$m.out" >/dev/null 2>&1; son=$?
            if [[ $son == 124 ]]; then
                echo "  $m tur $tur/$TUR: iki izinli buluşma zaman aşımı (10 sn)" | tee -a "$TMP/hatali-turlar.log"
                kotu=$((kotu+1)); break
            fi
        else
            "$TMP/$m.out" >/dev/null 2>&1; son=$?
        fi
        if [ "$son" -ne 42 ]; then
            echo "  $m tur $tur/$TUR: çıkış $son" | tee -a "$TMP/hatali-turlar.log"
            kotu=$((kotu+1))
        fi
    done
    if [ "$kotu" -ne 0 ]; then
        case "$m" in
            semafor_n) echo "  🔴 $m — $tur koşumda $kotu hata (1=izin aşıldı, 2=örtüşme yok, 3=sayaç, 124=buluşma zaman aşımı)" ;;
            bariyer)   echo "  🔴 $m — $TUR turun $kotu'inde bariyer sozlesmesi BOZUK (1=erken gecirdi 3=sayac bozuk)" ;;
            *)         echo "  🔴 $m — $TUR turun $kotu'inde sayaç yanlış (karşılıklı dışlama YOK)" ;;
        esac
        fail=$((fail+1))
    else
        case "$m" in
            semafor_n) echo "  ✅ $m — $TUR/$TUR tur, en fazla 2 VE gercekten 2 (4 kosucu, n=2)" ;;
            bariyer)   echo "  ✅ $m — $TUR/$TUR tur, kimse erken gecmedi (4 katilimci x 5 bulusma, yeniden kullanim dahil)" ;;
            *)         echo "  ✅ $m — $TUR/$TUR tur, sayaç tam (3 koşucu × 500 = 1500)" ;;
        esac
        pass=$((pass+1))
    fi
done

echo "=== eşzamanlılık ilkelleri koşumu: $pass/$((pass+fail)) modül ($TUR tur) ==="
[ "$fail" -eq 0 ]
