#!/usr/bin/env bash
# ============================================================================
# modul_codegen_harness.sh — ÇAPRAZ-DOSYA MODÜL codegen kapısı (D-402).
# ----------------------------------------------------------------------------
# NEDEN AYRI BİR KAPI: `test/moduller/` hiçbir kapının altında DEĞİLDİ. D-399/400/401
# boyunca oranı elle bir kabuk döngüsüyle izledim — ve bu tam olarak D-395'te
# "kapı değildir" diye yazdığım şeydi. Bedeli ölçüldü: D-401b denemesi (modül
# öneki soyma) oranı 11/18 → 9/18'e düşürdü ve düşenlerden biri LINK HATASI DEĞİL
# SESSİZ YANLIŞ CEVAPTI (`ana_golge_jenerik`: C=1, KEMGU=100). `codegen_diff` ve
# `codegen_genis` İKİSİ DE YEŞİLDİ — bu yüzeyi görmüyorlar. Değişikliği elle
# ölçtüğüm için geri aldım; ölçmeseydim gönderirdim.
#
# Kıyas: exit kodu + STDOUT (codegen_genis ile aynı disiplin).
# Kullanım: bash test/modul_codegen_harness.sh  (veya make calistir_modul_codegen)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}
RT=${RT:-build/kdl_runtime.o}
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/modcg.XXXXXX" 2>/dev/null || echo "build/modcg.$$")
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

# ---- MUAFİYET LİSTESİ (KÜÇÜLMEK ZORUNDA — asla büyümemeli) ----
# Kapı 7 muafiyetle kuruldu (11/18). D-404 (`yetki<R>`), D-405 (`bölge_al`) ve
# D-406 (ham-işaretçi indeksleme) ALTISINI kapattı → **17/18**.
#
# ⚠ TEŞHİS TARİHİ — ilk yazdığım gerekçe YANLIŞTI. Bu listeyi "hepsi tek kök:
# dönüş-tipi-güdümlü çıkarsama" diye açıklamıştım. Hata satırını tek tek
# izleyince ÜÇ AYRI kök çıktı ve hiçbiri o değildi:
#     bölge_al eşlemesi yok · yetki<R> IR tipi yok · `*T` indekslemesi heap-dizi
#     yoluna düşüyor
# "Dönüş-tipi-güdümlü çıkarsama" yazmaya başlasaydım YANLIŞ YERİ onarırdım.
#
# KALAN 1:
#   ana_ifd — çapraz-modül ÇEŞİT payload'u: `{ i8, i64, ptr, ptr, ptr, ptr }`
#             beklenen yerde i32. Tagged-union layout çözümü.
#
# ⚠ `dizi_yapi` BU LİSTEDE FAZLADAN DURUYORDU. D-406 ölçümünü ARA DURUMDA
# (yazma kolu henüz onarılmamışken) yapmıştım ve 127 gördüm; onarım tamamlanınca
# 42'ye dönmüştü ama listeyi yeniden ölçmedim. **Muafiyet eklerken ölçümün
# HANGİ ANDA yapıldığına dikkat et** — yarım onarımın sonucu kalıcı muafiyete
# dönüşebilir. Liste ancak SON durumda ölçülerek yazılmalı.
#
# KURAL: buraya satır EKLEMEK kapıyı zayıflatmaktır. Önce KÖKÜ onar.
MUAF=""
muaf_mi() { case " $MUAF " in *" $1 "*) return 0;; esac; return 1; }

link_retry() {
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    return 1
}
run_exe() {
    "$1" > "$2" 2>&1; RC=$?
    d=0
    while [ "$RC" -eq 127 ] && [ "$d" -lt 12 ]; do
        sleep 0.3; "$1" > "$2" 2>&1; RC=$?; d=$((d+1))
    done
    return 0
}

pass=0; fail=0; atla=0; muaf_say=0
for f in test/moduller/*.kem; do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)
    grep -q "işlev main()" "$f" 2>/dev/null || continue
    if muaf_mi "$b"; then
        echo "  ⚠ $b — MUAF (ayrı kök; harness başlığına bak)"
        muaf_say=$((muaf_say+1)); continue
    fi

    # ORACLE kurulamıyorsa karşılaştırma anlamsız → atla (başarısızlık DEĞİL).
    # test/moduller'de KASITLI hata örnekleri var (ana_belirsiz/ana_gizli/
    # ana_kutuphane C'de de linklenmez) — onlar buraya düşer.
    # D-424: oracle IR üretemiyorsa sebep TİP HATASIDIR → "atla" DEĞİL, POZİTİF
    # İDDİA: self-host da reddetmeli (T040/T041/T042 üçlüsü buraya düşer ve
    # `--check` zaten BİREBİR paritedeydi).
    if ! "$KEMGU" --llvm "$f" > "$TMP/$b.c.ll" 2>/dev/null; then
        if "$CODEGEN" --llvm "$f" > "$TMP/$b.s.ll" 2>/dev/null; then
            echo "  🔴 $b — C tip hatasıyla REDDEDİYOR, KEMGU IR ÜRETİYOR (loud→silent)"
            fail=$((fail+1)); continue
        fi
        pass=$((pass+1)); continue          # iki taraf da reddediyor → PARİTE
    fi
    link_retry "$TMP/$b.c.ll" "$TMP/$b.c.exe" || { atla=$((atla+1)); continue; }
    run_exe "$TMP/$b.c.exe" "$TMP/$b.c.out"; coracle=$RC
    if [ "$coracle" -eq 127 ]; then
        echo "  ⚠ $b — oracle 127 (ortamsal) atlandı"; atla=$((atla+1)); continue
    fi

    if ! "$CODEGEN" --llvm "$f" > "$TMP/$b.s.ll" 2>/dev/null; then
        echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue
    fi
    if ! link_retry "$TMP/$b.s.ll" "$TMP/$b.s.exe"; then
        echo "  🔴 $b — KEMGU IR link edilemedi: $(grep -m1 'error:' "$TMP/$b.err" 2>/dev/null | sed 's|.*error: ||' | cut -c1-60)"
        fail=$((fail+1)); continue
    fi
    run_exe "$TMP/$b.s.exe" "$TMP/$b.s.out"; kaday=$RC

    if [ "$coracle" -eq "$kaday" ] && diff -q "$TMP/$b.c.out" "$TMP/$b.s.out" >/dev/null 2>&1; then
        pass=$((pass+1))
    elif [ "$coracle" -ne "$kaday" ]; then
        echo "  🔴 $b — exit farkı: C=$coracle ≠ KEMGU=$kaday"; fail=$((fail+1))
    else
        echo "  🔴 $b — exit aynı ($coracle) ama STDOUT farklı:"
        diff "$TMP/$b.c.out" "$TMP/$b.s.out" 2>/dev/null | head -4 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done
echo "=== modül codegen eşdeğerlik: $pass/$((pass+fail)) ($atla atlandı, $muaf_say muaf) ==="
[ "$fail" -eq 0 ]
