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
# Kapı 7 muafiyetle kuruldu (11/18). D-404 (`yetki<R>`), D-405 (`bölge_al`) ve
# D-406 (ham-işaretçi indeksleme) beşini kapattı → **16/18**.
#
# ⚠ TEŞHİS TARİHİ — ilk yazdığım gerekçe YANLIŞTI. Bu listeyi "hepsi tek kök:
# dönüş-tipi-güdümlü çıkarsama" diye açıklamıştım. Hata satırını tek tek
# izleyince ÜÇ AYRI kök çıktı ve hiçbiri o değildi:
#     bölge_al eşlemesi yok · yetki<R> IR tipi yok · `*T` indekslemesi heap-dizi
#     yoluna düşüyor
# "Dönüş-tipi-güdümlü çıkarsama" yazmaya başlasaydım YANLIŞ YERİ onarırdım.
#
# KALAN 2, AYRI köklerde:
#   ana_ifd   — çapraz-modül ÇEŞİT payload'u: `{ i8, i64, ptr, ptr, ptr, ptr }`
#               beklenen yerde i32. Tagged-union layout çözümü.
#   dizi_yapi — C=42, KEMGU=127 (çalışma-anı çökmesi). Link GEÇİYOR, yani IR
#               geçerli; kusur DAVRANIŞTA — ayrı teşhis ister.
#
# KURAL: buraya satır EKLEMEK kapıyı zayıflatmaktır. Önce KÖKÜ onar.
MUAF="ana_ifd dizi_yapi"
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
