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
    # MODEL B kullanıcı programı: arch-etiketli asm → bayraksız --check AS001 verir.
    # Gerçek denetimi AŞAĞIDA, `--mimari arm64` ile yapılır (muafiyet DEĞİL, doğru bayrak).
    *test/ornekler/kem_kullanici.kem)   echo "arch-etiketli asm — asagida --mimari arm64 ile denetlenir" ;;
    # KASTEN hatalı örnek: L001/L002/L004/LR002 sergiler (belgede yazılı).
    *test/ornekler/lineer_hata.kem)     echo "kasten hatalı — lineer tanı örnekleri" ;;
    # stdlib modül-import yolu gerektiren örnek (tek başına T002).
    *test/ornekler/sifrele_dosya.kem)   echo "stdlib import gerektirir — tek başına T002" ;;
    # Codegen korpusu: KASTEN tip-kontrol sınırını zorlayan dönüşüm/deref testleri.
    # Bunlar `--llvm` codegen yolunu ölçer; `--check` reddi TASARIM GEREĞİ.
    *test/cg_korpus/cg6_trunc.kem)        echo "kasıtlı daraltma (E004) — codegen trunc yolu ölçülür" ;;
    *test/cg_korpus/cg_skaler_deref.kem)  echo "kasıtlı skaler deref cast (E002)" ;;
    *test/cg_korpus/cg_deref_genislik.kem) echo "kasıtlı skaler deref cast (E002) — D-347 yük genişliği" ;;
    *test/cg_korpus/cg_pointee_isaret.kem) echo "kasıtlı skaler deref cast (E002) — AH-P pointee işaretliliği" ;;
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

# --- KEM-OS BİRLEŞİK KAYNAK (tek başına parça dosyalar yukarıda muaf) ---
# Çekirdeğin KENDİSİ dilin tip kapısından geçmeli. kem_os bir zamanlar
# `--tip-atla` ile derleniyordu: "derleme zamanı güvenlik" tezini savunan bir
# dilin işletim sistemi, kendi tip denetimini atlıyordu. Borç kapandı; bu blok
# geri düşmeyi engeller. Dosya sırası Makefile'daki `calistir_kem_os_arm`
# cat sırasıyla AYNI olmalı (kem_heap ÖNCE — çapraz-birim T002'yi kapatır).
KEMOS_PARCALAR="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem
                runtime/kem_zaman.kem runtime/kem_virtio_blk.kem
                runtime/kem_minifs.kem runtime/kem_virtio_net.kem
                runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem"
kemos_eksik=0
for p in $KEMOS_PARCALAR; do [ -f "$p" ] || kemos_eksik=1; done
if [ "$kemos_eksik" -eq 0 ]; then
  mkdir -p build
  cat $KEMOS_PARCALAR > build/kem_os_kapi.kem
  tot=$((tot + 1))
  # --mimari arm64 ŞART: satıriçi_asm arch etiketleri hedefe bağlıdır (AS001).
  if "$KEMGU" --check --mimari arm64 build/kem_os_kapi.kem >/dev/null 2>&1; then
    echo "  ✅ kem_os (birleşik kaynak, 10 parça) --check geçti"
  else
    red=$((red + 1))
    echo "  🔴 kem_os birleşik kaynak --check'ten geçmiyor:"
    "$KEMGU" --check --mimari arm64 build/kem_os_kapi.kem 2>&1 \
      | grep -m5 "hata\[" | sed 's/^/       /'
    echo "       (--tip-atla EKLEMEYİN — kök nedeni düzeltin.)"
  fi
  rm -f build/kem_os_kapi.kem
else
  echo "  ⏭  kem_os parçaları eksik — birleşik kontrol atlandı"
fi

# MODEL B kullanıcı programı: çekirdekten AYRI derlenir, kendi başına tam bir
# birimdir → doğru mimari bayrağıyla tip kapısından GEÇMELİ.
if [ -f test/ornekler/kem_kullanici.kem ]; then
  tot=$((tot + 1))
  if "$KEMGU" --check --mimari arm64 test/ornekler/kem_kullanici.kem >/dev/null 2>&1; then
    echo "  ✅ kem_kullanici.kem (Model B kullanıcı programı) --check geçti"
  else
    red=$((red + 1))
    echo "  🔴 kem_kullanici.kem --check'ten geçmiyor:"
    "$KEMGU" --check --mimari arm64 test/ornekler/kem_kullanici.kem 2>&1 \
      | grep -m3 "hata\[" | sed 's/^/       /'
  fi
fi

echo "=== tip kontrol kapısı: $((tot - red - muaf_n))/$tot geçti, $muaf_n muaf, $red RED ==="
[ "$red" -eq 0 ] || {
  echo "🔴 Yukarıdaki dosyalar --check'ten geçmiyor. Ya düzeltin ya da"
  echo "   test/check_kapisi.sh MUAF listesine GEREKÇESİYLE ekleyin."
  exit 1
}
