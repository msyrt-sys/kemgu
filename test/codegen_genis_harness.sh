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
KEMGU=${KEMGU:-build/kemgu.exe}
CODEGEN=${CODEGEN:-build/codegen.exe}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/cggenis); mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
    echo "ℹ codegen.exe yok — kapı atlandı (önce build/codegen.exe kurulmalı)."
    exit 0
fi

# Win11: taze .exe ilk exec'te Defender taramasında 127 verebilir (ortamsal).
# codegen_diff_harness.sh'teki D-339 kuralı BURADA DA geçerli: 127 yalnız
# ORACLE'da ortamsal sayılır. Oracle sağlam değer verirken aday kalıcı 127
# diyorsa bu bir ANLAŞMAZLIKTIR, sessizce atlanamaz.
link_retry() {   # $1=ll  $2=exe
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    clang -x ir "$1" -x none "$RT" -o "$2" 2>/dev/null; [ -x "$2" ] && return 0
    return 1
}
run_exe() {   # $1=exe  $2=stdout dosyasi ; RC global
    "$1" > "$2" 2>&1; RC=$?
    deneme=0
    while [ "$RC" -eq 127 ] && [ "$deneme" -lt 12 ]; do
        sleep 0.3
        "$1" > "$2" 2>&1; RC=$?
        deneme=$((deneme+1))
    done
    return 0
}

# ---- MUAFİYET LİSTESİ (KÜÇÜLMEK ZORUNDA — asla büyümemeli) ----
# Bu kapı kurulduğunda 67 gerçek programdan 65'i geçiyordu. Kalan 2'si, kapının
# ölçtüğü sınıftan AYRI iki eksik özelliğe bağlı ve kökleri ÖLÇÜLDÜ:
#
#   matris_carpim — SIMD. Kaynak `vektör<kesirli32, 4>` kullanıyor; C bunu
#     `<4 x float>` olarak yayar, self-host codegen'de vektör TİPİ HİÇ YOK →
#     her şeyi i32 sanıyor ("'%9' i32 but expected 'float'"). Arg-genişletme
#     kusuru DEĞİL; eksik özellik.
#
# (gorev_temel D-396'da ONARILDI ve listeden ÇIKARILDI — muafiyet listesi
#  küçülmek içindir; kapatılan kök burada durmaz.)
#
# KURAL: buraya yeni satır EKLEMEK, kapıyı zayıflatmaktır. Bir sapma çıkarsa
# önce KÖKÜ onar; muafiyet yalnız kökü ÖLÇÜLMÜŞ ve ayrı iş olduğu kanıtlanmış
# durumlar içindir. (checker_diff'in muafiyet listesi bu disiplinle BOŞA indi.)
MUAF="matris_carpim"
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
    # Oracle kurulamıyorsa (tip hatası / modül importu / bu kapının işi olmayan
    # bir sebep) karşılaştırma ANLAMSIZ → atla. Bu bir başarısızlık DEĞİL.
    "$KEMGU" --llvm "$f" > "$TMP/$b.c.ll" 2>/dev/null || { atla=$((atla+1)); continue; }
    link_retry "$TMP/$b.c.ll" "$TMP/$b.c.exe" || { atla=$((atla+1)); continue; }
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
