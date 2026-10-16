#!/usr/bin/env bash
# [D-665] BÖLGE SAYAÇLARI — EŞ ZAMANLI DOĞRULUK KAPISI
#
# Gerçek `runtime/kdl_bolge.c`yi iki biçimde derleyip koşar:
#   (1) düz derleme, birkaç tur — sayaç kaybı var mı (beklenen = oluştur = serbest)
#   (2) ThreadSanitizer — yarışı ŞANSA BAĞLI OLMADAN yakalar
#
# ⚠ İKİSİ DE GEREKLİ: (1) yarış manifest olmazsa yeşil kalabilir (şansa bağlı);
# (2) TSan her platformda yok (Windows/MinGW desteklemez). (2) yoksa ATLANDIĞI
# AÇIKÇA bildirilir (D-486: sessiz atlama yasak).
#
# ⚠⚠ ASan BU SINIFI GÖRMEZ. Bu yarış bugüne dek fark edilmedi çünkü depodaki
# tüm sanitizer kapıları ASan/UBSan'dı. Dış bir inceleme izole testle buldu;
# bu kapı onu GERÇEK runtime'a karşı kalıcı kılar.
#
# Sabotajla ölçüldü (S167, -D__STDC_NO_ATOMICS__ = düz sayaç): (1) 5 turun
# 5'inde kayıp (ARM64'te oluştur 63.841/200.000), (2) TSan rc=66, yarış
# `kdl_bolge.c:84` (sayaç ++) satırını gösteriyor.
set -u

: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
CC=${CC_BOLGE:-clang}
command -v "$CC" >/dev/null 2>&1 || CC=gcc
TUR=${TUR:-3}

TMP=$(mktemp -d "build/bolge_sayac.XXXXXX" 2>/dev/null || echo "build/bolge_sayac.$$")
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

SRC="runtime/kdl_bolge.c test/test_kdl_bolge_eszamanli.c"
LIBS=""
[ -n "$EXE" ] || LIBS="-lpthread"   # POSIX: pthread; Windows: CreateThread (ek lib yok)

fail=0

# ---- (1) düz derleme, çok tur ----
if ! $CC -std=c11 -O2 -Iruntime -o "$TMP/duz$EXE" $SRC $LIBS 2>"$TMP/duz.err"; then
    echo "  🔴 düz derleme BAŞARISIZ:"; head -3 "$TMP/duz.err" | sed 's/^/       /'
    exit 1
fi
kayip=0
for i in $(seq 1 "$TUR"); do
    "$TMP/duz$EXE" > "$TMP/duz.out" 2>&1 || { kayip=$((kayip+1)); tail -1 "$TMP/duz.out" | sed 's/^/       /'; }
done
if [ "$kayip" -ne 0 ]; then
    echo "  🔴 sayaç KAYBI: $TUR turun $kayip'inde"; fail=1
else
    echo "  ✅ sayım: $TUR/$TUR tur, sayaçlar tam ($(head -1 "$TMP/duz.out"))"
fi

# ---- (2) ThreadSanitizer ----
# Yetenek ÖLÇÜLÜR (D-551/D-661): derlenip KOŞABİLMELİ.
tsan_var=0
if [ -z "$EXE" ] && printf 'int main(void){return 0;}\n' > "$TMP/p.c" \
   && $CC -fsanitize=thread -o "$TMP/p" "$TMP/p.c" 2>/dev/null \
   && "$TMP/p" >/dev/null 2>&1; then
    tsan_var=1
fi
if [ "$tsan_var" -eq 0 ]; then
    echo "  ⚠ ThreadSanitizer YOK bu ortamda (Windows/MinGW ya da derleyici desteksiz)"
    echo "     -> yarış yalnız SAYIMLA ölçüldü (şansa bağlı). Bkz. D-665."
else
    if ! $CC -std=c11 -g -O1 -fsanitize=thread -Iruntime -o "$TMP/tsan" $SRC $LIBS 2>"$TMP/tsan.err"; then
        echo "  🔴 TSan derlemesi BAŞARISIZ:"; head -3 "$TMP/tsan.err" | sed 's/^/       /'; fail=1
    else
        timeout 300 "$TMP/tsan" > "$TMP/tsan.out" 2>&1; trc=$?
        nyaris=$(grep -c "WARNING: ThreadSanitizer" "$TMP/tsan.out")
        if [ "$trc" -ne 0 ] || [ "$nyaris" -ne 0 ]; then
            echo "  🔴 ThreadSanitizer: rc=$trc, $nyaris yarış uyarısı"
            grep -m1 -A3 "WARNING: ThreadSanitizer" "$TMP/tsan.out" | sed 's/^/       /'
            fail=1
        else
            echo "  ✅ ThreadSanitizer: 0 yarış (rc=0)"
        fi
    fi
fi

echo "=== bölge sayaç eşzamanlılığı: $([ "$fail" -eq 0 ] && echo GEÇTİ || echo BAŞARISIZ) (TSan: $([ "$tsan_var" -eq 1 ] && echo var || echo YOK)) ==="
[ "$fail" -eq 0 ]
