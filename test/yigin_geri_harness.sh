#!/usr/bin/env bash
# [D-763] Yığın geri alma değişmezi. Self-host döngü geçicilerinin yığınını YALNIZ döngü gövdesinin
# sonunda geri alır. Literal / by-value dizi elemanı yardımcısının hemen ardından geri almak (D-753)
# YANLIŞTI: LLVM büyük yapı kopyasını geri almadan SONRAYA erteliyor, veri yığın işaretçisinin altında
# kalıyor ve araya giren kesme onu eziyordu (KEMGU-OS'ta aralıklı USB/ELF hataları; x86 xhci_adim
# makine kodunda ölçüldü). Kapı: her `stackrestore`dan sonraki 3 komut içinde döngü geri dalı
# (`br label`) olmalı. Davranışsal kapılar bu sınıfı GÖRMEZ (kesme zamanlamasına bağlı, aralıklı).
set -u
cd "$(dirname "$0")/.."
CODEGEN="${CODEGEN:-build/codegen}"
[ -x "$CODEGEN" ] || { echo "  🔴 $CODEGEN yok"; exit 2; }
W="build/_yigin_geri_$$"; mkdir -p "$W"; trap 'rm -rf "$W"' EXIT
kirmizi=0; toplam=0
for f in test/cg_korpus/cg_dongu_yapi_yigin.kem test/cg_korpus/cg_dizi_yaz_genislik.kem test/ornekler/kem_os_x86_kabuk.kem; do
  if [ "$f" = test/ornekler/kem_os_x86_kabuk.kem ]; then
    RT=$(grep -m1 '^RT="' test/bm_x86_uefi_harness.sh | sed 's/^RT="//;s/"$//')
    RT="$RT runtime/mimari/x86_64/kem_surec_hw.kem runtime/mimari/x86_64/kem_rtc_hw.kem test/ornekler/kem_os_x86_blok_ortak.kem test/ornekler/kem_os_x86_vfs_ortak.kem"
    cat $RT "$f" > "$W/birim.kem"; arg="--mimari x86_64-none"
  else cp "$f" "$W/birim.kem"; arg=""; fi
  if ! "$CODEGEN" --llvm $arg "$W/birim.kem" > "$W/b.ll" 2>"$W/err"; then echo "  🔴 $f: IR üretilemedi"; head -3 "$W/err"; kirmizi=$((kirmizi+1)); continue; fi
  sonuc=$(python3 - "$W/b.ll" <<'PY'
import sys
s=open(sys.argv[1],encoding='utf-8').read().split('\n')
r=[i for i,l in enumerate(s) if 'call void @llvm.stackrestore' in l]
kotu=[i for i in r if not any(x.strip().startswith('br label') for x in s[i+1:i+4])]
print(len(r), len(kotu), (s[kotu[0]+1].strip() if kotu else ''))
PY
)
  set -- $sonuc; toplam=$((toplam+1))
  if [ "$2" -ne 0 ]; then echo "  🔴 $f: $2/$1 stackrestore döngü sonunda DEĞİL (ilk: $3)"; kirmizi=$((kirmizi+1))
  elif [ "$1" -eq 0 ]; then echo "  🔴 $f: hiç stackrestore yok (döngü geçicileri geri alınmıyor)"; kirmizi=$((kirmizi+1))
  else echo "  ✓ $f: $1 stackrestore, hepsi döngü geri dalından önce"; fi
done
echo "=== yığın geri alma değişmezi: $((toplam-kirmizi))/$toplam birim ==="
[ $kirmizi -eq 0 ]
