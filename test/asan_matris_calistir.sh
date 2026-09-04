#!/usr/bin/env bash
# ============================================================================
# asan_matris_calistir.sh — KEMGU codegen bellek güvenliği MATRİSİ (kalıcı)
# ----------------------------------------------------------------------------
# D-029 (yapı alan-adı çözümü) + D-030 (dizi_olustur element_byte heap-overflow)
# hatalarının yaşadığı EKSENLERİ sınır-noktalarında zorlayan temsili program seti.
# Her program kendini doğrular (başarı = exit 42). Hem SANITIZER'SIZ (değer
# doğruluğu) hem ASan/UBSan altında (bellek güvenliği) çalıştırılır.
#
# Eksenler (test/asan_matris/m*.kem):
#   - Eleman tipi: tam8/16/32 (4-byte) vs tam64/metin/&T (8-byte) sınırı
#   - Kapasite: olustur(N)+ekle yarı-kapasite üstü (D-030) + realloc büyüme
#   - İşlemler: dizi_ekle / dizi_al / dizi_yaz (D-025 in-place) / dizi_boyut
#   - Yapı: karışık eleman-byte koleksiyonlar · aynı alan adı farklı tip · &Yapi param
#
# Kullanım:  bash test/asan_matris_calistir.sh   (veya: make calistir_asan_matris)
# Çıkış: 0 = tüm matris hem değer-doğru (42) hem ASan/UBSan-temiz; 1 = ihlal.
# ============================================================================
set -u
# [D-471] ZAMAN ASIMI SART. Bu kapi GERCEK programlari calistiriyor ve
# zaman asimi YOKTU: bloklanan tek bir program kapiyi SONSUZA DEK asar.
# Bu sinifin UCUNCU ornegi (ag_kosum D-466, codegen_genis D-468, burasi).
# Bu oturumda tam takim IKI KEZ saatlerce asili kaldi (2.5 sa ve 1.5 sa).
# ASan altinda program yavastir -> sinir cömert (60 sn), ama SONSUZ DEGIL.
# Asilan kapi, sessiz kapi kadar kotudur: kimse onu kosturmaz.
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-./build/kemgu${EXE}}
RT_OBJ="build/kdl_runtime.o build/kdl_runtime_mmio.o"
RT_SRC="runtime/kdl_runtime.c runtime/kdl_runtime_mmio.c"
DIR="test/asan_matris"
# [D-484] ASLR ENTROPISI — ASan ikilisi SESSIZCE COKER (exit 139, rapor YOK).
# OLCULDU (WSL, m04_ref_buyume): setarch'siz 15 kosumda 5 cokme, `setarch -R`
# ile 15/15 temiz. Cikti tamamen sessiz -- ASan HICBIR SEY basmiyor, yani
# "ihlal=0" oldugu halde kapi kirmizi olur ve kok gorunmez.
# D-478 bu duzeltmeyi Makefile'in KOSUM SATIRLARINA uygulamisti; kendi ASan
# ikilisini KURAN VE KOSTURAN harness'lar o kapsamin DISINDA kalmis.
# ⚠ `command -v setarch` YETMEZ: bazi cekirdeklerde setarch VAR ama -R
# BASARISIZ olur -> gercekten CALISTIGINI olc.
ASAN_RUN=""
if setarch -R true >/dev/null 2>&1; then ASAN_RUN="setarch -R"; fi
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/asan_matris.XXXXXX" 2>/dev/null || echo "build/asan_matris.$$")
mkdir -p "$TMP"
pass=0; fail=0

for f in "$DIR"/m*.kem; do
    b=$(basename "$f" .kem)
    if ! "$KEMGU" --check "$f" >/dev/null 2>&1; then
        echo "  ✗ $b: --check HATA"; fail=$((fail+1)); continue
    fi
    if ! "$KEMGU" --llvm "$f" > "$TMP/m.ll" 2>/dev/null; then
        echo "  ✗ $b: --llvm HATA"; fail=$((fail+1)); continue
    fi
    # 1) Sanitizer'sız: değer doğruluğu (exit 42)
    clang -x ir "$TMP/m.ll" -x none $RT_OBJ -o "$TMP/n.exe" 2>/dev/null
    timeout 60 "$TMP/n.exe" >/dev/null 2>&1; nrc=$?
    # 2) ASan/UBSan: bellek güvenliği (exit 42 + 0 ihlal)
    clang -fsanitize=address,undefined -x ir "$TMP/m.ll" -x none $RT_SRC -o "$TMP/a.exe" 2>/dev/null
    aout=$($ASAN_RUN timeout 60 "$TMP/a.exe" 2>&1); arc=$?
    viol=$(echo "$aout" | grep -ciE "AddressSanitizer|runtime error|SUMMARY:.*[Ss]anitizer")
    if [ "$nrc" = "42" ] && [ "$arc" = "42" ] && [ "$viol" = "0" ]; then
        echo "  ✓ $b"; pass=$((pass+1))
    else
        echo "  ✗ $b: normal=$nrc asan=$arc ihlal=$viol"
        echo "$aout" | grep -iE "ERROR|overflow|misalign|SUMMARY" | head -2
        fail=$((fail+1))
    fi
done
echo "=== ASan matris: $pass/$((pass+fail)) (değer-doğru + ASan/UBSan-temiz) ==="
[ "$fail" -eq 0 ]
