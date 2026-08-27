#!/usr/bin/env bash
# ============================================================================
# baremetal_diff_harness.sh — BARE-METAL / ARM64 codegen kapısı (D-418).
# ----------------------------------------------------------------------------
# NEDEN AYRI BİR KAPI: `runtime/*.kem` (OS/sürücü kodu) host'ta LİNKLENMEZ —
# aarch64 sysreg/MMIO içerir. Exit-kodu karşılaştırması imkânsız, bu yüzden
# davranışsal kapıların HEPSİ bu yüzeyi atlıyordu ve **self-host `--mimari`
# bayrağını hiç tanımadığı için bare-metal kodu DERLEYEMİYORDU BİLE** — bunu
# hiçbir kapı görmüyordu (ölçüldü: 0 define üretiyordu).
#
# Linkleyemediğimiz için DAVRANIŞ değil YAPI karşılaştırılır: emit edilen
# `define` kümesi (ad + DÖNÜŞ TİPİ) C oracle ile birebir mi. Bu, eksik işlevi,
# yanlış adı ve yanlış dönüş tipini yakalar — D-418'de üçü de gerçekten çıktı
# (void işlevler `i32` olarak yayılıyordu).
#
# ⚠ Dönüş tipini KESMEYİN: `sed 's/(.*//'` imzayı atar ama `define void @f` ile
# `define i32 @f` ayrımını KORUR — kusur tam da oradaydı.
#
# Kullanım: bash test/baremetal_diff_harness.sh (veya make calistir_baremetal_diff)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/bmdiff); mkdir -p "$TMP"

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

pass=0; fail=0; atla=0

# ---- (1) BİRLEŞİK OS BİRİMİ — asıl kapsam ----
# runtime/*.kem dosyalarının ÇOĞU tek başına derlenmez (birbirlerinin sembollerine
# bakarlar; C oracle T002 verir) → dosya-dosya döngü onların 6'sını ATLIYOR.
# GERÇEK derleme birimi Makefile'ın kurduğu birleşiktir (bkz. kem_os_comb.kem):
# tüm runtime + kem_os.kem. ~4166 satır, 240 işlev, 44 satıriçi asm bloğu.
# Bu adım o birimi bir bütün olarak ölçer — yoksa OS'un 2/3'ü kapısız kalır.
OS_PARCA="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem \
runtime/kem_zaman.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem \
test/ornekler/kem_os.kem"
os_var=1
for parca in $OS_PARCA; do [ -f "$parca" ] || os_var=0; done
if [ "$os_var" -eq 1 ]; then
    cat $OS_PARCA > "$TMP/os.kem"
    "$KEMGU" --llvm --mimari aarch64 "$TMP/os.kem" > "$TMP/os_c.ll" 2>/dev/null
    "$CODEGEN" --llvm --mimari aarch64 "$TMP/os.kem" > "$TMP/os_s.ll" 2>/dev/null
    if ! head -1 "$TMP/os_c.ll" 2>/dev/null | grep -q "hata\[" && [ -s "$TMP/os_c.ll" ]; then
        grep "^define" "$TMP/os_c.ll" | sed 's/(.*//' | sort > "$TMP/os_c.d"
        grep "^define" "$TMP/os_s.ll" | sed 's/(.*//' | sort > "$TMP/os_s.d"
        oc=$(grep -m1 "target triple" "$TMP/os_c.ll"); os=$(grep -m1 "target triple" "$TMP/os_s.ll")
        # satıriçi asm SAYISI da denetlenir: asm bloğu sessizce düşerse (D-416'nın
        # kusuru) define kümesi DEĞİŞMEZ — yalnız ad karşılaştırmak onu KAÇIRIR.
        ca=$(grep -c "asm sideeffect" "$TMP/os_c.ll"); sa=$(grep -c "asm sideeffect" "$TMP/os_s.ll")
        if [ "$oc" = "$os" ] && diff -q "$TMP/os_c.d" "$TMP/os_s.d" >/dev/null 2>&1 && [ "$ca" -eq "$sa" ]; then
            echo "  ✅ BİRLEŞİK OS — $(wc -l <"$TMP/os_c.d") işlev, $ca satıriçi asm, üçlü eşleşti"
            pass=$((pass+1))
        else
            echo "  🔴 BİRLEŞİK OS — işlev C=$(wc -l <"$TMP/os_c.d") KEMGU=$(wc -l <"$TMP/os_s.d") · asm C=$ca KEMGU=$sa"
            [ "$oc" = "$os" ] || echo "      üçlü: C=[$oc] KEMGU=[$os]"
            diff "$TMP/os_c.d" "$TMP/os_s.d" 2>/dev/null | head -5 | sed 's/^/      /'
            fail=$((fail+1))
        fi
    else
        atla=$((atla+1))
    fi
fi

# ---- (2) TEK TEK derlenebilen runtime dosyaları ----
for f in runtime/*.kem; do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)

    # Oracle IR üretemiyorsa (bu kapının işi olmayan bir sebep) atla.
    "$KEMGU" --llvm --mimari aarch64 "$f" > "$TMP/c.ll" 2>/dev/null || { atla=$((atla+1)); continue; }
    head -1 "$TMP/c.ll" 2>/dev/null | grep -q "hata\[" && { atla=$((atla+1)); continue; }
    [ -s "$TMP/c.ll" ] || { atla=$((atla+1)); continue; }

    "$CODEGEN" --llvm --mimari aarch64 "$f" > "$TMP/s.ll" 2>/dev/null || {
        echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue; }

    # Hedef üçlüsü de karşılaştırılır: --mimari yok sayılırsa burası yakalar.
    ct=$(grep -m1 "target triple" "$TMP/c.ll")
    st=$(grep -m1 "target triple" "$TMP/s.ll")
    if [ "$ct" != "$st" ]; then
        echo "  🔴 $b — hedef üçlüsü farklı: C=[$ct] KEMGU=[$st]"; fail=$((fail+1)); continue
    fi

    grep "^define" "$TMP/c.ll" | sed 's/(.*//' | sort > "$TMP/c.d"
    grep "^define" "$TMP/s.ll" | sed 's/(.*//' | sort > "$TMP/s.d"
    if diff -q "$TMP/c.d" "$TMP/s.d" >/dev/null 2>&1; then
        pass=$((pass+1))
    else
        echo "  🔴 $b — define kümesi farklı (C=$(wc -l <"$TMP/c.d") KEMGU=$(wc -l <"$TMP/s.d")):"
        diff "$TMP/c.d" "$TMP/s.d" 2>/dev/null | head -5 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done
echo "=== bare-metal (aarch64) yapı paritesi: $pass/$((pass+fail)) birim ($atla atlandı) ==="
[ "$fail" -eq 0 ]
