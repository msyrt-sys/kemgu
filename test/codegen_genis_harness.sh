#!/usr/bin/env bash
# ============================================================================
# codegen_genis_harness.sh — GENİŞ codegen eşdeğerlik kapısı (D-395).
# ----------------------------------------------------------------------------
# NEDEN AYRI BİR KAPI: `codegen_diff_harness.sh` yalnız `test/cg_korpus/`
# üzerinde koşar. O korpus AMAÇLI olarak dar (her dosya bir codegen özelliğini
# izole eder). Sonuç: GERÇEK programlardaki sapmalar sessizce birikti — bu kapı
# yazılmadan önce `test/ornekler` + `stdlib/temel` yüzeyinde **31 sapma** vardı
# ve `codegen_diff` bunların HİÇBİRİNİ görmüyordu (D-388..D-394 ile kapatıldı).
# Kapatılan köklerin ÜÇÜ sessiz yanlış cevap üretiyordu.
#
# İKİ FARK (codegen_diff'ten):
#   1. Korpus: elle seçilmiş özellik dosyaları DEĞİL, GERÇEK programlar.
#   2. Karşılaştırma: exit kodu + **STDOUT**. Yalnız exit'e bakmak yetmez —
#      `bignum_selfhost` iki tarafta da exit 0 veriyordu ama stdout'ta `0` yerine
#      yığın adresi basıyordu (D-393'te bu şekilde yakalandı).
#
# Kullanım: bash test/codegen_genis_harness.sh  (veya make calistir_codegen_genis)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/cggenis); mkdir -p "$TMP"

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

# Win11: taze .exe ilk exec'te Defender taramasında 127 verebilir (ortamsal).
# codegen_diff_harness.sh'teki D-339 kuralı BURADA DA geçerli: 127 yalnız
# ORACLE'da ortamsal sayılır. Oracle sağlam değer verirken aday kalıcı 127
# diyorsa bu bir ANLAŞMAZLIKTIR, sessizce atlanamaz.
link_retry() {   # $1=ll  $2=exe   ; LINK_ERR global (kurate denetimi icin)
    LINK_ERR="$2.linkerr"
    clang -x ir "$1" -x none "$RT" -o "$2" 2>"$LINK_ERR"; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>"$LINK_ERR"; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>"$LINK_ERR"; [ -x "$2" ] && return 0
    return 1
}
run_exe() {   # $1=exe  $2=stdout dosyasi ; RC global
    # [D-468] ZAMAN AŞIMI ŞART. Bu kapı GERÇEK programları çalıştırıyor ve
    # zaman aşımı YOKTU: bloklanan tek bir program (kanal/görev kilitlenmesi,
    # stdin bekleyen kod) kapıyı SONSUZA DEK asardı. Bu ölçüldü — bir tam
    # takım koşumu tam burada 2.5 saat boyunca hiçbir çıktı vermeden asılı
    # kaldı. Asılan kapı, sessiz kapı kadar kötüdür: kimse onu koşturmaz.
    # (Aynı sınıf `ag_kosum`da S66 sabotajıyla yakalanıp orada da `timeout`
    # ile kapatılmıştı — D-466.)
    timeout 30 "$1" > "$2" 2>&1; RC=$?
    deneme=0
    # 127 = Defender/exec yarışı (D-413) — ortamsal, yeniden dene.
    while [ "$RC" -eq 127 ] && [ "$deneme" -lt 12 ]; do
        sleep 0.3
        timeout 30 "$1" > "$2" 2>&1; RC=$?
        deneme=$((deneme+1))
    done
    return 0
}

# ---- MUAFİYET LİSTESİ: BOŞ ----
# Kapı kurulduğunda 2 satır vardı (D-395); ikisi de kapatıldı:
#   gorev_temel   → D-396 (eşleş desen-bağlamasının iç tipi)
#   matris_carpim → D-397 (SIMD vektör<T,N>)
# Muafiyet listesi KÜÇÜLMEK İÇİNDİR. Buraya yeni satır EKLEMEK kapıyı
# zayıflatmaktır: bir sapma çıkarsa önce KÖKÜ onar. Muafiyet yalnız kökü
# ÖLÇÜLMÜŞ ve ayrı iş olduğu KANITLANMIŞ durumlar içindir — ve o zaman bile
# geçici sayılır. (checker_diff'in listesi de bu disiplinle boşa indi.)
MUAF=""
muaf_mi_bos_() { :; }

# ---- [D-547] ORACLE LINK BASARISIZLIGI: KURATE LISTE ----
# ONCESINDE bu dal SESSIZDI (`atla=$((atla+1)); continue`) — yani ORACLE
# TARAFINDAKI HER GERILEME sessizce yutuluyordu. D-518'de `codegen_diff` icin
# tam bu kor nokta olculup kurate listeye baglanmisti; ayni sertlestirme.
#
# ⚠ LISTE OLCULDU, TAHMIN EDILMEDI (9 dosya, tek tek link hatasi okundu):
#   BM_MUAF (8) — `kdl_mmio_oku32` / `kdl_mmio_yaz32`: GERCEK bare-metal
#     semboller, host runtime'da YOK. Bunlar `--mimari arm64` yolunda derlenir;
#     host'ta link edilememeleri MESRUDUR.
#   ORACLE_KUSUR (1) — `05_yapi`: bare-metal DEGIL. C oracle'in KENDISI
#     gecersiz IR uretiyor ("base element of getelementptr must be sized",
#     D-419'da olculdu). Gercek bir dil kusuru; self-host paritesi isi DEGIL.
#
# ⚠ KAYITLI IDDIA YANLISTI: "9 atlama, hepsi mesru bare-metal link hatasi"
#   deniyordu; dokuzuncusu bare-metal degil, bir ORACLE KUSURUDUR.
#
# ⚠ AD-BAZLI DEGIL, SEBEP-BAZLI: BM_MUAF'taki bir dosya BASKA bir sebeple
#   linklenemezse kapi KIRMIZI olur (asagida `kdl_mmio_` araniyor). Duz bir
#   ad listesi o gerilemeyi de yutardi.
BM_MUAF="kem_heap kem_mmio_kernel mmio_smoke virtio_blk_config_selfhost virtio_net_mac_selfhost virtio_net_selfhost virtio_selfhost virtio_selfhost_rw"
ORACLE_KUSUR="05_yapi"
listede_() { case " $2 " in *" $1 "*) return 0;; esac; return 1; }

muaf_mi() { case " $MUAF " in *" $1 "*) return 0;; esac; return 1; }

pass=0; fail=0; atla=0; muaf_say=0
for f in test/ornekler/*.kem stdlib/temel/*.kem; do
    [ -f "$f" ] || continue
    b=$(basename "$f" .kem)
    # Yalnız çalıştırılabilir programlar (main'i olanlar).
    grep -q "işlev main()" "$f" 2>/dev/null || continue
    if muaf_mi "$b"; then
        echo "  ⚠ $b — MUAF (kökü ölçüldü, ayrı iş; harness başlığına bak)"
        muaf_say=$((muaf_say+1)); continue
    fi

    # --- ORACLE: C codegen ---
    # D-424: Oracle IR ÜRETEMİYORSA sebep TİP HATASIDIR (C `--llvm` tip hatasında
    # durur, D-337). Bu ARTIK "atla" DEĞİL, POZİTİF BİR İDDİADIR: self-host da
    # reddetmelidir. Öncesinde self sessizce IR üretiyordu ve kapı bunu ATLIYORDU
    # → "atlama listesi bir KÖR NOKTA ENVANTERİDİR" (D-419) dersinin tam örneği.
    # Bu dal aynı zamanda D-424'ün TEK GATE'idir: tip kapısı kaldırılırsa buradan
    # kırmızı döner.
    if ! "$KEMGU" --llvm "$f" > "$TMP/$b.c.ll" 2>/dev/null; then
        if "$CODEGEN" --llvm "$f" > "$TMP/$b.s.ll" 2>/dev/null; then
            echo "  🔴 $b — C tip hatasıyla REDDEDİYOR, KEMGU IR ÜRETİYOR (loud→silent)"
            fail=$((fail+1)); continue
        fi
        pass=$((pass+1)); continue          # iki taraf da reddediyor → PARİTE
    fi
    if ! link_retry "$TMP/$b.c.ll" "$TMP/$b.c.exe"; then
        if listede_ "$b" "$BM_MUAF"; then
            if grep -q "kdl_mmio_" "$LINK_ERR" 2>/dev/null; then
                echo "  ⚠ $b — bare-metal MMIO sembolu (host'ta link edilemez) atlandi"
                atla=$((atla+1)); continue
            fi
            echo "  🔴 $b — BM_MUAF listesinde ama link hatasi MMIO DEGIL:"
            head -2 "$LINK_ERR" | sed 's/^/      /'
            fail=$((fail+1)); continue
        fi
        if listede_ "$b" "$ORACLE_KUSUR"; then
            echo "  ⚠ $b — C oracle GECERSIZ IR uretiyor (D-419, bilinen dil kusuru) atlandi"
            atla=$((atla+1)); continue
        fi
        echo "  🔴 $b — oracle link BASARISIZ ve kurate listede YOK:"
        head -2 "$LINK_ERR" | sed 's/^/      /'
        echo "      (kok onarilmali; ya da SEBEBI OLCULUP BM_MUAF/ORACLE_KUSUR'a eklenmeli)"
        fail=$((fail+1)); continue
    fi
    run_exe "$TMP/$b.c.exe" "$TMP/$b.c.out"; coracle=$RC
    if [ "$coracle" -eq 127 ]; then
        echo "  ⚠ $b — oracle 127 (Defender exec yarışı, ortamsal) atlandı"
        atla=$((atla+1)); continue
    fi

    # --- ADAY: self-host codegen ---
    if ! "$CODEGEN" --llvm "$f" > "$TMP/$b.s.ll" 2>/dev/null; then
        echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue
    fi
    if ! link_retry "$TMP/$b.s.ll" "$TMP/$b.s.exe"; then
        echo "  🔴 $b — KEMGU IR link edilemedi"; fail=$((fail+1)); continue
    fi
    run_exe "$TMP/$b.s.exe" "$TMP/$b.s.out"; kaday=$RC

    # NOT: bare-metal keşif dosyaları (kem_mmio_ham, kem_pointer) host'ta
    # eşlenmemiş MMIO adresi okur ve İKİ TARAFTA DA segfault eder (139).
    # Bu BEKLENEN ve parite açısından EŞLEŞMEDİR — özel muafiyet GEREKMEZ,
    # çünkü oracle da aynı çöküyor. Muafiyet listesi tutmuyoruz: eşleşen
    # çökme zaten geçer, eşleşmeyen çökme geçmemeli.
    if [ "$coracle" -eq "$kaday" ] && diff -q "$TMP/$b.c.out" "$TMP/$b.s.out" >/dev/null 2>&1; then
        pass=$((pass+1))
    elif [ "$coracle" -ne "$kaday" ]; then
        echo "  🔴 $b — exit farkı: C=$coracle ≠ KEMGU=$kaday"; fail=$((fail+1))
    else
        echo "  🔴 $b — exit aynı ($coracle) ama STDOUT farklı:"
        diff "$TMP/$b.c.out" "$TMP/$b.s.out" 2>/dev/null | head -6 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done
echo "=== codegen GENİŞ eşdeğerlik: $pass/$((pass+fail)) gerçek program ($atla atlandı, $muaf_say muaf) ==="
[ "$fail" -eq 0 ]
