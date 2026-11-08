# CODEX GÖREVİ 8 — bare-metal sürücüler + KABUK v0 (ilk entegre KEMGU-OS çekirdeği)

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## 0. Hedef
v1 bitiş tanımının ilk somut hâli: QEMU q35'te açılan bir KEMGU-OS çekirdeği diski
KENDİSİ bulur (NVMe → AHCI → USB bellek), GPT + FAT32'yi bağlar, **USB klavyeden Türkçe
komut** alır, `ls` / `cat` / `echo` çalıştırır ve çıktıyı seri porta (COM1) yazar.
Ekran (framebuffer) bu dalgada YOK — Claude paralel olarak onu yapıyor.

## 1. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-746     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex8 -b codex/kabuk-dalga8 origin/main && cd ../kemgu-codex8
make -j8 build/kemgu build/codegen build/kdl_runtime.o build/kdl_runtime_mmio.o
make calistir_bm_x86_nvme calistir_qtest      # taban sağlam mı (20/20 + 6/6)
date > otomasyon/chatgpt/codex8_ilerleme.md
```
Tek dal, tek draft PR.

## 2. Okunacaklar (bu dalganın temeli)
- `test/ornekler/kem_os_x86_nvme.kem` + `test/bm_x86_nvme_harness.sh` — bare-metal NVMe
  (D-745): PCI tarama, firmware BAR'ı, `bm_yurut`, **yırtılmayan okuma** (`bm_bellek_oku`).
- `runtime/mimari/x86_64/kem_pci_hw.kem` — port G/Ç, MMIO, fiziksel bellek, `kem_qemu_cik`.
- `runtime/kem_dizi_genis.kem` — bare-metal `Dizi`/`metin` eklemeleri.
- `drivers/ortak/gc.kem` — yürütücü sözleşmesi (BellekOku yırtılmamalı).
- `tests/qemu/usb_harness.py` — `hlt` sahte BIOS + QMP tuş olayları (dalga 6).
- `LOOP.md` D-745 (yırtık okuma), E4j, E4i/D-746.

## 3. İzin genişletmesi (yalnız bu dosyalarda)
- `güvensiz` / `çıplak` / ham işaretçi YALNIZ şunlarda: **yeni**
  `runtime/mimari/x86_64/kem_gc_yurut.kem` ve `test/ornekler/kem_os_x86_*.kem`.
  `drivers/**` saf kalır. Yeni satıriçi_asm YAZMA — `kem_pci_hw.kem`'deki ilkelleri kullan.
- Yeni harness'lar: `test/bm_x86_*_harness.sh`, `tests/qemu/*` (Python yardımcıları dahil).
- Makefile: yeni `calistir_bm_x86_*` hedefleri + `test_tumu` listesine ekleme + `.PHONY`.
- Sınıflandırma (yalnız SATIR EKLE, var olanı değiştirme): Makefile `calistir_kem_os_x86`
  CAT listesi, `test/check_kapisi.sh` (muafiyet + birleşik denetim — D-745'teki NVMe
  desenini birebir izle), `test/baremetal_diff_harness.sh`. Yeni bir `runtime/*.kem`
  dosyası bu kapılarda sınıflandırılmazsa kapı KIRMIZI olur (beklenen; ekle).
- Derleyici (`src/`, `selfhost/`), runtime C dosyaları, `boot/`, `linker/` YASAK.
  Heap (16 MB, `linker/bare-metal-x86_64.ld`) yetmezse DUR ve kaydet; linker'ı değiştirme.

## 4. İş listesi
### A — temel
- **A0 ortak yürütücü:** `bm_yurut` + `bm_bellek_oku` + `bm_yaz` (seri) + PCI tarama
  yardımcısını `runtime/mimari/x86_64/kem_gc_yurut.kem`'e taşı (davranış BİREBİR).
  `kem_os_x86_nvme.kem` onu kullansın; `make calistir_bm_x86_nvme` 20/20 kalsın.
- **A1 (T1):** dalga 6 dolanmalarını kaldır (`codex6_bulgular.md` "kalan dolanmalar";
  C6-001..004 artık main'de). `calistir_chatgpt_kopru` + `calistir_qtest` yeşil.
- **A2 (E4j) belirlenimci tamamlama okuması:** NVMe çekirdeğinde faz biti eşleşince
  girdiyi **yeniden** `BellekOku` ile oku (Linux nvme deseni: önce durum, sonra girdi).
  Sahte NVMe modeline YIRTIK yazım kipi ekle (ilk okumada yeni faz + eski CID) ve testle:
  düzeltme olmadan KIRMIZI, düzeltmeyle yeşil (belirlenimci). Aynısını xHCI olay
  halkası için (döngü biti DW3) değerlendir; gerekiyorsa uygula.

### B — bare-metal AHCI
`test/ornekler/kem_os_x86_ahci.kem` + `test/bm_x86_ahci_harness.sh` + `calistir_bm_x86_ahci`
(NVMe deseni: 20 açılış + seri + QEMU rc 33 + imaj kâhini). q35 dahili AHCI (00:1f.2),
disk `-device ide-hd,drive=…,bus=ide.0`. Firmware'in BAR5'ini (ABAR) oku.

### C — bare-metal USB (xHCI + USB bellek)
`kem_os_x86_usb.kem` + harness + hedef. VM çalışırken SeaBIOS xHCI'ye dokunur: çekirdek
denetleyiciyi SIFIRLAR (HCRST) — dalga 6 çekirdeği bunu zaten yapıyor; gerçek CPU'da
yırtık okuma (D-745) olay halkasında da görünebilir → A2 ve yırtılmayan BellekOku şart.
USB bellekte dosya oku/yaz + imaj kâhini (`fsck.fat -n` + bağımsız Python okuyucu).

### D — KABUK v0 (`kem_os_x86_kabuk.kem`)
- **Disk otomatik bulma:** NVMe → AHCI → USB bellek; bulunanı seriye yaz.
- **FAT32 diskten İSTEĞE BAĞLI okuma — tüm diski belleğe ALMA.** Ölçülen sınırlar:
  çekirdek heap'i 16 MB ve `Dizi<dtam8>` eleman başına **4 bayt** kaplıyor
  (`kdl_dizi_ekle_tam` eleman boyutuna bakmadan `int32` yazar — C runtime'ında da aynı;
  Claude kaynaktan okudu). Önerilen tasarım (sans-IO ile uyumlu): FAT32 okuma işlemi
  bir **sektör önbelleği** üzerinde çalışır; önbellekte olmayan sektöre rastlayınca
  `Gerekli(lba)` döner, çekirdek o sektörü sürücüyle okuyup önbelleğe koyar ve işlemi
  YENİDEN dener. `ls`/`cat` salt okuma olduğu için yeniden deneme güvenlidir. Var olan
  bellek-içi API (`dosya_oku(imaj, yol)`) ve testleri BOZULMAMALI. Önbellek için
  `drivers/blok/onbellek.kem` (dalga 4) kullanılabilir. Tasarımı kendin ölçerek seç;
  seçimini `codex8_bulgular.md`'ye yaz.
- **USB klavye:** xHCI interrupt IN → `hid_klavye` → `klavye_tr` → `drivers/girdi/satir.kem`
  satır düzenleyici → komut. Enter'da komut çalışır, çıktı seriye.
- **Komutlar:** `ls <yol>`, `cat <yol>`, `echo <metin>`; bilinmeyen komut açık hata.
  `drivers/sim/kabuk.kem` deseni yol gösterir (ama o bellek imajı alır).
- **Harness `test/bm_x86_kabuk_harness.sh`:** disk imajını **bağımsız** bir Python FAT32
  yazıcısıyla hazırla (KEMGU sürücüsünü KULLANMA — döngüsel kâhin olur): `/merhaba.txt`
  = "Merhaba Dünya — ğüşıöç", `/belgeler/not.txt`. Çekirdek "KABUK_HAZIR" basınca QMP
  ile `ls /` + Enter, `cat /merhaba.txt` + Enter, `cat /yok` + Enter yaz; seri çıktıyı
  BİREBİR karşılaştır. ÜÇ yapılandırmada koş: disk NVMe, AHCI, USB bellek (klavye her
  zaman USB). Her yapılandırma ≥ 5 açılış (yarışlar aralıklıdır, D-745).

### E — zaman kalırsa
- `mkdir`, `echo metin > dosya` (FAT32 yazma, blok aygıtına). Kâhin: `fsck.fat -n` + Python.
- Sahte modelleri gerçek bare-metal ölçümlerine uydur (dalga 6 G6 deseni).

## 5. Kapılar
Her işten sonra: `calistir_chatgpt_kopru`, `calistir_qtest`, `calistir_bm_x86_nvme` ve
eklediğin `calistir_bm_x86_*`. Bitirince `make test_tumu` (rc PIPESTATUS ile, atlamalar
sayılmış), `calistir_check_kapisi` ve `calistir_baremetal_diff` dahil. Her iş için
sabotaj; aralıklı sınıflarda kaç açılışta kaç kırmızı olduğunu YAZ (D-745 dersi:
5 açılışlık sessiz sabotaj, kapının zayıf olduğunu gösterebilir).

## 6. Bitirince
Draft PR `codex/kabuk-dalga8`, gövde `codex8_ilerleme.md`; `codex8_bulgular.md`:
ölçülen bare-metal davranışları, sahte model düzeltmeleri, derleyici bulguları, tasarım
seçimleri, kalanlar. Auto-merge AÇMA, merge ETME.
