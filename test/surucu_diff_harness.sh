#!/usr/bin/env bash
# ============================================================================
# surucu_diff_harness.sh — VirtIO SÜRÜCÜ yüzeyi parite kapısı (D-427).
# ----------------------------------------------------------------------------
# NEDEN VAR — BİR REGRESYON GÖNDERDİM VE HİÇBİR KAPI GÖRMEDİ:
# `drivers/virtio` (10) + `tests/drivers/virtio` (9) = 19 dosya HİÇBİR kapının
# altında değildi. D-424'ün `--llvm` tip kapısı eklenince self-host bu dosyaları
# DERLEYEMEZ oldu (C 11/63/68 define, self 0) — çünkü legacy çok-segmentli
# `kullan a::b::c;` importunda sahte T002 üretiyordu.
#
# D-424'ün ön koşulunu 502 dosyada ölçüp "yanlış-pozitif SIFIR" demiştim; o
# listede `drivers/` YOKTU. Kendi dersimi ("parite sayısı yalnız ölçülen yüzey
# kadar geniştir") kendi kapıma uygulamayı atlamışım.
#
# Kapı İKİ şeyi ölçer:
#   1) `--check` paritesi (C --checkdump vs self --check) — tanı kodu+satır+sütun
#   2) `--llvm` YAPISAL paritesi — oracle IR üretebiliyorsa `define` kümesi
#      (ad + DÖNÜŞ TİPİ). Sürücüler host'ta LİNKLENMEZ (MMIO) → davranış
#      ölçülemez, yapı ölçülür (`baremetal_diff` deseni).
#
# Kullanım: bash test/surucu_diff_harness.sh (veya make calistir_surucu_diff)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/surdiff.XXXXXX" 2>/dev/null || echo "build/surdiff.$$")
mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
# [D-486] EKSIK IKILI = HATA, ATLAMA DEGIL. Bu kapinin make hedefi
# `$(BUILD)/codegen$(EXE)`e BAGIMLIDIR -> make uzerinden ikili GARANTI.
# Yani bu dal make yolunda OLU KOD; tek islevi bir YOL HATASINI SESSIZCE
# YUTMAKTI. Olculdu: Makefile `CODEGEN=build/codegen.exe` diye SABIT
# geciyordu -> Linux'ta ikilinin adi `build/codegen` -> SEKIZ parite
# kapisi birden atlandi ve `make` yine 0 dondu (tam takim YESIL gorundu).
# D-446'nin sinifi: var olan ama kosmayan kapi, olmayandan tehlikelidir.
    echo "🔴 HATA: codegen ikilisi YOK ($CODEGEN) — kapı KOŞMADI"
    exit 1
fi

# ---- BİLİNEN SAPMA ENVANTERİ — hedefi KÜÇÜLMEKTİR ----
# ⚠ BU LİSTE ÖLÇÜLEREK YAZILDI, TAHMİN EDİLMEDİ. İlk hâlinde 2 dosya vardı;
# dosya-başına tanı KODLARINI çıkarınca 7 olduğu görüldü (D-406 dersi:
# "muafiyet listesine yazdığın gerekçe de bir İDDİADIR — ÖLÇ").
#
# ✓ D-429: YANLIŞ-POZİTİFLERİN TAMAMI KAPANDI. Çapraz-dosya İMZA + YAPI
#   kayıtları artık taşınıyor (`capraz_imza_tasi`) → sahte CP005 (parametre
#   lineerliği bilinmiyordu) ve sahte T002 (`Virtqueue`/`BlkYapilandirma` yapı
#   kayıtları yoktu) GİTTİ. `virtio_blk_config_test` ve `virtio_mmio_mock_test`
#   listeden ÇIKARILDI — 7 → 5.
#
# KALAN 5 — HEPSİ KAÇIRMA YÖNÜNDE (self daha müsamahakâr; derlemeyi KIRMAZ).
# ⚠ KÖKLERİ D-430'da TEK TEK İZLENDİ; ilk kaydettiğim gerekçe ("çeşit
#   varyantları taşınmıyor") YANLIŞTI ve `cv_*` taşıması hiçbir şeyi
#   değiştirmedi (yazıldı, ölçüldü, GERİ ALINDI):
#     T011 → C `Virtqueue`yi TANIMIYOR ve T011 veriyor; self D-429'un yapı
#            taşımasıyla TANIYOR. Yani self burada C'den DAHA YETENEKLİ —
#            eşleşmek YETENEK SİLMEK olurdu.
#     M001 → skrutini `sonuç<T,H>`, `çeşit` DEĞİL. Ayrıca ölçüldü:
#            **C çapraz-dosya kapsayıcılık HİÇ yapmıyor** (`dışa çeşit` +
#            eksik varyant → C = OK), yani parite bunu gerektirmiyor.
#     T001/T020 → bileşik tip temsilinin çapraz-dosya sınırları.
# ⚠ `alan_tn`/`fn_ptn` düğüm indeksleri taşınamaz (`-1`) — muhafazakâr tarafa
#   düşerler, yani tanı KAÇIRIRLAR, sahte tanı ÜRETMEZLER.
MUAF="virtio_blk virtio_blk_oku virtio_blk_init_test
virtio_blk_oku_test virtqueue_bind_test"
muaf_mi() {
    for m in $MUAF; do [ "$m" = "$1" ] && return 0; done
    return 1
}

pass=0; fail=0; muaf=0; atla=0
for f in drivers/virtio/*.kem tests/drivers/virtio/*.kem; do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)

    "$KEMGU" --checkdump "$f" > "$TMP/c.chk" 2>/dev/null
    "$CODEGEN" --check "$f" > "$TMP/s.chk" 2>/dev/null
    if ! diff -q "$TMP/c.chk" "$TMP/s.chk" >/dev/null 2>&1; then
        if muaf_mi "$b"; then muaf=$((muaf+1)); continue; fi
        echo "  🔴 $b — --check farkı:"
        diff "$TMP/c.chk" "$TMP/s.chk" 2>/dev/null | head -4 | sed 's/^/      /'
        fail=$((fail+1)); continue
    fi
    if muaf_mi "$b"; then
        echo "  ⚠ $b — MUAF ama --check artık EŞLEŞİYOR: muafiyet listesinden ÇIKAR."
    fi

    # --llvm YAPISAL parite (oracle IR üretemiyorsa tip hatasıdır → self de
    # reddetmeli; D-424'ün pozitif-iddia deseni).
    if ! "$KEMGU" --llvm "$f" > "$TMP/c.ll" 2>/dev/null; then
        if "$CODEGEN" --llvm "$f" > "$TMP/s.ll" 2>/dev/null; then
            echo "  🔴 $b — C tip hatasıyla REDDEDİYOR, KEMGU IR ÜRETİYOR"
            fail=$((fail+1)); continue
        fi
        pass=$((pass+1)); continue
    fi
    grep -q "^define" "$TMP/c.ll" 2>/dev/null || { atla=$((atla+1)); continue; }

    "$CODEGEN" --llvm "$f" > "$TMP/s.ll" 2>/dev/null || {
        echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue; }

    # ⚠ BURADA DÖNÜŞ TİPİ KARŞILAŞTIRILMAZ — yalnız işlev ADLARI.
    # Ölçüldü: bu yüzeydeki define SAYILARI zaten eşleşiyor (C=16 KEMGU=16),
    # ayrışan tek şey `mantıksal` dönüşü (C `i1`, self `i32`) = `yapi_diff`in
    # K1 kökü. O bilinen sapmayı BURADA ikinci kez muaf yazmak envanteri
    # bölerdi; K1'in tek sahibi `yapi_diff`tir.
    # Bu kapının işi: işlev EKSİLMESİ/FAZLALIĞI ve "hiç IR üretmeme" — yani
    # D-427'nin onardığı regresyon sınıfı (self 0 define üretiyordu).
    grep "^define" "$TMP/c.ll" | sed 's/(.*//' | sed 's/^define [^ ]* //' | sort > "$TMP/c.d"
    grep "^define" "$TMP/s.ll" | sed 's/(.*//' | sed 's/^define [^ ]* //' | sort > "$TMP/s.d"
    if diff -q "$TMP/c.d" "$TMP/s.d" >/dev/null 2>&1; then
        pass=$((pass+1))
    else
        echo "  🔴 $b — işlev ADI kümesi farklı (C=$(wc -l <"$TMP/c.d") KEMGU=$(wc -l <"$TMP/s.d")):"
        diff "$TMP/c.d" "$TMP/s.d" 2>/dev/null | head -4 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done
echo "=== sürücü paritesi (--check + yapısal --llvm): $pass/$((pass+fail)) ($muaf muaf, $atla atlandı) ==="
[ "$fail" -eq 0 ]
