#!/usr/bin/env bash
# [D-772] Çekirdek önbelleği kapısı (host, QEMU'suz). Önbellek bir HIZ özelliğidir ama yanlış
# isabet SESSİZ bir doğruluk hatasıdır: eski çekirdek yeni kaynak yerine koşar ve kapılar onu
# ölçer. Bu kapı dört şeyi ölçer:
#  1. ilk derleme kaçırır, ikincisi isabet eder;
#  2. isabetin dört ürünü (ham.ll, k.ll, k.o, k.elf) önbelleksiz derlemeyle BAYT BAYT aynıdır;
#  3. kaynak değişince (tek yorum satırı) anahtar değişir ve yeniden derlenir;
#  4. derleyici ikilisi değişince (aynı ikilinin kopyası + bir bayt) yeniden derlenir.
set -euo pipefail
cd "$(dirname "$0")/.."
D="${KEMGU:-build/kemgu}"
T=$(mktemp -d build/obkapi.XXXXXX)

source test/calisma_dizini.sh
calisma_izle "$T"
export KEMGU_CEKIRDEK_ONBELLEK_KOK="$T/ob" UEFI_YALNIZ_DERLE=1
hata=0
derle() {  # derle <derleyici> <kaynak> <calisma> <onbellek 0/1> -> "isabet"/"derlendi"/"kapali"
    mkdir -p "$3"
    env UEFI_DERLEYICI="$1" UEFI_KAYNAK="$2" UEFI_CALISMA_DIZINI="$3" KEMGU_CEKIRDEK_ONBELLEK="$4" \
        bash test/bm_x86_uefi_harness.sh > "$3/log" 2>&1 || { echo "  🔴 derleme başarısız ($3)"; tail -5 "$3/log"; exit 2; }
    if grep -q "CEKIRDEK_ONBELLEK isabet" "$3/log"; then echo isabet
    elif grep -q "CEKIRDEK_ONBELLEK derlendi" "$3/log"; then echo derlendi
    else echo kapali; fi
}
bekle() { if [ "$2" = "$3" ]; then echo "  ✓ $1: $2"; else echo "  🔴 $1: beklenen $3, gelen $2"; hata=1; fi; }
K=test/ornekler/kem_os_x86_kabuk.kem
bekle "ilk derleme" "$(derle "$D" "$K" "$T/a" 1)" derlendi
bekle "ikinci derleme" "$(derle "$D" "$K" "$T/b" 1)" isabet
bekle "önbellek kapalı" "$(derle "$D" "$K" "$T/c" 0)" kapali
for x in ham.ll k.ll k.o k.elf; do
    if cmp -s "$T/b/$x" "$T/c/$x"; then echo "  ✓ isabet $x önbelleksiz derlemeyle aynı"
    else echo "  🔴 isabet $x önbelleksiz derlemeden FARKLI"; hata=1; fi
done
mkdir -p "$T/kaynak"; { cat "$K"; echo "// obkapi $$"; } > "$T/kaynak/kem_os_x86_kabuk.kem"
bekle "kaynak değişti" "$(derle "$D" "$T/kaynak/kem_os_x86_kabuk.kem" "$T/d" 1)" derlendi
cp "$D" "$T/derleyici"; printf '\0' >> "$T/derleyici"
bekle "derleyici değişti" "$(derle "$T/derleyici" "$K" "$T/e" 1)" derlendi
if [ "$hata" -ne 0 ]; then echo "🔴 çekirdek önbelleği kapısı BAŞARISIZ"; exit 2; fi
echo "=== çekirdek önbelleği: isabet + bayt eşitliği + kaynak/derleyici geçersizleştirmesi geçti ==="
