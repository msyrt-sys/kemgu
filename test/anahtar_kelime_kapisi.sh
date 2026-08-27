#!/usr/bin/env bash
# ============================================================================
# anahtar_kelime_kapisi.sh — [D-492] Dil yüzeyi ile BELGE senkron mu?
# ----------------------------------------------------------------------------
# OLCULEN: `src/anahtar_kelime.c`teki HER anahtar kelime, EBNF gramerinde
# (`belgeler/KEMGU_Grammar_EBNF.md`) geciyor mu?
#
# NEDEN: Ilk kosumda **41 anahtar kelimenin 16'si (%39) gramerde YOKTU**
# (`görev` `kanal` `tekkez` `imha` `yetki` `sabitsüre` `vektör` `değer` `hiç`
# `tamam` `hata` `bölge` `kendin` `geri_al` `delege` `gerçekzamanlı`).
# Gramer 2026-05-09'dan beri dokunulmamis; o tarihten sonra dile eszamanlilik,
# lineer tipler, yetki ve SIMD girdi.
# **Dili TARIF ETMEYEN bir gramer, olmayan gramerden TEHLIKELIDIR** — ona gore
# uygulama yazan yanlis yazar. Bu kapi sürüklenmeyi SABITLER.
#
# ⚠ ANAHTAR KELIMELER KAYNAKTA HEX ESCAPE'LIDIR (`"de\xc4\x9f" "er"`), yani
# duz `grep '"[a-z]+"'` TURKCE OLANLARI KACIRIR. Ilk olcumumde tam bu oldu:
# 41 yerine 25 buldum ve eksik listesi YANLIS cikti. Okunabilir ad, tablodaki
# YORUM SUTUNUNDADIR; kapi oradan okur.
#
# ⚠ BU KAPI SOZDIZIMI DOGRULUGU OLCMEZ — yalnizca "kelime gramerde GECIYOR mu"
# der. Gramerin kuralinin DOGRU oldugunu ölçmez; onu ancak parser paritesi
# (`calistir_parser_diff`) ve korpus gosterir.
# ============================================================================
set -u
KAYNAK=${KAYNAK:-src/anahtar_kelime.c}
GRAMER=${GRAMER:-belgeler/KEMGU_Grammar_EBNF.md}
[ -f "$KAYNAK" ] || { echo "🔴 HATA: $KAYNAK yok — kapı KOŞMADI"; exit 1; }
[ -f "$GRAMER" ] || { echo "🔴 HATA: $GRAMER yok — kapı KOŞMADI"; exit 1; }

LISTE=$(grep -oE "/\* [a-zA-ZçğıöşüÇĞİÖŞÜ_]+ +\*/" "$KAYNAK" | sed 's|/\* *||; s| *\*/||' | sort -u)
say=$(echo "$LISTE" | grep -c .)
if [ "$say" -lt 20 ]; then
    echo "🔴 HATA: yalnız $say anahtar kelime çözüldü — çıkarım BOZUK (kapı boşa koşar)"
    exit 1
fi

eksik=0
for k in $LISTE; do
    grep -q -- "$k" "$GRAMER" || { echo "  🔴 gramerde YOK: $k"; eksik=$((eksik+1)); }
done

if [ "$eksik" -ne 0 ]; then
    echo "=== anahtar kelime kapısı: $eksik/$say kelime GRAMERDE YOK ==="
    echo "    → $GRAMER dili tarif etmiyor; ekle ya da gramerin kapsamını açıkça daralt."
    exit 1
fi
echo "=== anahtar kelime kapısı: $say kelimenin tamamı gramerde geçiyor ==="
