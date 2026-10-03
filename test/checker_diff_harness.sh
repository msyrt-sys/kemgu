#!/usr/bin/env bash
# ============================================================================
# checker_diff_harness.sh — SELF-HOST tip denetleyici doğruluk kanıtı (Aşama 2).
# ----------------------------------------------------------------------------
# KEMGU'da yazılmış checker'ın (selfhost/checker.kem) ürettiği DÜZ hata dump'ını,
# C checker'ın `--checkdump` oracle'ına (D-051) karşı diff'ler.
#   Format: <KOD>\t<satır>\t<sütün> (traversal sırası) veya "OK"
# Korpus: test/check_korpus/*.kem (TC milestone'ları büyüdükçe genişler).
# TC1 = temel kapsam/ad çözümü (T002). Korpus TEMIZ parse eder (yalnız T/L/M).
#
# Kullanım: bash test/checker_diff_harness.sh  (veya make calistir_checker_diff)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
RT=${RT:-build/kdl_runtime.o}
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/checkdiff.XXXXXX" 2>/dev/null || echo "build/checkdiff.$$")
mkdir -p "$TMP"

if ! "$KEMGU" --llvm selfhost/checker.kem > "$TMP/c.ll" 2>/dev/null; then
    echo "🔴 KEMGU-checker --llvm üretemedi"; exit 1
fi
if ! clang -x ir "$TMP/c.ll" -x none "$RT" -o "$TMP/kemcheck.exe" 2>/dev/null; then
    echo "🔴 KEMGU-checker link edilemedi"; exit 1
fi

# D-361: modül fikstürleri de kapıya dâhil (çapraz-dosya import yüzeyi).
# MUAF (0) — liste BOŞ. Modül yüzeyi D-361/362/363'te tamamen kapandı:
#   ana_secili/ana_belirsiz → D-361 (seçili import + T042)
#   ana_kutuphane           → D-362 (runtime UTF-8 yol + T040 + T016)
#   ana_gizli               → D-363 (T041, private-by-default)
# Yeni bir muafiyet eklemek gerekiyorsa GEREKÇESİ DECISIONS_LOG'a yazılmalı.
MUAF=""
muaf_mi() { case " $MUAF " in *" $1 "*) return 0;; esac; return 1; }

# [D-660] PER-DOSYA ZAMAN ASIMI + BELLEK TAVANI (G14).
# NEDEN: asilan kapi, dusen kapidan KOTU teshis edilir. Oncesinde bir asilma
# TUM kapiyi asiyordu ve `make` yalniz `rc=124` donuyordu — hangi DOSYA astigi
# gorunmuyordu (D-650'de S150b ile olculdu).
# ⚠⚠ BELLEK TAVANI BIR TESPIT MEKANIZMASI DEGIL, HASAR SINIRLAMASIDIR —
# ve bu ayrim OLCULDU: tavani asan surec `rc=1` verir (tahsis hatasi), 137
# DEGIL; yani kapi onu cikis koduyla TANIMAZ. Tespiti saglayan sey asagidaki
# "oracle cikti uretmeli" denetimi ve adayin ciktisinin AYRISMASIDIR.
# Tavanin degeri su: bu oturumda bir sabotaj artigi `kemcheck`i SONSUZ
# DONGUYE soktu, surec 26.6 GiB'e cikti ve CEKIRDEGIN OOM-KILLER'I onu
# oldurup TUM OTURUMU asagi cekti (cekirdek gunlugunde kayitli). Zaman asimi
# tek basina yetmezdi: 26 GiB'e cikmak 60 saniyeden AZ suruyor.
# ⚠ BUGUNKU KORPUS TAVANI ASMIYOR (olculdu: C derleyici 20 MB altinda bile
# dogru cikti veriyor) -> tavan ATESLENMEYEN bir sigortadir, kapi onu
# GATE'LEMIYOR. Boyle kaydedilmesi D-510'un disiplini.
# Degerler cevre degiskeniyle ezilebilir (yavas makine / buyuk korpus).
KAP_SN=${KAP_SN:-60}
KAP_KB=${KAP_KB:-4000000}

# [D-661] `timeout` YETENEGI OLCULUR, VARSAYILMAZ. Bu depo POSIX varsayimindan
# ALTI KEZ isirildi (D-560..D-566: /tmp baglamasi, /dev/fd, sinyal-tabanli cikis
# kodu, RSS muhasebesi). `timeout` YOKSA `kos` 127 doner ve HIC CIKTI YAZMAZ ->
# asagidaki "oracle cikti uretmedi" denetimi WINDOWS'TA YANLIS KIRMIZI verirdi.
# Yetenek bir kez olculur; yoksa sinirsiz kosulur ve bu ACIKCA bildirilir
# (D-486: sessiz atlama yasak — ama burada atlanan sey KAPI degil SINIRDIR).
if command -v timeout >/dev/null 2>&1 && timeout 5 true >/dev/null 2>&1; then
    TO="timeout $KAP_SN"
else
    TO=""
    echo "  ⚠ 'timeout' YOK -> per-dosya zaman asimi UYGULANMIYOR (kapi yine olcer;"
    echo "     yalniz ASILMA koruması dusüyor). Bkz. D-661."
fi

kos() {   # kos <cikti-dosyasi> <komut...> -> rc
    out="$1"; shift
    ( ulimit -v "$KAP_KB" 2>/dev/null; $TO "$@" > "$out" 2>/dev/null )
}

pass=0; fail=0; muaf=0
for f in test/check_korpus/*.kem test/moduller/*.kem; do
    [ -f "$f" ] || continue
    if muaf_mi "$(basename "$f")"; then muaf=$((muaf+1)); continue; fi
    kos "$TMP/oracle.txt" "$KEMGU" --checkdump "$f"; orc=$?
    kos "$TMP/aday.txt" "$TMP/kemcheck.exe" "$f";     adr=$?
    # 124 = timeout . 137 = SIGKILL (OOM) . 139 = SIGSEGV
    if [ "$orc" -eq 124 ] || [ "$adr" -eq 124 ] \
       || [ "$orc" -eq 137 ] || [ "$adr" -eq 137 ]; then
        echo "  🔴 $(basename "$f") — ASILDI/OLDURULDU (sinir ${KAP_SN}s / ${KAP_KB}KB;" \
             "oracle rc=$orc, aday rc=$adr)"
        fail=$((fail+1)); continue
    fi
    # [D-660] ORACLE CIKTI URETMELI — yoksa KARSILASTIRMA ANLAMSIZ.
    # ⚠ BU DENETIM OLMADAN KENDI TAVANIM YANLIS YESIL URETIYORDU (olculdu):
    # `ulimit` iki tarafi da dusurunce IKISI DE BOS cikti veriyor, `diff` esit
    # diyor ve kapi GECIYOR. C derleyici her gecerli korpus dosyasi icin ya
    # `OK` ya tani basar; BOS cikti "oracle kosmadi" demektir (D-547'nin
    # "oracle tarafindaki gerilemeyi sessizce yutma" disiplini).
    if [ ! -s "$TMP/oracle.txt" ]; then
        echo "  🔴 $(basename "$f") — ORACLE CIKTI URETMEDI (rc=$orc) — kapi OLCMEDI"
        fail=$((fail+1)); continue
    fi
    if diff -q "$TMP/oracle.txt" "$TMP/aday.txt" >/dev/null 2>&1; then
        echo "  ✅ $(basename "$f")"; pass=$((pass+1))
    else
        echo "  🔴 $(basename "$f") — C checker (oracle) vs KEMGU checker farkı:"
        diff "$TMP/oracle.txt" "$TMP/aday.txt" | head -10
        fail=$((fail+1))
    fi
done
echo "=== checker --checkdump sıfır-diff: $pass/$((pass+fail)) korpus ($muaf muaf) ==="
[ "$fail" -eq 0 ]
