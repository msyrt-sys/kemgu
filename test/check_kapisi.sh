#!/usr/bin/env bash
# KEMGU — TIP KONTROL KAPISI: korpus/örnek/stdlib dosyaları `--check`ten geçmeli.
#
# NEDEN (D-336): `--llvm` tip kontrolünü BAĞLAMAZ — `--check` reddettiği bir
# programı derleyip ÇALIŞAN ikili üretebiliyor (ölçüldü: `f(40, 99)` fazla
# argümanla exit 42). Hiçbir kapı korpus üzerinde `--check` koşturmadığı için
# depoda `--check` RED alan 11 dosya birikmişti ve kimse görmemişti; bunların
# 4'ü GERÇEK lineer sızıntıydı (hata dalında görev birleştirilmiyor → L005).
#
# Bu kapı o boşluğu kapatır: yeni eklenen korpus/örnek dosyası tip kontrolünden
# geçmek ZORUNDA. Geçmeyecekse MUAF listesine GEREKÇESİYLE yazılmalı — sessiz
# birikme yerine açık karar.
set -u
KEMGU="${KEMGU:-build/kemgu.exe}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
[ -x "$KEMGU" ] || { echo "⏭  $KEMGU yok — atlandı"; exit 0; }

# --- MUAF LİSTESİ (her satır: dosya|gerekçe) ---
# Muafiyet KALICI DEĞİL: gerekçe ortadan kalkarsa satır silinmeli.
muaf() {
  case "$1" in
    # Tek başına derlenemeyen PARÇA dosyalar: gerçek yapı runtime/*.kem ile
    # BİRLEŞTİRİLİP derler (Makefile kem_os_comb). Tek başına T002 doğaldır.
    *test/ornekler/kem_os.kem)          echo "parça dosya — gerçek yapı birleştirilmiş kaynak" ;;
    *test/ornekler/kem_asm_kernel.kem)  echo "parça dosya + arch-etiketli asm (AS001 hedefe bağlı)" ;;
    # KASTEN hatalı örnek: L001/L002/L004/LR002 sergiler (belgede yazılı).
    *test/ornekler/lineer_hata.kem)     echo "kasten hatalı — lineer tanı örnekleri" ;;
    # stdlib modül-import yolu gerektiren örnek (tek başına T002).
    *test/ornekler/sifrele_dosya.kem)   echo "stdlib import gerektirir — tek başına T002" ;;
    # Codegen korpusu: KASTEN tip-kontrol sınırını zorlayan dönüşüm/deref testleri.
    # Bunlar `--llvm` codegen yolunu ölçer; `--check` reddi TASARIM GEREĞİ.
    *test/cg_korpus/cg6_trunc.kem)        echo "kasıtlı daraltma (E004) — codegen trunc yolu ölçülür" ;;
    *test/cg_korpus/cg_skaler_deref.kem)  echo "kasıtlı skaler deref cast (E002)" ;;
    *test/cg_korpus/cg_deref_pointer.kem) echo "kasıtlı ham-pointer arg (T001)" ;;
    *) return 1 ;;
  esac
  return 0
}

red=0; tot=0; muaf_n=0
for f in test/cg_korpus/*.kem test/ornekler/*.kem stdlib/*.kem \
         stdlib/temel/*.kem stdlib/kripto/*.kem; do
  [ -f "$f" ] || continue
  tot=$((tot + 1))
  if "$KEMGU" --check "$f" >/dev/null 2>&1; then continue; fi
  if gerekce="$(muaf "$f")"; then
    muaf_n=$((muaf_n + 1))
    continue
  fi
  red=$((red + 1))
  echo "  🔴 $f"
  "$KEMGU" --check "$f" 2>&1 | grep -m1 "hata\[" | sed 's/^/       /'
done

echo "=== tip kontrol kapısı: $((tot - red - muaf_n))/$tot geçti, $muaf_n muaf, $red RED ==="
[ "$red" -eq 0 ] || {
  echo "🔴 Yukarıdaki dosyalar --check'ten geçmiyor. Ya düzeltin ya da"
  echo "   test/check_kapisi.sh MUAF listesine GEREKÇESİYLE ekleyin."
  exit 1
}
