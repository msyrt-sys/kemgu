#!/usr/bin/env bash
# ============================================================================
# bolge_operand_harness.sh — [D-506] YAPISAL kapı: BÖLGE OPERANDI paritesi
# ----------------------------------------------------------------------------
# ⚠⚠ NEDEN AYRI KAPI GEREKTİ — ÜÇ MEVCUT KAPI DA BU SAPMAYI GÖREMEZ:
#   `codegen_diff`  ÇIKIŞ KODU karşılaştırır  → yanlış bölge de 42 döndürür
#   `yapi_diff`     `define` KÜMESİ            → bölge operandı define'da yok
#   `checker_diff`  TANI dump'ı                → codegen'e hiç bakmaz
# ÖLÇÜLDÜ (D-506): C `%5` (ρ_yerel) yayarken self-host `%rho` (ρ_caller)
# yayıyordu ve ÜÇ KAPI DA YEŞİLDİ. D-417'nin (spekülasyon bariyeri) birebir
# tekrarı: davranışsal kapılar bellek-yönetimi özelliğine KÖRDÜR.
#
# OLÇÜLEN: her korpus dosyasında `kdl_dizi_olustur(ptr X` operandlarının
# DİZİSİ C ve self-host'ta AYNI mı? (register NUMARASI değil — ρ_caller mı
# ρ_yerel mi olduğu; ikisi farklı BİÇİMDE yazılır: `%rho` vs `%N`.)
# ============================================================================
set -u
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU="${KEMGU:-build/kemgu${EXE}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
[ -x "$KEMGU" ] || { echo "🔴 HATA: kemgu ikilisi YOK ($KEMGU) — kapı KOŞMADI"; exit 1; }
CODEGEN="${CODEGEN:-build/codegen${EXE}}"
[ -x "$CODEGEN" ] || CODEGEN="build/codegen"
[ -x "$CODEGEN" ] || { echo "🔴 HATA: codegen ikilisi YOK ($CODEGEN) — kapı KOŞMADI"; exit 1; }

TMP=$(mktemp -d 2>/dev/null || echo /tmp/bolge_op); mkdir -p "$TMP"

# ρ SINIFI: `%rho` (çağıran/parametre) mi, `%N` (yerel, kdl_bolge_olustur) mi?
# Register NUMARASINI kasten YOK SAYAR — o iki derleyicide farklı olabilir ve
# bu kapının sorusu "hangi BÖLGE", "hangi numara" değil.
rho_sinifi() {
    grep -oE 'kdl_dizi_olustur\(ptr %[A-Za-z0-9_]+' "$1" \
        | sed 's/.*ptr //' \
        | sed 's/^%rho.*$/CALLER/; s/^%[0-9][0-9]*$/YEREL/'
}

# [D-507] KAPANIS ENV i de karsilastir: `@malloc` (HEAP) mi `@kdl_bolge_ayir`
# (BOLGE) mi? Kapanis bagi hapsedilmisse env bolgeden alinir ve islev sonunda
# TOPLU serbest edilir; oncesinde HIC serbest edilmiyordu.
# ⚠ MUAFIYET `43_closure_param` DEGIL — o dosya bu korpusta yok. Muafiyet
# gerekirse buraya yazilir; su an BOS (muafiyetsiz kapi).
# ⚠⚠ MUAFIYET = ACIK PARITE BORCU ENVANTERI (gizleme DEGIL, IZLEME):
#   Bu dort dosya `bölge_al` yolunu kullanir. D-494/D-495 (`bölge_al` ->
#   ρ_yerel, hapsedilme kanitliysa) YALNIZ C ye uygulanmisti; self-host a
#   HIC PORTLANMADI. Bu kapi onu ILK KEZ gorunur kildi — borc YARATILMADI,
#   OLCULDU. Self-host DAHA KONSERVATIF (malloc = sizinti, UAF DEGIL).
#   Port yapilinca bu satir BOSALTILMALI; kapi o zaman "MUAF ama artik
#   ESLESIYOR" demez, sadece yesil kalir -> periyodik olarak DENE.
ENV_MUAF=""
env_sinifi() {
    grep -v '^declare' "$1" | grep -oE 'call ptr @(malloc|kdl_bolge_ayir)' \
        | sed 's/.*@malloc/HEAP/; s/.*@kdl_bolge_ayir/BOLGE/'
}

topl=0; fark=0; atla=0
for f in test/cg_korpus/*.kem test/perf/*.kem; do
    [ -f "$f" ] || continue
    "$KEMGU"   --llvm "$f" > "$TMP/c.ll"  2>/dev/null || { atla=$((atla+1)); continue; }
    "$CODEGEN" --llvm "$f" > "$TMP/s.ll" 2>/dev/null || { atla=$((atla+1)); continue; }
    grep -q "kdl_dizi_olustur(ptr" "$TMP/c.ll" || continue   # dizi yok → konu dışı
    topl=$((topl+1))
    b=$(basename "$f" .kem); em=0
    for m in $ENV_MUAF; do [ "$m" = "$b" ] && em=1; done
    if [ "$em" -eq 0 ] && ! diff -q <(env_sinifi "$TMP/c.ll") <(env_sinifi "$TMP/s.ll") >/dev/null; then
        echo "  🔴 $b — kapanış env operandı SAPMASI:"
        diff <(env_sinifi "$TMP/c.ll") <(env_sinifi "$TMP/s.ll") | head -4 | sed 's/^/     /'
        fark=$((fark+1))
    fi
    if ! diff -q <(rho_sinifi "$TMP/c.ll") <(rho_sinifi "$TMP/s.ll") >/dev/null; then
        echo "  🔴 $(basename "$f") — bölge operandı SAPMASI:"
        diff <(rho_sinifi "$TMP/c.ll") <(rho_sinifi "$TMP/s.ll") | head -4 | sed 's/^/     /'
        fark=$((fark+1))
    fi
done

if [ "$topl" -eq 0 ]; then
    echo "🔴 HATA: dizi tahsisi içeren dosya bulunamadı — kapı boşa koştu"; exit 1
fi
if [ "$fark" -ne 0 ]; then
    echo "=== bölge operandı paritesi: $fark/$topl dosyada SAPMA ==="
    exit 1
fi
echo "=== bölge operandı paritesi: $topl/$topl dosya (atlanan $atla) ==="
