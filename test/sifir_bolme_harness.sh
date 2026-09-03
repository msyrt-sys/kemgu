#!/usr/bin/env bash
# ============================================================================
# sifir_bolme_harness.sh — [D-502] Sıfıra bölme TEMİZ DURUYOR mu?
# ----------------------------------------------------------------------------
# ÖLÇÜLEN (D-501'de bulunan açık): korumasız `sdiv`/`srem` süreci SIGFPE ile
# ÖLDÜRÜYORDU (exit 136), tanı YOK, panik mesajı YOK, ve düz literal `10 / 0`
# bile `--check`ten temiz geçiyordu. Bu, dilin manşet iddiasının ihlaliydi:
# "Çökmezlik: Exception yok (sonuç<T,H>)".
#
# BEKLENEN: bölen sıfırsa `PANIK: sifira bolme` + exit 134 (kdl_panik →
# abort → SIGABRT). Dizi sınır ihlaliyle AYNI mekanizma (D-069).
# exit 136 (SIGFPE) = GERİLEME: koruyucu düşmüş.
#
# ⚠ HER İKİ DERLEYİCİDE ölçülür — koruyucu C'de olup self-host'ta olmazsa
# üretilen program yine çöker (D-407: aynı soruyu iki yerde ayrı yanıtlayan
# kod ayrışır).
#
# ⚠ NORMAL BÖLME AYRICA ÖLÇÜLÜR (`normal.kem` → 42): koruyucunun her bölmeyi
# panikletmediğini kanıtlar. Yalnız negatif şekiller olsaydı "her bölmeyi
# reddet" sabotajı kapıdan GEÇERDİ (D-425).
# ============================================================================
set -u
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU="${KEMGU:-build/kemgu${EXE}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
[ -x "$KEMGU" ] || { echo "🔴 HATA: kemgu ikilisi YOK ($KEMGU) — kapı KOŞMADI"; exit 1; }
CODEGEN="${CODEGEN:-build/codegen${EXE}}"
[ -x "$CODEGEN" ] || CODEGEN="build/codegen"
[ -x "$CODEGEN" ] || { echo "🔴 HATA: codegen ikilisi YOK ($CODEGEN) — kapı KOŞMADI"; exit 1; }
RT=build/kdl_runtime.o
[ -f "$RT" ] || { echo "🔴 HATA: $RT yok — kapı KOŞMADI"; exit 1; }

TMP=$(mktemp -d 2>/dev/null || echo /tmp/sifir_bolme); mkdir -p "$TMP"
hata=0; olculen=0

kos() {   # $1=derleyici $2=etiket $3=dosya $4=beklenen_exit [$5=beklenen_mesaj]
    "$1" --llvm "$3" > "$TMP/a.ll" 2>/dev/null || { echo "  🔴 $2 $(basename $3): IR üretilemedi"; hata=1; return; }
    clang -x ir "$TMP/a.ll" -x none "$RT" -o "$TMP/a.exe" 2>/dev/null \
        || { echo "  🔴 $2 $(basename $3): LINK-RED"; hata=1; return; }
    out=$(timeout 30 "$TMP/a.exe" 2>&1); rc=$?
    olculen=$((olculen+1))
    if [ "$rc" -ne "$4" ]; then
        echo "  🔴 $2 $(basename $3): exit=$rc (beklenen $4)"
        [ "$rc" -eq 136 ] && echo "     → SIGFPE: sıfır-bölme koruyucusu DÜŞMÜŞ (D-502 gerilemesi)"
        hata=1
        return
    fi
    msg="${5:-PANIK: sifira bolme}"
    if [ "$4" -eq 134 ] && ! echo "$out" | grep -q "$msg"; then
        echo "  🔴 $2 $(basename $3): exit doğru ama PANİK MESAJI yok (beklenen: $msg)"
        hata=1
    fi
}

# [D-513] KAYDIRMA MİKTARI da AYNI KAPIDA. Mekanizma birebir aynı (inline icmp
# + br + kdl_panik + unreachable) ve aynı çökmezlik değişmezini korur; ayrı bir
# kapı açmak envanteri gereksiz bölerdi.
# ⚠ ÖLÇÜLEN AÇIK: korumasız `shl/ashr/lshr` miktar >= bit genişliğinde LLVM
#   POISON üretiyordu ve sonuç OPTİMİZASYON SEVİYESİNE BAĞLIYDI — dört şeklin
#   dördünde de -O0 exit=0, -O2 exit=1 (AYNI PROGRAM, AYNI IR). Bu SARMA DEĞİL
#   UB'dir: aritmetik taşma (`add`, nsw/nuw YOK) tanımlı sarmadır ve
#   -O0/-O2'de KARARLIDIR; kaydırma değildi.
# ⚠ `normal.kem` (21 << 1 -> 42) ZORUNLU: yalnız negatif şekiller olsaydı
#   "her kaydırmayı reddet" sabotajı kapıdan GEÇERDİ (D-425).
for c in "$KEMGU:C" "$CODEGEN:SELF"; do
    bin="${c%%:*}"; et="${c##*:}"
    for f in test/bolme/sifir_*.kem; do kos "$bin" "$et" "$f" 134; done
    kos "$bin" "$et" test/bolme/normal.kem 42
    for f in test/kaydirma/sol_sabit.kem test/kaydirma/sag_degisken.kem \
             test/kaydirma/isaretsiz.kem test/kaydirma/negatif.kem; do
        kos "$bin" "$et" "$f" 134 "PANIK: kaydirma miktari gecersiz"
    done
    kos "$bin" "$et" test/kaydirma/normal.kem 42
    # [D-546] VEKTOR LANE INDEKSI de AYNI KAPIDA — mekanizma birebir aynı
    # (inline icmp uge + br + kdl_panik + unreachable) ve aynı çökmezlik
    # değişmezini korur; üçüncü bir kapı envanteri gereksiz bölerdi.
    # ⚠ ÖLÇÜLEN AÇIK: korumasız `extractelement` aralık dışı indekste LLVM
    #   POISON üretiyordu ve sonuç OPTİMİZASYON SEVİYESİNE BAĞLIYDI
    #   (-O0 exit=224, -O2 exit=1 — AYNI PROGRAM, AYNI IR).
    # ⚠ `negatif.kem` bir POZİTİF ölçümdür: `icmp uge` İŞARETSİZ olduğu için
    #   negatif indeks aynı dala düşer, ayrı bir `icmp slt 0` GEREKMEZ.
    # ⚠ `degisken_disi.kem` derleme-zamanı reddin KAÇIRACAĞI şekli ölçer.
    # ⚠ `normal.kem` (21+21 -> 42) ZORUNLU: yalnız negatif şekiller olsaydı
    #   "her vektor_eleman'i panikletir" sabotajı kapıdan GEÇERDİ (D-425).
    for f in test/vektor/sabit_disi.kem test/vektor/negatif.kem              test/vektor/degisken_disi.kem; do
        kos "$bin" "$et" "$f" 134 "PANIK: vektor lane indeksi gecersiz"
    done
    kos "$bin" "$et" test/vektor/normal.kem 42
done

if [ "$olculen" -eq 0 ]; then
    echo "🔴 HATA: hiç ölçüm yapılmadı — kapı boşa koştu"; exit 1
fi
if [ "$hata" -ne 0 ]; then
    echo "=== sıfıra bölme kapısı: BAŞARISIZ ($olculen ölçüm) ==="; exit 1
fi
echo "=== sıfıra bölme + kaydırma + vektör lane: $olculen ölçüm (C + SELF), hepsi temiz durdu ==="
