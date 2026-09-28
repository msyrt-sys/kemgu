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

TMP=$(mktemp -d "build/eszamanli.XXXXXX" 2>/dev/null || echo "build/eszamanli.$$")
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
for m in kilit semafor; do
    LIB="stdlib/$m.kem"
    TST="test/eszamanli/${m}_kosum.kem"
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
    for _ in $(seq 1 "$TUR"); do
        "$TMP/$m.out" >/dev/null 2>&1
        [ $? -eq 42 ] || kotu=$((kotu+1))
    done
    if [ "$kotu" -ne 0 ]; then
        echo "  🔴 $m — $TUR turun $kotu'inde sayaç yanlış (karşılıklı dışlama YOK)"
        fail=$((fail+1))
    else
        echo "  ✅ $m — $TUR/$TUR tur, sayaç tam (3 koşucu × 500 = 1500)"
        pass=$((pass+1))
    fi
done

echo "=== eşzamanlılık ilkelleri koşumu: $pass/$((pass+fail)) modül ($TUR tur) ==="
[ "$fail" -eq 0 ]
