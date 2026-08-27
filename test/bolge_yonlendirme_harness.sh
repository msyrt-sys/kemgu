#!/usr/bin/env bash
# ============================================================================
# bolge_yonlendirme_harness.sh — [D-488] YAPISAL kapi: cagrilan-ozeti
# ----------------------------------------------------------------------------
# ⚠ NEDEN YAPISAL? Davranissal kapilar bu ozellige KORDUR: ozet calissa da
# calismasa da program 42 doner, IR gecerli kalir, hicbir test kizarmaz.
# D-417'nin dersi (spekulasyon bariyeri) burada birebir tekrar ediyor:
# "program dogru calisti" bir BELLEK-YONETIMI ozelliginde YETERSIZ KANITTIR.
#
# OLCULEN INVARYANT (`test/cg_korpus/cg_bolge_ozet.kem`):
#   dizi `a` -> SAKLAMAYAN `topla`ya gecer -> ρ_yerel  (kdl_bolge_olustur)
#   dizi `b` -> SAKLAYAN   `aynen`e gecer  -> ρ_caller (donduruyor)
#   dizi `d` -> SAKLAYAN   `sakla`ya gecer -> ρ_caller (YAPI ALANINA yaziyor —
#                          `stdlib/regex.kem`in GERCEK deseni: Regex.op: Dizi<tam32>)
# Ikisi de ayni bolgeye giderse ozet ya HIC calismiyor (ikisi de caller) ya da
# SAGLAMSIZ (ikisi de yerel = saklanan dizi serbest edilir = UAF).
#
# ⚠⚠ OLCULDU: SAGLAMSIZ HAL BU FIKSTURDE GOZLENEBILIR DEGILDIR.
# Sabotaj S73b (`ea_param_tutmuyor` daima 1 = saklanan dizi de ρ_yerel'e) ile
# olculdu: program exit=42 VERDI ve ASan/UBSan SIFIR bulgu bastı. Sebep:
# ρ_yerel serbest birakmasi `main` SONUNDA olur, okumalar ondan ONCE biter.
# Yani bu kapi kaldirilirsa D-488'in sagligini olcen HICBIR SEY KALMAZ:
# davranissal kapilar da, ASan da SESSIZ kalir.
# D-417'nin dersinin (spekulasyon bariyeri) ampirik tekrari:
# "program dogru calisti" + "ASan temiz" BELLEK-YONETIMI ozelliginde
# YETERSIZ KANITTIR.
# ============================================================================
set -u
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU="${KEMGU:-build/kemgu${EXE}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
# [D-487] eksik ikili = HATA, atlama DEGIL.
[ -x "$KEMGU" ] || { echo "🔴 HATA: kemgu ikilisi YOK ($KEMGU) — kapı KOŞMADI"; exit 1; }

F=test/cg_korpus/cg_bolge_ozet.kem
[ -f "$F" ] || { echo "🔴 HATA: fikstür YOK ($F)"; exit 1; }

IR=$("$KEMGU" --llvm "$F" 2>/dev/null) || { echo "🔴 HATA: IR üretilemedi"; exit 1; }

# ⚠ HER islevin kendi `kdl_bolge_olustur` cagrisi vardir -> dosya genelinde
# `head -1` `topla`nınkini secer ve kapi SAHTE KIRMIZI verir (bu harness'in
# ilk surumunde tam bu oldu). Once `main` GOVDESINI ayikla.
IR=$(echo "$IR" | awk '/^define i32 @main\(\)/{f=1} f{print} f&&/^}$/{exit}')
# main gövdesindeki iki bölge kaydını çöz.
YEREL=$(echo "$IR" | grep -oE '%[0-9]+ = call ptr @kdl_bolge_olustur\(\)' | head -1 | grep -oE '^%[0-9]+')
CALLER=$(echo "$IR" | grep -oE '%[0-9]+ = call ptr @kdl_global_bolge_al\(\)' | head -1 | grep -oE '^%[0-9]+')
if [ -z "$YEREL" ] || [ -z "$CALLER" ]; then
    echo "🔴 HATA: bölge kayıtları çözülemedi (ρ_yerel='$YEREL' ρ_caller='$CALLER')"; exit 1
fi

# Dizi tahsislerinin bölge operandlarını say.
NY=$(echo "$IR" | grep -cE "call ptr @kdl_dizi_olustur\(ptr ${YEREL}," )
NC=$(echo "$IR" | grep -cE "call ptr @kdl_dizi_olustur\(ptr ${CALLER}," )

hata=0
if [ "$NY" -ne 1 ]; then
    echo "  🔴 SAKLAMAYAN çağrıya giden dizi ρ_yerel'de DEĞİL (beklenen 1, gelen $NY)"
    echo "     → çağrılan-özeti çalışmıyor (D-488 gerilemesi)"
    hata=1
fi
if [ "$NC" -ne 2 ]; then
    echo "  🔴 SAKLAYAN çağrılara giden diziler ρ_caller'da DEĞİL (beklenen 2, gelen $NC)"
    echo "     → özet SAĞLAMSIZ: saklanan dizi serbest edilir = UAF"
    hata=1
fi

if [ "$hata" -eq 0 ]; then
    echo "=== bölge yönlendirme (çağrılan-özeti): ρ_yerel=$NY ρ_caller=$NC — DOĞRU ==="
else
    echo "=== bölge yönlendirme: BAŞARISIZ ==="
fi
exit $hata
