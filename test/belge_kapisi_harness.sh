#!/usr/bin/env bash
# ============================================================================
# belge_kapisi_harness.sh — "KOD VAR AMA HİÇBİR ÖLÇÜM ATEŞLEMİYOR" kapısı.
# ----------------------------------------------------------------------------
# NEDEN VAR: bu sınıf bu depoda DÖRT kez elle yakalandı ve her seferinde gerçek
# bir şey sakladı:
#   · D-458  json.kem başlık yorumu `\b`/`\f`yi "desteklenir" sayıyordu ama
#            KOLLARI YOKTU — belge kodu yanlış anlatıyordu.
#   · D-461  regex `\d \w \s` kodda vardı, --check geçiyordu, hiçbir ölçüm
#            ateşlemiyordu.
#   · D-462  json AYRIŞTIRICI tarafında `\" \ \/` ölçülmüyordu (`\"`/`\`
#            yalnız YAZICI tarafında sınanıyordu).
#   · D-462  `\u`nun BAŞARI yolu hiç ölçülmüyordu: sanılan test `"aAb"` idi,
#            içinde ters bölü YOK — kaçış hiç ateşlenmiyordu. Bu kapı bunu
#            kendi ilk koşumunda buldu.
# Deponun ilkesi: elle taranan ölçüm eskir, kapı eskimez (D-428).
#
# KAPSAM BİLİNÇLİ OLARAK DAR: yalnız `stdlib/json.kem` dizgi ayrıştırıcısının
# kaçış kolları. Genel bir "belge iddiası" tarayıcısı yanlış-pozitif üretip
# kapıyı gürültüye çevirirdi. Bu kapı kolları KODDAN, ölçümleri TESTTEN okur;
# prozadan tahmin YOK.
#
# ⚠ Argüman çıkarımı `grep -oE` ile YAPILAMAZ: POSIX ERE'de tembel niceleyici
# yoktur ve çağrılar ÇOK SATIRLI olabilir (ilk sürüm tam bu yüzden iki
# yanlış-pozitif verdi). Aşağıdaki awk parantez derinliği izler.
#
# Kullanım: bash test/belge_kapisi_harness.sh   (make calistir_belge_kapisi)
# ============================================================================
set -u
JSON=stdlib/json.kem
TEST=test/stdlib/test_json.kem
[ -f "$JSON" ] && [ -f "$TEST" ] || { echo "🔴 kaynak dosyalar yok"; exit 1; }

# --- (1) UYGULANAN kaçış kolları: `ja_metin` içindeki `k == N` --------------
# N = kaçış KARAKTERİNİN baytı (ör. 110 = 'n').
uygulanan=$(awk '/işlev ja_metin/,/^}/' "$JSON" | grep -oE 'k == [0-9]+' \
            | grep -oE '[0-9]+' | sort -un)
[ -n "$uygulanan" ] || { echo "🔴 kaçış kolları bulunamadı — `ja_metin` adı değişmiş olabilir (kapı körleşmesin)"; exit 1; }

# --- (2) ÖLÇÜLEN çözülen-baytlar + `\u` başarı ölçümü var mı ----------------
ARGS=$(tr '\n' ' ' < "$TEST" | awk '
{
  s=$0
  while ((p=index(s,"json_kacis_bayt(")) > 0) {
    s=substr(s,p+16); d=1; i=1; buf=""
    while (i<=length(s)) {
      c=substr(s,i,1)
      if (c=="(") d++
      else if (c==")") { d--; if (d==0) break }
      buf=buf c; i++
    }
    dd=0; f=1; a[1]=""
    for (j=1;j<=length(buf);j++){
      c=substr(buf,j,1)
      if(c=="(") dd++
      else if(c==")") dd--
      else if(c=="," && dd==0){ f++; a[f]=""; continue }
      a[f]=a[f] c
    }
    if (f>=2) { gsub(/^ +| +$/,"",a[2]); print a[2] "\t" a[1] }
    s=substr(s,i)
  }
}')
olculen=$(echo "$ARGS" | cut -f1 | grep -oE '^[0-9]+$' | sort -un)
# `\u` BAŞARI ölçümü: birinci argümanda ters bölü + `u` + hex.
u_olculdu=$(echo "$ARGS" | cut -f2 \
            | grep -cE 'ters_bolu\(\)[^"]*"u[0-9A-Fa-f]|\u[0-9A-Fa-f]')

# --- (3) kaçış karakteri -> çözülen bayt eşlemesi ---------------------------
coz() {
    case "$1" in
        34) echo 34 ;;   92) echo 92 ;;   47) echo 47 ;;
        98) echo 8  ;;  102) echo 12 ;;  110) echo 10 ;;
       114) echo 13 ;;  116) echo 9  ;;
       117) echo U ;;                      # \uXXXX — çözülen bayt değişken
        *)  echo "?" ;;
    esac
}

eksik=0
for k in $uygulanan; do
    bek=$(coz "$k")
    case "$bek" in
      "?") echo "🔴 BİLİNMEYEN kaçış kolu k == $k — eşleme tablosuna EKLE (yoksa kapı körleşir)."
           eksik=$((eksik+1)) ;;
      "U") if [ "$u_olculdu" -eq 0 ]; then
               echo "🔴 \u kolu UYGULANMIŞ ama BAŞARI yolunu ölçen test YOK."
               echo "     (reddetme testleri saymaz — D-462'de tam bu kaçmıştı)"
               eksik=$((eksik+1))
           fi ;;
      *)   if ! echo "$olculen" | grep -qx "$bek"; then
               echo "🔴 kaçış kolu k == $k (çözülen bayt $bek) UYGULANMIŞ ama ÖLÇÜLMÜYOR."
               echo "     -> test_json.kem'e json_kacis_bayt(...) ölçümü ekle."
               eksik=$((eksik+1))
           fi ;;
    esac
done

kol=$(echo "$uygulanan" | wc -w)
echo "=== belge/ölçüm kapısı: $((kol-eksik))/$kol kaçış kolu ölçülüyor ==="
[ "$eksik" -eq 0 ]
