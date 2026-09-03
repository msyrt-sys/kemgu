#!/usr/bin/env bash
# ============================================================================
# lean_sorry_harness.sh — [D-491] Lean ispat kodunda `sorry`/`admit` SIFIR mi?
# ----------------------------------------------------------------------------
# NEDEN: `proofs/drf-v2-lean` DRF + bellek-guvenligi mekanizasyonudur ve
# Adim 7 hedefi "0 sorry" idi. Hedef TUTTURULMUS (olculdu) ama
# `SORRY_TRACKER.md` Mayis'tan beri 47 diyordu -> KAPANMIS bir borc ACIK
# gorunuyordu. Kapi bunu SABITLER: kimse sessizce `sorry` ekleyemez.
#
# ⚠⚠ BU KAPI `lake build`IN YERINI TUTMAZ — fark ONEMLIDIR:
#   "0 sorry" (metin taramasi)  !=  "ISPATLAR GECIYOR" (tip denetimi)
# Bir dosyada tip hatasi olsa bu kapi GORMEZ. Gercek dogrulama `lake build`
# ister; Lean bu makinede KURULU DEGIL (olculdu: lake/lean PATH'te yok).
# `calistir_lean_aksiyom` hedefi VAR ama Lean yoksa SESSIZCE ATLIYOR ve
# `test_tumu`ya BAGLI DEGIL. Onu baglamak Lean kurulumunu ZORUNLU kilar ->
# DERLEME POLITIKASI karari (Mehmet). Sessizce-atlayan kapi eklemek BILINCLI
# OLARAK REDDEDILDI: D-486/D-490'da kapatilan kapsam yanilsamasinin ta kendisi.
#
# ⚠ LEAN'DE IKI YORUM BICIMI VAR: `--` SATIR ve `/- ... -/` BLOK.
# Ilk surumum yalniz `--`i atliyordu -> `NoFault.lean`in BLOK yorumundaki
# "sorry 0" ACIKLAMALARI acik borc sanildi (3 yanlis-pozitif, OLCULDU).
# Naif `grep -c sorry` ise 27 yanlis-pozitif verir (dosya basligindaki
# "sorry/axiom YOK" politika satiri). Tarayici ikisini de elemelidir.
# ============================================================================
set -u
DIZIN=${DIZIN:-proofs}
[ -d "$DIZIN" ] || { echo "🔴 HATA: $DIZIN yok — kapı KOŞMADI"; exit 1; }

# Tek kaynak: hem sayim hem raporlama BU yardimciyi kullanir (ayri iki
# tarayici yazmak D-407 sinifidir — ayrisirlar).
borc_satirlari() {
    awk '
      BEGIN { blok = 0 }
      {
        l = $0
        if (blok) { if (l ~ /-\//) blok = 0; next }
        sub(/^[ \t]+/, "", l)
        if (l ~ /^--/) next
        if (l ~ /^\/-/) { if (l !~ /-\//) blok = 1; next }
        if (l ~ /(^|[^A-Za-z_])(sorry|admit)([^A-Za-z_]|$)/) print FNR": "l
      }' "$1"
}

dosya=0; kotu=0
while IFS= read -r f; do
    dosya=$((dosya+1))
    cikti=$(borc_satirlari "$f")
    if [ -n "$cikti" ]; then
        echo "  🔴 $f"
        echo "$cikti" | head -3 | sed 's/^/     /'
        kotu=$((kotu+1))
    fi
done < <(find "$DIZIN" -name "*.lean" | sort)

if [ "$dosya" -eq 0 ]; then
    echo "🔴 HATA: hiç .lean dosyası bulunamadı — kapı boşa koştu"; exit 1
fi
if [ "$kotu" -ne 0 ]; then
    echo "=== Lean ispat borcu: $kotu/$dosya dosyada AÇIK sorry/admit ==="
    exit 1
fi
# ============================================================================
# [D-548] KULLANILMAYAN `require` KORUYUCUSU — D-529'un onarımını SABİTLER.
# ----------------------------------------------------------------------------
# ÖLÇÜLEN TARİH: `lakefile.lean` bildirim artığı bir `require mathlib` taşıyordu
# ve HİÇBİR dosya Mathlib'i import etmiyordu. Sonuç: `lake build` mathlib4'ü
# klonlamaya çalışıp 11 DAKİKA sonra düşüyordu → ispatlar AYLARCA hiç
# tip-denetlenmedi ve bunu hiçbir kapı görmedi ("0 sorry" bir tip denetimi
# DEĞİLDİR).
#
# ⚠ KURAL "hiç `require` olmasın" DEĞİL — o, meşru bir bağımlılığı da
#   yasaklardı. Kural D-529'un kendi cümlesidir: bir `require` ancak GERÇEK bir
#   kullanıma dayanıyorsa meşrudur. Yani `require X` varsa, en az bir `.lean`
#   dosyası `import X...` etmelidir; etmiyorsa bu bir BİLDİRİM ARTIĞIDIR.
#
# ⚠ NEDEN BU KAPIDA: `lean_sorry` `test_tumu`da HER KOŞUMDA çalışır;
#   `calistir_lean_tam` ise opt-in ve bu ortamda lean/lake YOK (ölçüldü:
#   WSL'de `command -v lake lean elan` → hiçbiri). Yani gerçek derleme kapısı
#   burada ATEŞLENEMEZ; ateşlenebilen tek şey, onu KOŞULAMAZ kılan kök
#   nedenin geri gelmediğini denetlemektir. Ayrı kapı açmak envanteri
#   gereksiz bölerdi (D-514/D-546 deseni).
LAKEFILE=$(find "$DIZIN" -name "lakefile.lean" -not -path "*/.lake/*" | head -1)
if [ -n "$LAKEFILE" ]; then
    req=$(grep -oE "^[[:space:]]*require[[:space:]]+[A-Za-z0-9_]+" "$LAKEFILE"           | awk '{print $2}' | sort -u)
    for r in $req; do
        # Lean import adı büyük harfle başlar: `require mathlib` -> `import Mathlib`
        buyuk=$(printf '%s' "$r" | cut -c1 | tr 'a-z' 'A-Z')$(printf '%s' "$r" | cut -c2-)
        if ! grep -rqE "^[[:space:]]*import[[:space:]]+$buyuk"                --include="*.lean" "$DIZIN" 2>/dev/null; then
            echo "🔴 $LAKEFILE: 'require $r' var ama HİÇBİR dosya 'import $buyuk' etmiyor."
            echo "   Bu bir BİLDİRİM ARTIĞIDIR ve 'lake build'i koşulamaz kılar (D-529)."
            echo "   Ya gerçek bir kullanım ekle ya da satırı KALDIR."
            exit 1
        fi
    done
fi

# [D-529] Uyarı GÜNCELLENDİ: `lake build` artık koşulabiliyor ama BU kapıda
# değil — ayrı ve OPT-IN hedef `calistir_lean_tam`da (test_tumu'ya bağlı değil;
# lean/lake bu depoda Windows'ta kurulu, takım WSL'de koşuyor).
# Bu kapı yalnız SAYAR; tip denetimi ötekinin işidir. İkisi birbirini tamamlar:
# `sorry`suz ama DERLENMEYEN bir dosya hiçbir şey kanıtlamaz.
echo "=== Lean ispat borcu: $dosya dosya, 0 sorry/admit; lakefile 'require' artigi YOK ==="
echo "    (GERÇEK tip denetimi ELLE: make calistir_lean_tam — lean/lake bu ortamda YOK)"
