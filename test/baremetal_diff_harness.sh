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
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/bmdiff.XXXXXX" 2>/dev/null || echo "build/bmdiff.$$")
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

pass=0; fail=0
# ============================================================================
# [D-675] "ATLANDI" KALDIRILDI — derlenemeyen birim KIRMIZIDIR.
# Ölçüldü (2026-10-12, onarım ÖNCESİ): birleşik OS biriminden bir parça
# düşürülünce (T002) ya da bir parça dosyasına tip hatası sokulunca kapı
# `rc=0` + "2/2 birim (9 atlandı)" verdi — çekirdeğin TAMAMI ölçülmeden YEŞİL.
# ZERO-C audit'teki "pure-.kem" körlüğüyle aynı sınıf: kapı geçiyor ama hiçbir
# şey ölçmüyor (D-446/D-486). Yeni kurallar:
#   (1) birleşik OS birimi derlenemezse / parçası eksikse → KIRMIZI
#   (2) BIRIMLER tablosundaki her birim (dosya + mimari bağımlılıkları)
#       derlenemezse → KIRMIZI
#   (3) PARCALAR: tek başına derlenemeyen dosyalar — her birinin TÜM işlev ve
#       küresel sembolleri birleşik birimin C VE KEMGU IR'ında bulunmalı
#       (SEMBOL SAYIMI, iddia değil). Eksik → KIRMIZI. Parça tek başına
#       derleniyorsa → KIRMIZI (bayat sınıflandırma, D-534: BIRIMLER'e taşı).
#   (4) runtime/*.kem + runtime/mimari/arm64/*.kem içindeki HER dosya (2) ya
#       da (3)'te olmalı; sınıflandırılmamış dosya → KIRMIZI.
# Gerekçeler: docs/BAREMETAL_DIFF_KAPSAM.md
# ============================================================================

# ---- (1) BİRLEŞİK OS BİRİMİ — asıl kapsam ----
# Makefile `calistir_kem_os_arm` cat sırasıyla AYNI olmalı.
OS_PARCA="runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/arm64/kem_gorev_hw.kem \
runtime/kem_zaman.kem runtime/mimari/arm64/kem_zaman_hw.kem runtime/mimari/arm64/kem_bariyer.kem runtime/mimari/arm64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/mimari/arm64/kem_kesif.kem runtime/kem_dtb.kem \
test/ornekler/kem_os_algoritma.kem test/ornekler/kem_os_cekirdek_ortak.kem test/ornekler/kem_os.kem"
os_ok=0
os_eksik=""
for parca in $OS_PARCA; do [ -f "$parca" ] || os_eksik="$os_eksik $parca"; done
if [ -n "$os_eksik" ]; then
    echo "  🔴 BİRLEŞİK OS — parça dosyası YOK:$os_eksik"
    fail=$((fail+1))
else
    cat $OS_PARCA > "$TMP/os.kem"
    "$KEMGU" --llvm --mimari aarch64 "$TMP/os.kem" > "$TMP/os_c.ll" 2> "$TMP/os_c.err"; orc=$?
    "$CODEGEN" --llvm --mimari aarch64 "$TMP/os.kem" > "$TMP/os_s.ll" 2>/dev/null
    if [ "$orc" -ne 0 ] || head -1 "$TMP/os_c.ll" 2>/dev/null | grep -q "hata\[" || [ ! -s "$TMP/os_c.ll" ]; then
        echo "  🔴 BİRLEŞİK OS — C oracle DERLEYEMEDİ (rc=$orc) — birim ÖLÇÜLEMEDİ:"
        grep -m3 "hata\[" "$TMP/os_c.err" "$TMP/os_c.ll" 2>/dev/null | sed 's/^/      /'
        fail=$((fail+1))
    elif [ ! -s "$TMP/os_s.ll" ]; then
        echo "  🔴 BİRLEŞİK OS — KEMGU codegen IR üretemedi"
        fail=$((fail+1))
    else
        grep "^define" "$TMP/os_c.ll" | sed 's/(.*//' | sort > "$TMP/os_c.d"
        grep "^define" "$TMP/os_s.ll" | sed 's/(.*//' | sort > "$TMP/os_s.d"
        oc=$(grep -m1 "target triple" "$TMP/os_c.ll"); os=$(grep -m1 "target triple" "$TMP/os_s.ll")
        # satıriçi asm SAYISI da denetlenir: asm bloğu sessizce düşerse (D-416'nın
        # kusuru) define kümesi DEĞİŞMEZ — yalnız ad karşılaştırmak onu KAÇIRIR.
        ca=$(grep -c "asm sideeffect" "$TMP/os_c.ll"); sa=$(grep -c "asm sideeffect" "$TMP/os_s.ll")
        # [D-604] HAM İŞARETÇİ + VOLATILE paritesi. Eşitlik tek başına YETMEZ:
        # iki derleyici birlikte volatile'ı bıraksa 0=0 geçerdi → C > 0 da şart.
        cv=$(grep -cE "load volatile|store volatile" "$TMP/os_c.ll"); sv=$(grep -cE "load volatile|store volatile" "$TMP/os_s.ll")
        ci=$(grep -c "inttoptr" "$TMP/os_c.ll"); si=$(grep -c "inttoptr" "$TMP/os_s.ll")
        # [D-704] noredzone YALNIZ bare-metal x86'ya: aarch64 biriminde iki derleyicide de 0.
        cz=$(grep -c ") noredzone {" "$TMP/os_c.ll"); sz=$(grep -c ") noredzone {" "$TMP/os_s.ll")
        if [ "$oc" = "$os" ] && diff -q "$TMP/os_c.d" "$TMP/os_s.d" >/dev/null 2>&1 && [ "$ca" -eq "$sa" ] \
           && [ "$cv" -gt 0 ] && [ "$cv" -eq "$sv" ] && [ "$ci" -gt 0 ] && [ "$ci" -eq "$si" ] \
           && [ "$cz" -eq 0 ] && [ "$sz" -eq 0 ]; then
            echo "  ✅ BİRLEŞİK OS — $(wc -l <"$TMP/os_c.d") işlev, $ca satıriçi asm, $cv volatile, $ci inttoptr, üçlü eşleşti"
            pass=$((pass+1)); os_ok=1
        else
            echo "  🔴 BİRLEŞİK OS — işlev C=$(wc -l <"$TMP/os_c.d") KEMGU=$(wc -l <"$TMP/os_s.d") · asm C=$ca KEMGU=$sa · volatile C=$cv KEMGU=$sv · inttoptr C=$ci KEMGU=$si · noredzone C=$cz KEMGU=$sz (0 olmalı)"
            [ "$oc" = "$os" ] || echo "      üçlü: C=[$oc] KEMGU=[$os]"
            diff "$TMP/os_c.d" "$TMP/os_s.d" 2>/dev/null | head -5 | sed 's/^/      /'
            fail=$((fail+1))
        fi
    fi
fi

# ---- (2) BİRİMLER — tek başına ya da MİMARİ BAĞIMLILIKLARIYLA derlenen ----
# Biçim: ad|ilk dosya birimin SAHİBİ, sonrakiler aarch64 bağımlılıkları.
# kem_heap ve kem_virtio_blk D-671/D-672'de mimari başına dosyalara (D2b)
# bağlandığı için TEK BAŞINA derlenmez oldu; bağımlılıklarıyla birim olarak
# GERİ ALINDI (onarım öncesi sessizce "atlandı"ya düşmüşlerdi).
# [D-663] PER-BİRİM ZAMAN AŞIMI + BELLEK TAVANI (G15) — self-host ikilisi korpus
# üzerinde koşar; `timeout` yeteneği ÖLÇÜLÜR (D-661).
KAP_SN=${KAP_SN:-60}
KAP_KB=${KAP_KB:-4000000}
if command -v timeout >/dev/null 2>&1 && timeout 5 true >/dev/null 2>&1; then
    GTO="timeout $KAP_SN"
else
    GTO=""
    echo "  ⚠ 'timeout' YOK -> per-birim zaman asimi UYGULANMIYOR (kapi yine olcer). Bkz. D-663."
fi
ir_uret() { o="$1"; shift; ( ulimit -v "$KAP_KB" 2>/dev/null; $GTO "$@" > "$o" 2>/dev/null ); }

BIRIMLER="kem_dtb|runtime/kem_dtb.kem runtime/mimari/arm64/kem_kesif.kem
kem_kesif|runtime/mimari/arm64/kem_kesif.kem
kem_kanal_abi|runtime/kem_kanal_abi.kem
kem_os_algoritma|test/ornekler/kem_os_algoritma.kem
kem_heap|runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem
kem_bariyer|runtime/mimari/arm64/kem_bariyer.kem
kem_virtio_blk|runtime/kem_virtio_blk.kem runtime/mimari/arm64/kem_bariyer.kem runtime/mimari/arm64/kem_platform.kem"

birim_sahipleri=""
while IFS='|' read -r b dosyalar; do
    [ -n "$b" ] || continue
    birim_sahipleri="$birim_sahipleri ${dosyalar%% *}"
    eksik=""; for d in $dosyalar; do [ -f "$d" ] || eksik="$eksik $d"; done
    if [ -n "$eksik" ]; then echo "  🔴 $b — dosya YOK:$eksik"; fail=$((fail+1)); continue; fi
    cat $dosyalar > "$TMP/$b.kem"
    "$KEMGU" --llvm --mimari aarch64 "$TMP/$b.kem" > "$TMP/c.ll" 2> "$TMP/c.err"; crc=$?
    if [ "$crc" -ne 0 ] || head -1 "$TMP/c.ll" 2>/dev/null | grep -q "hata\[" || [ ! -s "$TMP/c.ll" ]; then
        echo "  🔴 $b — C oracle DERLEYEMEDİ (rc=$crc) — birim ÖLÇÜLEMEDİ:"
        grep -m3 "hata\[" "$TMP/c.err" "$TMP/c.ll" 2>/dev/null | sed 's/^/      /'
        fail=$((fail+1)); continue
    fi
    ir_uret "$TMP/s.ll" "$CODEGEN" --llvm --mimari aarch64 "$TMP/$b.kem"; sir=$?
    if [ "$sir" -eq 124 ] || [ "$sir" -eq 137 ]; then
        echo "  🔴 $b — KEMGU IR URETIMI ASILDI/OLDURULDU (rc=$sir)"; fail=$((fail+1)); continue
    fi
    [ -s "$TMP/s.ll" ] || { echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue; }
    # Hedef üçlüsü de karşılaştırılır: --mimari yok sayılırsa burası yakalar.
    ct=$(grep -m1 "target triple" "$TMP/c.ll"); st=$(grep -m1 "target triple" "$TMP/s.ll")
    if [ "$ct" != "$st" ]; then
        echo "  🔴 $b — hedef üçlüsü farklı: C=[$ct] KEMGU=[$st]"; fail=$((fail+1)); continue
    fi
    grep "^define" "$TMP/c.ll" | sed 's/(.*//' | sort > "$TMP/c.d"
    grep "^define" "$TMP/s.ll" | sed 's/(.*//' | sort > "$TMP/s.d"
    if diff -q "$TMP/c.d" "$TMP/s.d" >/dev/null 2>&1; then
        echo "  ✅ $b — $(wc -l <"$TMP/c.d") işlev ($(echo $dosyalar | wc -w) dosya)"
        pass=$((pass+1))
    else
        echo "  🔴 $b — define kümesi farklı (C=$(wc -l <"$TMP/c.d") KEMGU=$(wc -l <"$TMP/s.d")):"
        diff "$TMP/c.d" "$TMP/s.d" 2>/dev/null | head -5 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done <<EOF
$BIRIMLER
EOF

# ---- (3) PARÇALAR — tek başına derlenemeyen; kapsam BİRLEŞİK birimde ÖLÇÜLÜR ----
# Her parçanın dosyadan okunan TÜM `işlev` ve `küresel değişken` adları
# birleşik birimin C VE KEMGU IR'ında tanımlı olmalı (`define ... @ad(` /
# `@ad = `). `sabit`ler IR'da sembol bırakmaz (satır içi) → sayılamaz.
# Gerekçeler (neden tek başına derlenemiyor): docs/BAREMETAL_DIFF_KAPSAM.md
PARCALAR="runtime/kem_elf.kem runtime/kem_gorev.kem runtime/kem_minifs.kem
runtime/kem_mmu.kem runtime/kem_virtio_net.kem runtime/kem_zaman.kem
runtime/mimari/arm64/kem_uart.kem runtime/mimari/arm64/kem_platform.kem
runtime/mimari/arm64/kem_zaman_hw.kem
runtime/kem_gorev_cekirdek.kem runtime/mimari/arm64/kem_gorev_hw.kem
test/ornekler/kem_os_cekirdek_ortak.kem test/ornekler/kem_os.kem"
kapsam_islev=0; kapsam_kuresel=0
for f in $PARCALAR; do
    b=$(basename "$f" .kem)
    [ -f "$f" ] || { echo "  🔴 parça $f — dosya YOK"; fail=$((fail+1)); continue; }
    # Bayat sınıflandırma: tek başına derleniyorsa parça DEĞİL → BIRIMLER'e taşı.
    if "$KEMGU" --llvm --mimari aarch64 "$f" > "$TMP/p.ll" 2>/dev/null && ! head -1 "$TMP/p.ll" | grep -q "hata\[" \
       && grep -q "^define" "$TMP/p.ll"; then
        echo "  🔴 parça $f — TEK BAŞINA DERLENİYOR: bayat sınıflandırma, BIRIMLER'e taşı (D-534)"
        fail=$((fail+1)); continue
    fi
    if [ "$os_ok" -ne 1 ]; then
        echo "  🔴 parça $f — birleşik birim ölçülemedi → kapsamı DOĞRULANAMADI"
        fail=$((fail+1)); continue
    fi
    adlar=$(grep -ohE '^\s*(çıplak\s+|genel\s+|gerçekzamanlı\s+)*işlev\s+[A-Za-z_][A-Za-z_0-9]*' "$f" | sed -E 's/.*işlev\s+//' | sort -u)
    kureseller=$(grep -ohE '^\s*küresel\s+değişken\s+[A-Za-z_][A-Za-z_0-9]*' "$f" | sed -E 's/.*değişken\s+//' | sort -u)
    n_i=$(printf '%s\n' "$adlar" | grep -c .); n_k=$(printf '%s\n' "$kureseller" | grep -c .)
    eksik=""
    for a in $adlar; do
        grep -qE "^define [^@]*@$a\(" "$TMP/os_c.ll" || eksik="$eksik C:@$a"
        grep -qE "^define [^@]*@$a\(" "$TMP/os_s.ll" || eksik="$eksik KEMGU:@$a"
    done
    for k in $kureseller; do
        grep -qE "^@$k = " "$TMP/os_c.ll" || eksik="$eksik C:@$k"
        grep -qE "^@$k = " "$TMP/os_s.ll" || eksik="$eksik KEMGU:@$k"
    done
    if [ $((n_i + n_k)) -eq 0 ]; then
        echo "  🔴 parça $f — ölçülecek sembol YOK (sayım 0 → kapsam iddiası BOŞ)"
        fail=$((fail+1))
    elif [ -n "$eksik" ]; then
        echo "  🔴 parça $f — birleşik birimde EKSİK:$(echo $eksik | cut -c1-200)"
        fail=$((fail+1))
    else
        echo "  ✅ parça $b — $n_i işlev + $n_k küresel birleşik birimde (C + KEMGU)"
        pass=$((pass+1)); kapsam_islev=$((kapsam_islev + n_i)); kapsam_kuresel=$((kapsam_kuresel + n_k))
    fi
done

# ---- (5) BİRLEŞİK x86 BİRİMİ — aarch64 (1)'in x86_64 ikizi ----
# NEDEN: D-674 sonrası x86 birleşik birimi (calistir_kem_os_x86 CAT) VAR ama
# C↔self YAPISAL paritesini ölçen kapı YOKTU — aarch64'ün (1)'i x86'da karşılık-
# sızdı (elle ölçüldü: 72 define, 4 asm, 79 volatile, 83 inttoptr, üçlü aynı;
# kapı sessiz). Var olan ama KOŞMAYAN kapı, olmayandan tehlikelidir (D-446/D-486).
# Parça listesi TEK KAYNAKTAN: Makefile'daki x86 CAT satırından OKUNUR. Elle
# kopyalanan liste kaçınılmaz KAYAR (D-534 sınıfı: bayat sınıflandırma). Sed
# deseni Makefile'a ÇAPALI — CAT satırının biçimi değişirse liste BOŞ döner ve
# kapı KIRMIZI verir (sessiz atlama YOK, D-675).
# D1 (D-681): x86_64-none → x86_64-unknown-none-elf; burada ÜÇLÜNÜN ADI değil, iki
# derleyicinin AYNI üçlüyü üretip üretmediği ölçülür (EŞİTLİK).
# [D-663] per-birim zaman aşımı + bellek tavanı ir_uret ile (bölüm (2) tanımı).
X86_PARCA=$(sed -n 's/.*cat \(runtime\/kem_heap\.kem.*kem_os_x86\.kem\) > \$(BUILD)\/kem_os_x86_comb\.kem/\1/p' Makefile)
if [ -z "$X86_PARCA" ]; then
    echo "  🔴 BİRLEŞİK x86 — Makefile'dan parça listesi okunamadı"
    fail=$((fail+1))
else
    x86_eksik=""
    for parca in $X86_PARCA; do [ -f "$parca" ] || x86_eksik="$x86_eksik $parca"; done
    if [ -n "$x86_eksik" ]; then
        echo "  🔴 BİRLEŞİK x86 — parça dosyası YOK:$x86_eksik"
        fail=$((fail+1))
    else
        cat $X86_PARCA > "$TMP/x86.kem"
        "$KEMGU" --llvm --mimari x86_64-none "$TMP/x86.kem" > "$TMP/x86_c.ll" 2> "$TMP/x86_c.err"; xrc=$?
        ir_uret "$TMP/x86_s.ll" "$CODEGEN" --llvm --mimari x86_64-none "$TMP/x86.kem"; xsr=$?
        if [ "$xrc" -ne 0 ] || head -1 "$TMP/x86_c.ll" 2>/dev/null | grep -q "hata\[" || [ ! -s "$TMP/x86_c.ll" ]; then
            echo "  🔴 BİRLEŞİK x86 — C oracle DERLEYEMEDİ (rc=$xrc) — birim ÖLÇÜLEMEDİ:"
            grep -m3 "hata\[" "$TMP/x86_c.err" "$TMP/x86_c.ll" 2>/dev/null | sed 's/^/      /'
            fail=$((fail+1))
        elif [ "$xsr" -eq 124 ] || [ "$xsr" -eq 137 ]; then
            echo "  🔴 BİRLEŞİK x86 — KEMGU IR URETIMI ASILDI/OLDURULDU (rc=$xsr)"
            fail=$((fail+1))
        elif [ ! -s "$TMP/x86_s.ll" ]; then
            echo "  🔴 BİRLEŞİK x86 — KEMGU codegen IR üretemedi"
            fail=$((fail+1))
        else
            grep "^define" "$TMP/x86_c.ll" | sed 's/(.*//' | sort > "$TMP/x86_c.d"
            grep "^define" "$TMP/x86_s.ll" | sed 's/(.*//' | sort > "$TMP/x86_s.d"
            xc=$(grep -m1 "target triple" "$TMP/x86_c.ll"); xs=$(grep -m1 "target triple" "$TMP/x86_s.ll")
            xa=$(grep -c "asm sideeffect" "$TMP/x86_c.ll"); xsa=$(grep -c "asm sideeffect" "$TMP/x86_s.ll")
            xv=$(grep -cE "load volatile|store volatile" "$TMP/x86_c.ll"); xsv=$(grep -cE "load volatile|store volatile" "$TMP/x86_s.ll")
            xi=$(grep -c "inttoptr" "$TMP/x86_c.ll"); xsi=$(grep -c "inttoptr" "$TMP/x86_s.ll")
            # [D-704] bare-metal x86: HER define `noredzone` taşımalı (ring0 kesme çerçevesi
            # RSP altına itilir). Ad kümesi `sed 's/(.*//'` ile özniteliği SİLER → ayrı sayılır;
            # yalnız eşitlik yetmez (ikisi birden bıraksa 0=0) → define sayısına eşit olmalı.
            xn=$(grep -c ") noredzone {" "$TMP/x86_c.ll"); xsn=$(grep -c ") noredzone {" "$TMP/x86_s.ll")
            xd=$(grep -c "^define" "$TMP/x86_c.ll"); xsd=$(grep -c "^define" "$TMP/x86_s.ll")
            # asm ŞABLON + KISIT dizgisi de eşleşmeli: ADET eşit ama İÇERİK farklı
            # olabilir (D-416 kusuru) — adet tek başına onu KAÇIRIR.
            grep -o 'asm sideeffect "[^"]*", "[^"]*"' "$TMP/x86_c.ll" | sort > "$TMP/x86_c.a"
            grep -o 'asm sideeffect "[^"]*", "[^"]*"' "$TMP/x86_s.ll" | sort > "$TMP/x86_s.a"
            if [ "$xc" = "$xs" ] && diff -q "$TMP/x86_c.d" "$TMP/x86_s.d" >/dev/null 2>&1 \
               && [ "$xa" -eq "$xsa" ] && diff -q "$TMP/x86_c.a" "$TMP/x86_s.a" >/dev/null 2>&1 \
               && [ "$xv" -gt 0 ] && [ "$xv" -eq "$xsv" ] \
               && [ "$xi" -gt 0 ] && [ "$xi" -eq "$xsi" ] \
               && [ "$xn" -gt 0 ] && [ "$xn" -eq "$xd" ] && [ "$xsn" -eq "$xsd" ]; then
                echo "  ✅ BİRLEŞİK x86 — $(wc -l <"$TMP/x86_c.d") işlev, $xa satıriçi asm (şablon+kısıt eşleşti), $xv volatile, $xi inttoptr, noredzone $xn/$xd, üçlü eşleşti"
                pass=$((pass+1))
            else
                echo "  🔴 BİRLEŞİK x86 — işlev C=$(wc -l <"$TMP/x86_c.d") KEMGU=$(wc -l <"$TMP/x86_s.d") · asm C=$xa KEMGU=$xsa · volatile C=$xv KEMGU=$xsv · inttoptr C=$xi KEMGU=$xsi · noredzone C=$xn/$xd KEMGU=$xsn/$xsd"
                [ "$xc" = "$xs" ] || echo "      üçlü: C=[$xc] KEMGU=[$xs]"
                diff "$TMP/x86_c.d" "$TMP/x86_s.d" 2>/dev/null | head -5 | sed 's/^/      /'
                diff "$TMP/x86_c.a" "$TMP/x86_s.a" 2>/dev/null | head -5 | sed 's/^/      /'
                fail=$((fail+1))
            fi
        fi
    fi
fi

# x86_64 MİMARİ dosyalarının SINIFLANDIRMA denetimi — aarch64 döngüsünün ikizi.
# Her biri Makefile x86 CAT listesinde GEÇMELİ; geçmeyen dosya kapı dışında
# kalır (D-675 sınıfı sessiz körlük).
for f in runtime/mimari/x86_64/*.kem; do
    [ -f "$f" ] || continue
    case " $X86_PARCA " in
        *" $f "*) ;;
        *) echo "  🔴 $f — SINIFLANDIRILMAMIŞ: Makefile calistir_kem_os_x86 CAT listesine ekle"; fail=$((fail+1)) ;;
    esac
done

# ---- (4) SINIFLANDIRMA BÜTÜNLÜĞÜ — sessiz dışarıda kalma YOK ----
# runtime/mimari/x86_64/*.kem ayrı hedef (aarch64 AS001 verir) → sınıflandırması
# yukarıda bölüm (5)'te Makefile x86 CAT listesine karşı denetlenir.
for f in runtime/*.kem runtime/mimari/arm64/*.kem; do
    [ -f "$f" ] || continue
    case " $birim_sahipleri $(echo $PARCALAR) " in
        *" $f "*) ;;
        *) echo "  🔴 $f — SINIFLANDIRILMAMIŞ: BIRIMLER ya da PARCALAR'a ekle"; fail=$((fail+1)) ;;
    esac
done

echo "=== bare-metal (aarch64 + x86_64) yapı paritesi: $pass/$((pass+fail)) (birleşik OS + birimler + parça kapsamı: $kapsam_islev işlev, $kapsam_kuresel küresel), 0 atlandı ==="
[ "$fail" -eq 0 ]
