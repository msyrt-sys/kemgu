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
TMP=$(mktemp -d 2>/dev/null || echo /tmp/cgdiff); mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
    echo "ℹ codegen.exe yok (selfhost/codegen.kem henüz CG1'de — D-072 ADIM-0)."
    echo "   Oracle hazır; codegen.exe derlenince diff koşar. (test_tumu'ya CG1'de bağlanır.)"
    exit 0
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
    return 0
}

pass=0; fail=0
for f in "$KORPUS"/*.kem; do
    [ -f "$f" ] || continue
    # Win11'de .exe yeniden-yazımı dosya-kilidi yarışına girer → dosya-başı benzersiz ad.
    b=$(basename "$f" .kem)
    # C codegen → exit (oracle)
    # D-337: bu harness'ın işi CODEGEN eşdeğerliği; tip kapısı AYRI kapıdır
    # (calistir_check_kapisi, D-336) ve cg_korpus'u zaten kapsar. Korpusta
    # KASITLI tip-geçersiz dosyalar var (cg6_trunc/cg_skaler_deref/
    # cg_deref_pointer — codegen trunc/deref yollarını ölçerler); tip kapısı
    # katı olunca bunlar oracle'sız kalıp SESSİZCE atlanıyordu (105→102).
    # `--tip-atla` ile codegen kapsamı korunur, tip zorlaması kaybolmaz.
    "$KEMGU" --llvm --tip-atla "$f" > "$TMP/$b.c.ll" 2>/dev/null
    if ! link_retry "$TMP/$b.c.ll" "$TMP/$b.c.exe"; then
        echo "  ⚠ $(basename "$f") — C-codegen IR link edilemedi (oracle yok, atla)"; continue
    fi
    run_exe "$TMP/$b.c.exe"; coracle=$RC
    # KEMGU codegen → exit (aday)
    # D-424: self-host `--llvm` ARTIK tip hatasında durur (C aynası). Oracle'a
    # yukarıda `--tip-atla` geçiliyor; SİMETRİK olmazsa korpustaki KASITLI
    # tip-geçersiz dosyalar (cg6_trunc, cg_deref_pointer) yalnız self tarafında
    # reddedilir ve kapı YANLIŞ SEBEPLE kırmızıya döner.
    "$CODEGEN" --llvm --tip-atla "$f" > "$TMP/$b.k.ll" 2>/dev/null
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
echo "=== codegen semantik eşdeğerlik: $pass/$((pass+fail)) korpus ==="
[ "$fail" -eq 0 ]
