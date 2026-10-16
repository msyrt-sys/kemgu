#!/usr/bin/env bash
# ============================================================================
# ag_kosum_harness.sh — GERCEK TCP gidis-donusu [D-466]
# ----------------------------------------------------------------------------
# NEDEN AYRI KAPI: `calistir_stdlib_check` yalnizca DIS BAGIMLILIGI OLMAYAN
# yollari olcer (dogrulama + baglanamama). Pozitif yol bir KARSI TARAF ister;
# onu surekli kosan kapiya koymak ortamdan oturu ARALIKLI kirmizi uretirdi ve
# "aralikli kapi" en kotu kapi turudur (insanlar gormezden gelmeyi ogrenir).
#
# Kapi HER IKI DERLEYICIYI de olcer: C oracle ve self-host ayni gidis-donusu
# yapmali. Boylece soket yolunda parite de gate'lenmis olur.
#
# QEMU kapisi gibi: dinleyici kurulamazsa ZARIFCE ATLAR (kirmizi vermez).
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}
CC_HOST=${CC_HOST:-gcc}
PORT=58421
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/agkosum.XXXXXX" 2>/dev/null || echo "build/agkosum.$$")
mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
# [D-486] EKSIK IKILI = HATA, ATLAMA DEGIL. Bu kapinin make hedefi
# `$(BUILD)/codegen$(EXE)`e BAGIMLIDIR -> make uzerinden ikili GARANTI.
# Yani bu dal make yolunda OLU KOD; tek islevi bir YOL HATASINI SESSIZCE
# YUTMAKTI. Olculdu: Makefile `CODEGEN=build/codegen.exe` diye SABIT
# geciyordu -> Linux'ta ikilinin adi `build/codegen` -> SEKIZ parite
# kapisi birden atlandi ve `make` yine 0 dondu (tam takim YESIL gorundu).
# D-446'nin sinifi: var olan ama kosmayan kapi, olmayandan tehlikelidir.
    echo "🔴 HATA: codegen ikilisi YOK ($CODEGEN) — kapı KOŞMADI"; exit 1
fi

DINLE="$TMP/dinleyici.exe"
if ! $CC_HOST -O2 test/ag/dinleyici.c -o "$DINLE" -lws2_32 2>/dev/null; then
    if ! $CC_HOST -O2 test/ag/dinleyici.c -o "$DINLE" 2>/dev/null; then
        echo "ℹ dinleyici derlenemedi — kapı atlandı (soket yok?)."; exit 0
    fi
fi

kosum() {   # $1 = derleyici yolu, $2 = etiket
    local ll="$TMP/$2.ll" exe="$TMP/$2.exe" log="$TMP/$2.srv"
    if ! "$1" --llvm test/ag/istemci.kem > "$ll" 2>/dev/null; then
        echo "  🔴 $2 — IR uretilemedi"; return 1
    fi
    if ! clang -x ir "$ll" -x none build/kdl_runtime.o -o "$exe" 2>/dev/null; then
        echo "  🔴 $2 — link edilemedi"; return 1
    fi
    "$DINLE" "$PORT" > "$log" 2>&1 &
    local spid=$!
    # Dinleyici "HAZIR" yazana dek bekle (sabit sleep YERINE kosula bak:
    # sabit gecikme yavas makinede ARALIKLI kirmizi uretirdi).
    local i=0
    while [ $i -lt 50 ]; do
        grep -q HAZIR "$log" 2>/dev/null && break
        sleep 0.1; i=$((i+1))
    done
    # ⚠ ZAMAN AŞIMI ŞART — SABOTAJLA ÖLÇÜLDÜ (S66): yetki argümanı düşürülmezse
    # istemci yanlış register'lardan okuyup çöp handle üretiyor, dinleyici ise
    # gelmeyen bağlantıyı SONSUZA DEK bekliyor → kapı ASILIYOR. Asılan kapı,
    # sessiz kapı kadar kötüdür: kimse 10 dakika bekleyen bir testi koşturmaz.
    # `timeout` hem istemciyi hem `wait`i sınırlar.
    timeout 15 "$exe" > "$TMP/$2.out" 2>&1
    local rc=$?
    if [ "$rc" -eq 124 ]; then
        echo "  🔴 $2 — istemci ZAMAN AŞIMI (15 sn)"
        kill $spid 2>/dev/null; wait $spid 2>/dev/null
        return 1
    fi
    # Dinleyici hâlâ bekliyor olabilir (istemci hiç bağlanmadıysa) — öldür.
    kill $spid 2>/dev/null
    wait $spid 2>/dev/null
    if [ "$rc" -ne 42 ]; then
        echo "  🔴 $2 — istemci exit=$rc (42 bekleniyor)"
        sed 's/^/      /' "$TMP/$2.out" 2>/dev/null | head -3
        return 1
    fi
    if ! grep -q 'ALDIM:SELAM-SUNUCU' "$log" 2>/dev/null; then
        echo "  🔴 $2 — sunucu beklenen veriyi ALMADI"
        sed 's/^/      /' "$log" 2>/dev/null | head -3
        return 1
    fi
    echo "  ✅ $2 — gidiş-dönüş tamam (istemci 42, sunucu veriyi aldı)"
    return 0
}

fail=0
kosum "$KEMGU"   "C-oracle" || fail=$((fail+1))
kosum "$CODEGEN" "self-host" || fail=$((fail+1))
echo "=== ag gerçek gidiş-dönüş: $((2-fail))/2 derleyici ==="
[ "$fail" -eq 0 ]
