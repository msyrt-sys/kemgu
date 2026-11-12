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
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU="${KEMGU:-build/kemgu${EXE}}"
[ -x "$KEMGU" ] || KEMGU="build/kemgu"
# [D-487] EKSIK IKILI = HATA (D-486 dersinin genellemesi). Hedef
# `$(BUILD)/kemgu$(EXE)`e BAGIMLI -> make yolunda ikili GARANTI, yani bu
# dal OLU KOD; tek islevi bir YOL HATASINI SESSIZCE YUTMAKTI. Yukaridaki
# yedek satiri bu kapiyi Linux'ta KURTARDI; D-486'da sekiz kardesi
# kurtaramadi ve takim SAHTE YESIL verdi.
[ -x "$KEMGU" ] || { echo "🔴 HATA: kemgu ikilisi YOK ($KEMGU) — kapı KOŞMADI"; exit 1; }

# --- MUAF LİSTESİ (her satır: dosya|gerekçe) ---
# Muafiyet KALICI DEĞİL: gerekçe ortadan kalkarsa satır silinmeli.
muaf() {
  case "$1" in
    # Tek başına derlenemeyen PARÇA dosyalar: gerçek yapı runtime/*.kem ile
    # BİRLEŞTİRİLİP derler (Makefile kem_os_comb). Tek başına T002 doğaldır.
    # [D-710] BLAKE3 parça+ağaç: ilkeller karma.kem'de; kripto paketi (stdlib/kripto/*.kem) birleşik denetlenir (kripto_check/kosum).
    *stdlib/kripto/blake3.kem)          echo "parça dosya — kripto paketiyle birleşik denetlenir" ;;
    *test/ornekler/kem_os.kem)          echo "parça dosya — gerçek yapı birleştirilmiş kaynak" ;;
    # [D-671] x86 kanıt çekirdeği de parça: kem_dma_bariyeri runtime/mimari/x86_64'ten
    # gelir. Gerçek denetim AŞAĞIDA birleşik x86 biriminde (--mimari x86_64).
    # [D-693] ortak çekirdek fazları parçası (zamanlayıcı + kanal çekirdek uygulamasına bağlı).
    *test/ornekler/kem_os_cekirdek_ortak.kem) echo "parça dosya — arm64 ve x86 birleşik birimlerinde denetlenir" ;;
    *test/ornekler/kem_os_x86.kem)      echo "parça dosya — birleşik x86 birimi aşağıda denetlenir" ;;
    # [D-745] bare-metal NVMe test çekirdeği: kem_pci_hw/kem_uart'tan işlev ister (tek başına T002).
    *test/ornekler/kem_os_x86_kabuk.kem) echo "parça — birleşik kabuk birimi aşağıda denetlenir" ;;
    *test/ornekler/kem_os_x86_uefi.kem) echo "parça — birleşik UEFI birimi aşağıda denetlenir" ;;
    *test/ornekler/kem_os_x86_ps2.kem) echo "parça — birleşik PS2 birimi aşağıda denetlenir" ;;
    *test/ornekler/kem_os_x86_panik.kem) echo "parça — birleşik panik birimi aşağıda denetlenir" ;;
    *test/ornekler/kem_os_x86_ekran.kem) echo "parça — birleşik ekran birimi aşağıda denetlenir" ;;
    *test/ornekler/kem_os_x86_usb.kem) echo "parça — birleşik USB birimi aşağıda denetlenir" ;;
    *test/ornekler/kem_os_x86_ahci.kem) echo "parça — birleşik AHCI birimi aşağıda denetlenir" ;;
    *test/ornekler/kem_os_x86_nvme.kem) echo "parça dosya — birleşik bare-metal NVMe birimi aşağıda denetlenir" ;;
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
    # [D-534] `cg_skaler_deref.kem` SİLİNDİ — gerekçesi ("kasıtlı skaler deref
    # cast, E002") ARTIK GEÇERLİ DEĞİL: dosya `--check`ten TEMİZ geçiyor
    # (ölçüldü, 8 girdinin tek tek denetiminde çıktı). Listenin kendi kuralı:
    # *"Muafiyet KALICI DEĞİL: gerekçe ortadan kalkarsa satır silinmeli."*
    # ⚠ Bayat muafiyet ZARARSIZ DEĞİLDİR: o dosya bir gün GERÇEKTEN kırılsa
    #   kapı susardı — muafiyet listesi bir KÖR NOKTA ENVANTERİDİR (D-419).
    *test/cg_korpus/cg_deref_pointer.kem) echo "kasıtlı ham-pointer arg (T001)" ;;
    *) return 1 ;;
  esac
  return 0
}

# --- [D-469] ARCH-ETİKETLİ DOSYA KENDİ BAYRAĞIYLA DENETLENİR ---
# `satıriçi_asm` bloğundaki `mimari:` etiketi AS001'de KONAK hedefiyle
# karşılaştırılır ve D-469'dan beri varsayılan hedef DERLEME PLATFORMUNDAN
# gelir. Bayraksız `--check` bu yüzden KONAĞA BAĞLI sonuç verir: aynı dosya
# x86_64'te geçer, ARM64'te AS001 yer. DGX Spark'ta ÖLÇÜLDÜ —
# `test/cg_korpus/cg_satirici_asm.kem` (etiketi `x86_64`) kapıyı kırmızı yaptı.
#
# Bu kapının ölçtüğü şey TİP DENETİMİDİR, konak mimarisi değil. Dosyanın
# KENDİ etiketi bayrak olarak verilir — muafiyet DEĞİL, doğru bayrak; yukarıda
# `kem_kullanici.kem` için zaten uygulanan desenin genelleştirilmişi.
#
# ⚠ YALNIZ TEK ETİKETLİ dosyalarda uygulanır. Kasten BİRDEN ÇOK mimari
# karıştıran dosyalar (ör. `tc24_01_as001.kem`) AS001'in ta kendisini ölçer;
# onlara bayrak vermek testin anlamını yok ederdi. Birden çok farklı etiket
# görülürse bayrak verilmez ve dosya bayraksız denetlenir.
dosya_mimari() {
  etiketler="$(grep -oE 'mimari:[[:space:]]*(x86_64|arm64)' "$1" 2>/dev/null \
                | grep -oE '(x86_64|arm64)' | sort -u)"
  [ "$(printf '%s\n' "$etiketler" | grep -c .)" -eq 1 ] || return 1
  printf '%s' "$etiketler"
}

red=0; tot=0; muaf_n=0
for f in test/cg_korpus/*.kem test/ornekler/*.kem stdlib/*.kem \
         stdlib/temel/*.kem stdlib/kripto/*.kem; do
  [ -f "$f" ] || continue
  tot=$((tot + 1))
  if mim="$(dosya_mimari "$f")"; then
    set -- --mimari "$mim"
  else
    set --
  fi
  if "$KEMGU" --check "$@" "$f" >/dev/null 2>&1; then continue; fi
  if gerekce="$(muaf "$f")"; then
    muaf_n=$((muaf_n + 1))
    continue
  fi
  red=$((red + 1))
  echo "  🔴 $f${mim:+  (--mimari $mim)}"
  "$KEMGU" --check "$@" "$f" 2>&1 | grep -m1 "hata\[" | sed 's/^/       /'
done

# --- KEM-OS BİRLEŞİK KAYNAK (tek başına parça dosyalar yukarıda muaf) ---
# Çekirdeğin KENDİSİ dilin tip kapısından geçmeli. kem_os bir zamanlar
# `--tip-atla` ile derleniyordu: "derleme zamanı güvenlik" tezini savunan bir
# dilin işletim sistemi, kendi tip denetimini atlıyordu. Borç kapandı; bu blok
# geri düşmeyi engeller. Dosya sırası Makefile'daki `calistir_kem_os_arm`
# cat sırasıyla AYNI olmalı (kem_heap ÖNCE — çapraz-birim T002'yi kapatır).
KEMOS_PARCALAR="runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/arm64/kem_gorev_hw.kem
                runtime/kem_zaman.kem runtime/mimari/arm64/kem_zaman_hw.kem runtime/mimari/arm64/kem_bariyer.kem runtime/mimari/arm64/kem_platform.kem runtime/kem_virtio_blk.kem
                runtime/kem_minifs.kem runtime/kem_virtio_net.kem
                runtime/kem_elf.kem runtime/mimari/arm64/kem_kesif.kem runtime/kem_dtb.kem test/ornekler/kem_os_algoritma.kem test/ornekler/kem_os_cekirdek_ortak.kem test/ornekler/kem_os.kem"
kemos_eksik=0
for p in $KEMOS_PARCALAR; do [ -f "$p" ] || kemos_eksik=1; done
if [ "$kemos_eksik" -eq 0 ]; then
  mkdir -p build
  cat $KEMOS_PARCALAR > build/kem_os_kapi.kem
  tot=$((tot + 1))
  # --mimari arm64 ŞART: satıriçi_asm arch etiketleri hedefe bağlıdır (AS001).
  if "$KEMGU" --check --mimari arm64 build/kem_os_kapi.kem >/dev/null 2>&1; then
    echo "  ✅ kem_os (birleşik kaynak, 17 parça) --check geçti"
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

# [D-671] x86 kanıt çekirdeği birleşik birimi — Makefile `calistir_kem_os_x86`
# cat sırasıyla AYNI olmalı. Mimari başına dosya (D2b) seçimi burada da x86_64.
KEMOS_X86_PARCALAR="runtime/kem_heap.kem runtime/kem_zaman.kem runtime/mimari/x86_64/kem_zaman_hw.kem runtime/kem_gorev_cekirdek.kem runtime/mimari/x86_64/kem_gorev_hw.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem
                    runtime/mimari/x86_64/kem_platform.kem runtime/kem_virtio_blk.kem
                    runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/mimari/x86_64/kem_pvh.kem runtime/mimari/x86_64/kem_kesif.kem runtime/mimari/x86_64/kem_mmu_hw.kem
                    runtime/mimari/x86_64/kem_ring3_hw.kem runtime/kem_dtb.kem runtime/mimari/x86_64/kem_pci_hw.kem test/ornekler/kem_os_algoritma.kem test/ornekler/kem_os_cekirdek_ortak.kem test/ornekler/kem_os_x86.kem"
kemos_x86_eksik=0
for p in $KEMOS_X86_PARCALAR; do [ -f "$p" ] || kemos_x86_eksik=1; done
if [ "$kemos_x86_eksik" -eq 0 ]; then
  mkdir -p build
  cat $KEMOS_X86_PARCALAR > build/kem_os_x86_kapi.kem
  tot=$((tot + 1))
  if "$KEMGU" --check --mimari x86_64-none build/kem_os_x86_kapi.kem >/dev/null 2>&1; then
    echo "  ✅ kem_os_x86 (birleşik kaynak, $(echo $KEMOS_X86_PARCALAR | wc -w) parça, --mimari x86_64-none) --check geçti"
  else
    red=$((red + 1))
    echo "  🔴 kem_os_x86 birleşik kaynak --check'ten geçmiyor:"
    "$KEMGU" --check --mimari x86_64-none build/kem_os_x86_kapi.kem 2>&1 \
      | grep -m5 "hata\[" | sed 's/^/       /'
  fi
  rm -f build/kem_os_x86_kapi.kem
else
  red=$((red + 1))
  echo "  🔴 kem_os_x86 parçaları eksik — birleşik x86 kontrolü KOŞAMADI"
fi

# [D-745] bare-metal NVMe test çekirdeği birleşik birimi — test/bm_x86_nvme_harness.sh
# CAT sırasıyla AYNI (o harness'taki RT listesi + çekirdek).
BM_NVME_PARCALAR="$(echo $KEMOS_X86_PARCALAR | tr ' ' '\n' | grep '^runtime/' | grep -v kem_pci_hw | tr '\n' ' ') runtime/kem_dizi_genis.kem runtime/mimari/x86_64/kem_pci_hw.kem test/ornekler/kem_os_x86_nvme.kem"
mkdir -p build
BM_NVME_PARCALAR="$BM_NVME_PARCALAR runtime/mimari/x86_64/kem_gc_yurut.kem"
cat $BM_NVME_PARCALAR > build/kem_bm_nvme_kapi.kem
tot=$((tot + 1))
if "$KEMGU" --check --mimari x86_64-none build/kem_bm_nvme_kapi.kem >/dev/null 2>&1; then
  echo "  ✅ kem_os_x86_nvme (birleşik bare-metal NVMe, $(echo $BM_NVME_PARCALAR | wc -w) parça) --check geçti"
else
  red=$((red + 1))
  echo "  🔴 kem_os_x86_nvme birleşik kaynak --check'ten geçmiyor:"
  "$KEMGU" --check --mimari x86_64-none build/kem_bm_nvme_kapi.kem 2>&1 | grep -m5 "hata\[" | sed 's/^/       /'
fi
rm -f build/kem_bm_nvme_kapi.kem

BM_AHCI_PARCALAR="${BM_NVME_PARCALAR/test\/ornekler\/kem_os_x86_nvme.kem/test\/ornekler\/kem_os_x86_ahci.kem}"
cat $BM_AHCI_PARCALAR > build/kem_bm_ahci_kapi.kem
tot=$((tot + 1))
if "$KEMGU" --check --mimari x86_64-none build/kem_bm_ahci_kapi.kem >/dev/null 2>&1; then
  echo "  ✅ kem_os_x86_ahci (birleşik bare-metal AHCI, $(echo $BM_AHCI_PARCALAR | wc -w) parça) --check geçti"
else
  red=$((red + 1))
  echo "  🔴 kem_os_x86_ahci birleşik kaynak --check'ten geçmiyor:"
  "$KEMGU" --check --mimari x86_64-none build/kem_bm_ahci_kapi.kem 2>&1 | grep -m5 "hata\[" | sed 's/^/       /'
fi
rm -f build/kem_bm_ahci_kapi.kem

BM_USB_PARCALAR="${BM_NVME_PARCALAR/test\/ornekler\/kem_os_x86_nvme.kem/test\/ornekler\/kem_os_x86_usb.kem}"
cat $BM_USB_PARCALAR > build/kem_bm_usb_kapi.kem
tot=$((tot + 1))
if "$KEMGU" --check --mimari x86_64-none build/kem_bm_usb_kapi.kem >/dev/null 2>&1; then
  echo "  ✅ kem_os_x86_usb (birleşik bare-metal USB, $(echo $BM_USB_PARCALAR | wc -w) parça) --check geçti"
else
  red=$((red + 1))
  echo "  🔴 kem_os_x86_usb birleşik kaynak --check'ten geçmiyor:"
  "$KEMGU" --check --mimari x86_64-none build/kem_bm_usb_kapi.kem 2>&1 | grep -m5 "hata\[" | sed 's/^/       /'
fi
rm -f build/kem_bm_usb_kapi.kem

BM_KABUK_PARCALAR="${BM_NVME_PARCALAR/test\/ornekler\/kem_os_x86_nvme.kem/test\/ornekler\/kem_os_x86_kabuk.kem}"
BM_KABUK_PARCALAR="$BM_KABUK_PARCALAR runtime/mimari/x86_64/kem_ekran_bga.kem"   # [D-749] ortak ekran modülü
BM_KABUK_PARCALAR="$BM_KABUK_PARCALAR runtime/mimari/x86_64/kem_ps2_hw.kem"
BM_KABUK_PARCALAR="$BM_KABUK_PARCALAR runtime/mimari/x86_64/kem_uefi.kem runtime/mimari/x86_64/kem_ekran_gop.kem runtime/mimari/x86_64/kem_bellek_harita.kem runtime/mimari/x86_64/kem_acpi_uefi.kem"
cat $BM_KABUK_PARCALAR > build/kem_bm_kabuk_kapi.kem
tot=$((tot + 1))
if "$KEMGU" --check --mimari x86_64-none build/kem_bm_kabuk_kapi.kem >/dev/null 2>&1; then
  echo "  ✅ kem_os_x86_kabuk (birleşik bare-metal KABUK, $(echo $BM_KABUK_PARCALAR | wc -w) parça) --check geçti"
else
  red=$((red + 1))
  echo "  🔴 kem_os_x86_kabuk birleşik kaynak --check'ten geçmiyor:"
  "$KEMGU" --check --mimari x86_64-none build/kem_bm_kabuk_kapi.kem 2>&1 | grep -m5 "hata\[" | sed 's/^/       /'
fi
rm -f build/kem_bm_kabuk_kapi.kem

# [D-748] bare-metal ekran (std VGA + Bochs DISPI + PSF) — kabuk deseninin aynısı.
BM_EKRAN_PARCALAR="${BM_NVME_PARCALAR/test\/ornekler\/kem_os_x86_nvme.kem/test\/ornekler\/kem_os_x86_ekran.kem}"
BM_EKRAN_PARCALAR="$BM_EKRAN_PARCALAR runtime/mimari/x86_64/kem_ekran_bga.kem"   # [D-749] ortak ekran modülü
cat $BM_EKRAN_PARCALAR > build/kem_bm_ekran_kapi.kem
tot=$((tot + 1))
if "$KEMGU" --check --mimari x86_64-none build/kem_bm_ekran_kapi.kem >/dev/null 2>&1; then
  echo "  ✅ kem_os_x86_ekran (birleşik bare-metal EKRAN, $(echo $BM_EKRAN_PARCALAR | wc -w) parça) --check geçti"
else
  red=$((red + 1))
  echo "  🔴 kem_os_x86_ekran birleşik kaynak --check'ten geçmiyor:"
  "$KEMGU" --check --mimari x86_64-none build/kem_bm_ekran_kapi.kem 2>&1 | grep -m5 "hata\[" | sed 's/^/       /'
fi
rm -f build/kem_bm_ekran_kapi.kem

# [D-748] bare-metal panik (std VGA + Bochs DISPI + PSF) — kabuk deseninin aynısı.
BM_PANIK_PARCALAR="${BM_NVME_PARCALAR/test\/ornekler\/kem_os_x86_nvme.kem/test\/ornekler\/kem_os_x86_panik.kem}"
BM_PANIK_PARCALAR="$BM_PANIK_PARCALAR runtime/mimari/x86_64/kem_ekran_bga.kem"   # Ortak ekran modülü; panik birleşik biriminde de denetlenir.
cat $BM_PANIK_PARCALAR > build/kem_bm_panik_kapi.kem
tot=$((tot + 1))
if "$KEMGU" --check --mimari x86_64-none build/kem_bm_panik_kapi.kem >/dev/null 2>&1; then
  echo "  ✅ kem_os_x86_panik (birleşik bare-metal PANIK, $(echo $BM_PANIK_PARCALAR | wc -w) parça) --check geçti"
else
  red=$((red + 1))
  echo "  🔴 kem_os_x86_panik birleşik kaynak --check'ten geçmiyor:"
  "$KEMGU" --check --mimari x86_64-none build/kem_bm_panik_kapi.kem 2>&1 | grep -m5 "hata\[" | sed 's/^/       /'
fi
rm -f build/kem_bm_panik_kapi.kem

BM_PS2_PARCALAR="${BM_NVME_PARCALAR/test\/ornekler\/kem_os_x86_nvme.kem/test\/ornekler\/kem_os_x86_ps2.kem}"
BM_PS2_PARCALAR="$BM_PS2_PARCALAR runtime/mimari/x86_64/kem_ekran_bga.kem"   # Ortak ekran modülü; PS/2 birleşik biriminde de denetlenir.
BM_PS2_PARCALAR="$BM_PS2_PARCALAR runtime/mimari/x86_64/kem_ps2_hw.kem"
cat $BM_PS2_PARCALAR > build/kem_bm_ps2_kapi.kem
tot=$((tot + 1))
if "$KEMGU" --check --mimari x86_64-none build/kem_bm_ps2_kapi.kem >/dev/null 2>&1; then
  echo "  ✅ kem_os_x86_ps2 (birleşik bare-metal PS2, $(echo $BM_PS2_PARCALAR | wc -w) parça) --check geçti"
else
  red=$((red + 1))
  echo "  🔴 kem_os_x86_ps2 birleşik kaynak --check'ten geçmiyor:"
  "$KEMGU" --check --mimari x86_64-none build/kem_bm_ps2_kapi.kem 2>&1 | grep -m5 "hata\[" | sed 's/^/       /'
fi
rm -f build/kem_bm_ps2_kapi.kem

BM_UEFI_PARCALAR="${BM_NVME_PARCALAR/test\/ornekler\/kem_os_x86_nvme.kem/test\/ornekler\/kem_os_x86_uefi.kem} runtime/mimari/x86_64/kem_uefi.kem runtime/mimari/x86_64/kem_ekran_bga.kem runtime/mimari/x86_64/kem_ekran_gop.kem runtime/mimari/x86_64/kem_bellek_harita.kem runtime/mimari/x86_64/kem_acpi_uefi.kem"
cat $BM_UEFI_PARCALAR > build/kem_bm_uefi_kapi.kem
tot=$((tot + 1))
if "$KEMGU" --check --mimari x86_64-none build/kem_bm_uefi_kapi.kem >/dev/null 2>&1; then
 echo "  ✅ kem_os_x86_uefi birleşik --check geçti"
else
 red=$((red + 1)); echo "  🔴 kem_os_x86_uefi birleşik --check başarısız"
 "$KEMGU" --check --mimari x86_64-none build/kem_bm_uefi_kapi.kem
fi
rm -f build/kem_bm_uefi_kapi.kem

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
