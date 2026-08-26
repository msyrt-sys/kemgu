#!/usr/bin/env bash
# ============================================================================
# check_genis_harness.sh — KAPISIZ KALAN yüzeylerde `--check` paritesi (D-428).
# ----------------------------------------------------------------------------
# NEDEN VAR: bu oturumda parite hataları defalarca ÖLÇÜLMEMİŞ yüzeyde bulundu.
# En sert örnek D-427: `drivers/virtio` hiçbir kapının altında değildi ve
# D-424'ün tip kapısı orada derlemeyi KIRDI — hiçbir kapı görmedi.
# Dizin sayımı yapılınca `test/snapshots` (82 dosya!) başta olmak üzere
# ALTI yüzeyin daha kapısız olduğu görüldü:
#   test/snapshots · test/ornekler/eski · test/stdlib · test/asan_matris ·
#   test/crossfile · stdlib/ (kök)
# Elle taranan her ölçüm er ya da geç eskir; kapı eskimez.
#
# Kapsam: `--check` paritesi (C `--checkdump` vs self `--check`) — tanı
# KODU + SATIR + SÜTUN birebir. Codegen/IR bu kapının işi DEĞİL (o yüzeyler
# `codegen_diff`/`codegen_genis`/`yapi_diff`e ait).
#
# Kullanım: bash test/check_genis_harness.sh (veya make calistir_check_genis)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/chkgenis); mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
    echo "ℹ codegen.exe yok — kapı atlandı (önce build/codegen.exe kurulmalı)."
    exit 0
fi

# ---- BİLİNEN SAPMA ENVANTERİ (ölçüldü, tahmin edilmedi) — hedefi KÜÇÜLMEKTİR
# (E1) C'nin KENDİ sınırlamaları — self daha müsamahakâr. Taklit etmek düşük
#      değerli ve yanlış-pozitif riski taşır:
#        21_modul_kullan      C=T002×2  modül-kapsamlı yapı alanı çözümü
#        23_generic_constraint C=T007   generic bound'da method çağrısı
#        49_generic_method    C=T001×2  `uygula Cift` (tip paramsız) + literal
# (E2) PARSER sapması: `tip Ad = HedefTip;` KEMGU'da YOK. C parser'ı P001 ile
#      reddeder, self parse edip T002/T011 verir → farklı KATMAN, ayrı iş.
#        tip_alias
# (E3) `eşleş` desen bağlamında kaçırma (self daha müsamahakâr): C `değer(v)` /
#      `hata(m)` kolunda T002/T011 veriyor, self vermiyor.
#        test_metin · test_sonuc · heap_dizi_metin · test_json · test_dosya
#      ⚠ `test_json` (D-436): bu dosyalar MODÜLLE BİRLEŞTİRİLEREK derlenmek
#      üzere yazılmıştır (bkz. `calistir_stdlib_check`); TEK BAŞINA ölçülünce
#      doğal olarak T002 dolu olurlar. Gerçek fark yalnız 4 satırdır ve hepsi
#      `hata(m) =>` bağlamasındaki `m` içindir — E3'ün ta kendisi (C 57, self
#      53 tanı). Modülle BİRLEŞTİRİLMİŞ hâli `stdlib_check`te TEMİZ geçer.
#      ⚠ `test_dosya` (D-443): AYNI sınıf, YENİ giren. O dosyaya gerçek I/O
#      gidiş-dönüşü eklenince `eşleş oku_metin(...)` kolları oluştu; skrutini
#      (modülsüz derlemede) tanımsız olduğu için C desen bağlamalarını HİÇ
#      kurmuyor ve `v`/`e` kullanımları T002 alıyor, self bağlayıp susuyor
#      (C 46, self 36 tanı — fark yalnız bu 10 kaskad satırı). Kusur DEĞİL,
#      hata-kaskadı derinliği farkı; birleştirilmiş hâli `stdlib_check`te
#      TEMİZ geçer.
#      D-461: test_regex — AYNI E3 sinifi. Tek basina `derle` tanimsizdir; C,
#      `hata(m) =>` kolundaki `m` icin IKI T002 basar (skrutini cozulemeyince
#      desen baglamalarini HIC kurmuyor), self-host baglayip susuyor -> fark
#      tam 2 kaskad satiri (C 14 / self 12), ikisi de satir 15 sutun 47.
#      Birlestirilmis hali calistir_stdlib_check'te TEMIZ gecer (C ve self: 0).
#      D-456: test_semafor / test_bariyer — test_kilit ile BIREBIR ayni sekil.
#      Tek basina (modulsuz) semaforda/bekle tanimsizdir; C, gorev_baslat(|| ..)
#      ATAMASINA ayrica T001 basar (kapanisin donus tipi cozulemiyor), self-host
#      basmaz -> fark tam 2 kaskad satiri (semafor C 10 / self 8; bariyer C 13 /
#      self 11). Fark satirlarinin HANGI satirlar oldugu OLCULDU: ikisinde de
#      "degisken gN: sonuc<gorev<tam32>, metin> =". Birlestirilmis halleri
#      calistir_stdlib_check'te TEMIZ gecer (C ve self: exit 0).
#
# ⚠⚠ MUAF DIZGISININ ICINE ACIKLAMA YAZMA (D-456'da isirdi). Bu bir cift
# tirnakli dizgidir: icine yazilan her CIPLAK KELIME bir muafiyet adi olur
# (aciklamama "bariyer" yazmistim -> stdlib/bariyer.kem sessizce muaf oldu) ve
# her BACKTICK komut ikamesi tetikler ("stdlib_check: command not found").
# Aciklamalar YALNIZ bu yorum blogunda; dizginin icinde SADECE dosya adlari.
#      D-455: test_kilit — AYNI sinif. Tek basina (modulsuz) kilitle/Kilit
#      tanimsizdir; C, gorev_baslat(|| kilitle(k, ..)) atamasina AYRICA T001
#      basar (kapanisin donus tipi cozulemiyor), self-host basmaz -> fark tam
#      2 kaskad satiri (C 10, self 8 tani). Birlestirilmis hali
#      calistir_stdlib_check'te TEMIZ gecer (C ve self: exit 0).
MUAF="21_modul_kullan 23_generic_constraint 49_generic_method
tip_alias test_metin test_sonuc heap_dizi_metin test_json test_dosya test_kilit
test_semafor test_bariyer test_regex"
muaf_mi() {
    for m in $MUAF; do [ "$m" = "$1" ] && return 0; done
    return 1
}

pass=0; fail=0; muaf=0
for f in test/snapshots/*.kem test/ornekler/eski/*.kem test/stdlib/*.kem \
         test/asan_matris/*.kem test/crossfile/*.kem stdlib/*.kem; do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)

    "$KEMGU" --checkdump "$f" > "$TMP/c.chk" 2>/dev/null
    "$CODEGEN" --check "$f" > "$TMP/s.chk" 2>/dev/null

    if diff -q "$TMP/c.chk" "$TMP/s.chk" >/dev/null 2>&1; then
        # Muaf dosya ARTIK EŞLEŞİYORSA listeden çıkarılmalıdır.
        if muaf_mi "$b"; then
            echo "  ⚠ $b — MUAF ama artık EŞLEŞİYOR: muafiyet listesinden ÇIKAR."
        fi
        pass=$((pass+1))
    elif muaf_mi "$b"; then
        muaf=$((muaf+1))
    else
        echo "  🔴 $b — --check farkı (C --checkdump vs KEMGU --check):"
        diff "$TMP/c.chk" "$TMP/s.chk" 2>/dev/null | head -6 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done
echo "=== geniş --check paritesi: $pass/$((pass+fail)) dosya ($muaf muaf) ==="
[ "$fail" -eq 0 ]
