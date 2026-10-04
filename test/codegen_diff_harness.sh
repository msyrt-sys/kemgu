#!/usr/bin/env bash
# ============================================================================
# codegen_diff_harness.sh — AŞAMA 3 (codegen self-host) SEMANTİK oracle (D-072).
# ----------------------------------------------------------------------------
# KEMGU'da yazılmış codegen'in (selfhost/codegen.kem → build/codegen.exe) ürettiği
# IR'ı, C codegen (build/kemgu.exe --llvm) ile EXIT-KODU eşdeğerliği üzerinden
# doğrular (byte-identik IR DEĞİL — SSA/hoist/format uygulama detayı; bkz. D-072).
#   Her korpus programı: C-codegen→clang→çalıştır→exit  vs  KEMGU-codegen→...→exit.
# Korpus: test/cg_korpus/*.kem (CG milestone'ları büyüdükçe genişler; her dosyada main).
#
# Kullanım: bash test/codegen_diff_harness.sh  (veya make calistir_codegen_diff)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}
RT=${RT:-build/kdl_runtime.o}
KORPUS=${KORPUS:-test/cg_korpus}
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/cgdiff.XXXXXX" 2>/dev/null || echo "build/cgdiff.$$")
mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
    # [D-486] Bu mesaj BAYATTI (D-072/CG1 donemi); kapi artik 155/155.
    # Hedef `$(BUILD)/codegen$(EXE)`e BAGIMLI -> make yolunda ikili GARANTI,
    # yani bu dal OLU KOD ve tek islevi bir yol hatasini yutmakti.
    echo "🔴 HATA: codegen ikilisi YOK ($CODEGEN) — kapı KOŞMADI"
    exit 1
fi

# Win11 flakiness: freshly-linked .exe Defender taramasında ilk exec'te 127 verebilir
# / clang çıktısı kilitlenebilir → link'i exe oluşana dek 3 kez dene (semantik değil, ortam).
link_retry() {   # $1=ll  $2=exe
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && ver_ok=1 || ver_ok=0
    [ "$ver_ok" -eq 1 ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    return 1
}
# Yeni .exe ilk exec'te Defender taraması yüzünden 127 (command-not-found) verebilir;
# korpusta hiçbir program 127 dönmez → 127 DAİMA ortamsal (gerçek codegen hatası kesin
# bir exit verir: 139 segfault / yanlış değer, asla 127). OS dosyayı bırakana dek bekle +
# tekrar dene (RC global, en çok 12 tur ~3.6s). Kalıcı 127 → çağıran ⚠ ATLAR (fail DEĞİL).
run_exe() {   # $1=exe
    "$1" >/dev/null 2>&1; RC=$?
    deneme=0
    while [ "$RC" -eq 127 ] && [ "$deneme" -lt 12 ]; do
        sleep 0.3
        "$1" >/dev/null 2>&1; RC=$?
        deneme=$((deneme+1))
    done
    # [D-572] AYIRT EDICI: Defender yarisi ILK EXEC'e ozgudur (dosya taranmamis).
    # Yeni bir yola KOPYALAYIP tekrar denemek yarisi cozer; PROGRAM COKUYORSA
    # kopya da ayni kodu verir. Bu sart, cunku Windows'ta cokme 127 gorunebilir
    # (D-565: abort -> 127) ve "127 = daima ortamsal" premisi o yuzden YANLIS.
    # Olculdu: D-572 sabotaji 3 kosumun 2'sinde 127 ile SESSIZCE atlaniyordu.
    KALICI127=0
    if [ "$RC" -eq 127 ]; then
        cp "$1" "$1.r2.exe" 2>/dev/null || { KALICI127=1; return 0; }
        sleep 0.3
        "$1.r2.exe" >/dev/null 2>&1; RC=$?
        [ "$RC" -eq 127 ] && KALICI127=1
    fi
    return 0
}

# [D-469] KONAK MİMARİSİ — TEK KAYNAK MAKEFILE'DAN GELİR (`ARCH`).
# Burada `uname -m` ÇAĞIRMIYORUZ: aynı soruyu ikinci bir yerde yanıtlamak
# D-407'nin ayrışma sınıfıdır. Makefile `KONAK_MIM=$(ARCH)` geçirir; değişken
# boşsa kapı ATLAMAZ, GÜRÜLTÜ ÇIKARIR (D-446: koşmayan kapı, olmayandan kötü).
if [ -z "${KONAK_MIM:-}" ]; then
    echo "🔴 HATA: KONAK_MIM geçirilmedi (Makefile ARCH) — kapı KOŞMADI"
    exit 1
fi

# Dosyanın TEK arch etiketi (yoksa/karışıksa boş döner) — check_kapisi.sh ile
# aynı kural.
dosya_mimari() {
    et="$(grep -oE 'mimari:[[:space:]]*(x86_64|arm64)' "$1" 2>/dev/null \
           | grep -oE '(x86_64|arm64)' | sort -u)"
    [ "$(printf '%s\n' "$et" | grep -c .)" -eq 1 ] || return 1
    printf '%s' "$et"
}

# [D-662] IR URETIMINDE PER-DOSYA ZAMAN ASIMI + BELLEK TAVANI (G15).
# codegen_diff self-host ikilisini (`$CODEGEN`) KORPUS UZERINDE kosturur; bu,
# D-660'in `checker_diff`te kapattigi asilma/OOM sinifina ACIKTI. Bir sabotaj
# artigi ya da gercek bir parser dongusu self `--llvm`i asabilir, surec GiB'lerce
# bellege cikar ve (D-660'ta olculdu) OOM-killer TUM OTURUMU asagi ceker.
# ⚠ `timeout` YETENEGI OLCULUR, VARSAYILMAZ (D-661): yoksa `ir_uret` 127 doner,
# IR dosyasi bos kalir ve asagidaki link/oracle mantigi YANLIS KIRMIZI verirdi.
# Yetenek yoksa sinirsiz kosulur ve ACIKCA bildirilir (kapi yine olcer).
KAP_SN=${KAP_SN:-60}
KAP_KB=${KAP_KB:-4000000}
if command -v timeout >/dev/null 2>&1 && timeout 5 true >/dev/null 2>&1; then
    TO="timeout $KAP_SN"
else
    TO=""
    echo "  ⚠ 'timeout' YOK -> per-dosya zaman asimi UYGULANMIYOR (kapi yine olcer;"
    echo "     yalniz ASILMA koruması dusüyor). Bkz. D-662/D-661."
fi
# ir_uret <cikti.ll> <komut...> -> rc. 124=timeout, 137=SIGKILL(OOM).
ir_uret() { o="$1"; shift; ( ulimit -v "$KAP_KB" 2>/dev/null; $TO "$@" > "$o" 2>/dev/null ); }

pass=0; fail=0; arch_atla=0
for f in "$KORPUS"/*.kem; do
    [ -f "$f" ] || continue
    # Win11'de .exe yeniden-yazımı dosya-kilidi yarışına girer → dosya-başı benzersiz ad.
    b=$(basename "$f" .kem)

    # [D-469] YABANCI MİMARİ ETİKETLİ PROGRAM BU KONAKTA KOŞAMAZ.
    # Bu kapı programı ÇALIŞTIRIP exit kodunu karşılaştırır; `satıriçi_asm`
    # gerçek makine komutu taşır, yani x86 ikizi ARM64'te derlense bile
    # çalışmaz. Tek dosya iki konağı kapsayamaz → korpusta MİMARİ İKİZLER
    # tutulur (`cg_satirici_asm.kem` + `cg_satirici_asm_arm64.kem`) ve konak
    # hangisini koşacağını seçer. Bu bir MUAFİYET DEĞİLDİR: ikiz her konakta
    # ölçülür, yalnız yabancı olan atlanır ve atlama AÇIKÇA yazdırılır
    # (D-518: sessiz atlama gerilemeleri yutar).
    if fm="$(dosya_mimari "$f")" && [ "$fm" != "$KONAK_MIM" ]; then
        echo "  ⏭  $b — '$fm' etiketli asm, konak '$KONAK_MIM' (mimari ikizi koşuyor)"
        arch_atla=$((arch_atla+1)); continue
    fi
    # C codegen → exit (oracle)
    # D-337: bu harness'ın işi CODEGEN eşdeğerliği; tip kapısı AYRI kapıdır
    # (calistir_check_kapisi, D-336) ve cg_korpus'u zaten kapsar. Korpusta
    # KASITLI tip-geçersiz dosyalar var (cg6_trunc/cg_skaler_deref/
    # cg_deref_pointer — codegen trunc/deref yollarını ölçerler); tip kapısı
    # katı olunca bunlar oracle'sız kalıp SESSİZCE atlanıyordu (105→102).
    # `--tip-atla` ile codegen kapsamı korunur, tip zorlaması kaybolmaz.
    ir_uret "$TMP/$b.c.ll" "$KEMGU" --llvm --tip-atla "$f"; oir=$?
    if [ "$oir" -eq 124 ] || [ "$oir" -eq 137 ]; then
        echo "  🔴 $(basename "$f") — ORACLE IR URETIMI ASILDI/OLDURULDU (rc=$oir, sinir ${KAP_SN}s/${KAP_KB}KB)"
        fail=$((fail+1)); continue
    fi
    if ! link_retry "$TMP/$b.c.ll" "$TMP/$b.c.exe"; then
        # [D-518] ATLAMA ARTIK KÜRATE LİSTEYE BAĞLI — eskiden HER oracle-link
        # hatası sessizce atlanıyordu ve bu, C tarafındaki GERİLEMELERİ YUTUYORDU.
        # ⚠ SABOTAJLA ÖLÇÜLDÜ (S96, D-518'in kesirli onarımını C'de geri al):
        #   kapı KIRMIZI olmak yerine YEŞİL kaldı, yalnız sayı 162 -> 161 düştü.
        #   Yani C'nin GEÇERSİZ IR üretmesi bir BAŞARISIZLIK değil, bir ATLAMA
        #   olarak görünüyordu (D-424'ün "atlama listesi bir KÖR NOKTA
        #   ENVANTERİDİR" dersinin aynısı).
        # Liste ÖLÇÜLDÜ, tahmin edilmedi: yalnız bu iki dosya çapraz-modül
        # yüklemesi ister ve C oracle onları TEK BAŞINA derleyemez.
        case "$b" in
            cgmodul_mat|cgmodul_zincir)
                echo "  ⚠ $(basename "$f") — C-codegen IR link edilemedi (oracle yok, atla)"
                continue ;;
            *)
                echo "  🔴 $(basename "$f") — C-codegen IR LINK EDİLEMEDİ."
                echo "     → Bu dosya kürate atlama listesinde DEĞİL. Oracle'ın geçersiz"
                echo "       IR üretmesi bir GERİLEMEDİR; sessizce atlanamaz. Gerçekten"
                echo "       meşru bir atlamaysa listeye GEREKÇESİYLE ekle."
                fail=$((fail+1)); continue ;;
        esac
    fi
    run_exe "$TMP/$b.c.exe"; coracle=$RC; ORACLE_KALICI127=$KALICI127
    # KEMGU codegen → exit (aday)
    # D-424: self-host `--llvm` ARTIK tip hatasında durur (C aynası). Oracle'a
    # yukarıda `--tip-atla` geçiliyor; SİMETRİK olmazsa korpustaki KASITLI
    # tip-geçersiz dosyalar (cg6_trunc, cg_deref_pointer) yalnız self tarafında
    # reddedilir ve kapı YANLIŞ SEBEPLE kırmızıya döner.
    ir_uret "$TMP/$b.k.ll" "$CODEGEN" --llvm --tip-atla "$f"; kir=$?
    if [ "$kir" -eq 124 ] || [ "$kir" -eq 137 ]; then
        echo "  🔴 $(basename "$f") — KEMGU IR URETIMI ASILDI/OLDURULDU (rc=$kir, sinir ${KAP_SN}s/${KAP_KB}KB)"
        fail=$((fail+1)); continue
    fi
    if ! link_retry "$TMP/$b.k.ll" "$TMP/$b.k.exe"; then
        echo "  🔴 $(basename "$f") — KEMGU IR link edilemedi"; fail=$((fail+1)); continue
    fi
    run_exe "$TMP/$b.k.exe"; kaday=$RC
    # D-339 ONARIM: eski kural `coracle==127 || kaday==127` idi ve "korpusta hiçbir
    # program 127 dönmez" premisine dayanıyordu. Bu premis YANLIŞ ölçüldü:
    # cg_isaretsiz_alan.kem sabotajlı codegen ile TAM OLARAK 127 üretti (12 retry
    # sonrası kararlı, oracle 60) → GERÇEK bir miscompile ⚠ ATLANDI olarak yeşil
    # geçti. Yani sabotaj kapısı kendi ölçtüğü şeye kördü.
    # Yeni kural: 127 yalnız ORACLE'da ortamsal sayılır (oracle yoksa karşılaştırma
    # anlamsız). Oracle sağlam bir değer verirken aday kalıcı 127 diyorsa bu bir
    # ANLAŞMAZLIKTIR — sessizce atlanamaz.
    # [D-572] D-339'un ADAY tarafinda uyguladigi kural ORACLE tarafina da
    # simetrik uygulaniyor: 12 tekrar (~3.6s) VE taze bir kopyadan sonra hala
    # 127 ise bu artik "ilk-exec Defender yarisi" DEGILDIR — Windows'ta cokme
    # 127 gorunebilir (D-565). Olculdu: D-572 sabotaji 3 kosumun 2'sinde bu
    # atlama yuzunden SESSIZCE yesil geciyordu.
    if [ "$coracle" -eq 127 ] && [ "${ORACLE_KALICI127:-0}" -eq 1 ]; then
        echo "  🔴 $(basename "$f") — oracle KALICI 127 (12 tekrar + taze kopya sonrasi): ortamsal DEGIL"
        fail=$((fail+1)); continue
    fi
    if [ "$coracle" -eq 127 ]; then
        echo "  ⚠ $(basename "$f") — oracle 127 (Defender exec yarışı, ortamsal) atlandı"; continue
    fi
    if [ "$coracle" -eq "$kaday" ]; then
        echo "  ✅ $(basename "$f") (exit=$coracle)"; pass=$((pass+1))
    else
        echo "  🔴 $(basename "$f") — C-codegen exit=$coracle ≠ KEMGU-codegen exit=$kaday"
        fail=$((fail+1))
    fi
done
echo "=== codegen semantik eşdeğerlik: $pass/$((pass+fail)) korpus ($arch_atla yabancı-mimari atlandı) ==="
[ "$fail" -eq 0 ]
