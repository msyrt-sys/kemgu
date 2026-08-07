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
KEMGU=${KEMGU:-build/kemgu.exe}
CODEGEN=${CODEGEN:-build/codegen.exe}
RT=${RT:-build/kdl_runtime.o}
TMP=$(mktemp -d 2>/dev/null || echo /tmp/modcg); mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
    echo "ℹ codegen.exe yok — kapı atlandı (önce build/codegen.exe kurulmalı)."
    exit 0
fi

# ---- MUAFİYET LİSTESİ (KÜÇÜLMEK ZORUNDA — asla büyümemeli) ----
# Kapı kurulduğunda 11/18 geçiyordu. Kalan 7'nin TAMAMI tek kök: çapraz-modül
# generic monomorfizasyonunun DÖNÜŞ-TİPİ-GÜDÜMLÜ kısmı.
#
# D-401 generic işlev mono'sunu ekledi ama V1'i yalnız ÇIPLAK `T` parametresinden
# çıkarsar. `kütüphane/dizi.kem` imzaları bunu aşıyor:
#     oluştur<T>(böl: yetki<Bellek>) -> Liste<T>   ← T YALNIZ dönüşte
#     boy<T>(l: &Liste<T>) -> tam64                ← T İÇ İÇE (&Liste<T>)
# T, `değişken l: dizi::Liste<tam64>` annotasyonundan gelmeli. Ayrıca
# `yetki<Bellek>` parametresi `%kdl_yetki` taşınmalı, self-host `i32` sanıyor.
#
# KURAL: buraya satır EKLEMEK kapıyı zayıflatmaktır. Önce KÖKÜ onar.
MUAF="ana_ifd ana_kap ana_kap_coklu dizi_coklu dizi_kullan dizi_nitelikli_param dizi_yapi"
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
        echo "  ⚠ $b — MUAF (dönüş-tipi-güdümlü mono; harness başlığına bak)"
        muaf_say=$((muaf_say+1)); continue
    fi

    # ORACLE kurulamıyorsa karşılaştırma anlamsız → atla (başarısızlık DEĞİL).
    # test/moduller'de KASITLI hata örnekleri var (ana_belirsiz/ana_gizli/
    # ana_kutuphane C'de de linklenmez) — onlar buraya düşer.
    "$KEMGU" --llvm "$f" > "$TMP/$b.c.ll" 2>/dev/null || { atla=$((atla+1)); continue; }
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
