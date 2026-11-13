# CODEX GÖREVİ 11 — kesme altyapısı: GDT/TSS/IST, istisna panik ekranı, MADT + IOAPIC + LAPIC, PS/2 IRQ1/IRQ12 + fare

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## 0. Hedef
KEMGU-OS bugün kesmeleri yarım kullanıyor: IDT var (`runtime/kdl_kesme.c` x86 bloğu,
istisnalar 0-31 + IRQ0 + `int 0x80`), ama **IST yığını yok** (çift hata #DF, bozuk yığında
üçlü hataya → QEMU sıfırlanır, iz kalmaz), **istisnalar yalnız seriye** yazıyor (ekran boş
kalır), zamanlayıcı eski **8259 PIC + PIT** üzerinden, klavye **yoklamalı** (D-752) ve **PS/2
fare hiç bağlı değil**. Gerçek bir PC'de (yol haritası H0g: sıradan UEFI dizüstü/masaüstü)
bunların hepsi gerekir.

Bu dalganın sonunda: (1) kasıtlı #PF, #GP, #UD, #DE ve **#DF** (bozuk yığınla) her biri
ekranda kırmızı PANIK + vektör/hata kodu/RIP/CR2, seride aynı satır, sistem `hlt` ile durur;
(2) dizi sınır ihlali de aynı ekrana çıkar (E4n); (3) UEFI yolunda kesme denetleyicisi
**ACPI MADT'den** bulunan **IOAPIC + LAPIC**; zamanlayıcı LAPIC; (4) PS/2 klavye **IRQ1** ve
PS/2 fare **IRQ12** kesmeyle, kesme-güvenli halka üzerinden; kabuk fare olaylarını görür.

Yol haritası maddeleri: H2e, E4n, H1g (kısmen), H3c, H3d, H3e, H5b, H6a, H6b, H6f (kısmen).

## 1. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-756     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex11 -b codex/kesme-dalga11 origin/main && cd ../kemgu-codex11
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_bm_x86_panik calistir_bm_x86_ps2 calistir_bm_x86_uefi_kabuk calistir_kem_os_x86   # taban sağlam mı
date > otomasyon/chatgpt/codex11_ilerleme.md
```
Tek dal, tek draft PR.

## 2. Okunacaklar
- `boot/start_x86_64.S`: GDT (`0x28` 64 bit TSS tanımlayıcısı, D-699), `kdl_x86_tss` (104 B),
  `isr_ortak` (ring3 dönüşü D-699, kurtarılabilir #PF D-687/D-689 → aksi `kdl_istisna_isle`),
  IRQ0 koçanı (PIC EOI `outb 0x20`), iki giriş noktası (PVH + UEFI, ikisi de `kdl_idt_kur`).
- `runtime/kdl_kesme.c` `#if defined(__x86_64__)` bloğu: `KdlIdtKapi`, `kdl_idt_gate`
  (`ist = 0` sabit), `kdl_idt_kur`.
- `runtime/mimari/x86_64/kem_ring3_hw.kem` `kxr_tss_kur` (TSS doldurma + `ltr`).
- `runtime/mimari/x86_64/kem_mmu_hw.kem`: `kdl_istisna_isle` (yalnız seri, `kis_*`
  yardımcıları — HEAP KULLANMAZ), `kdl_panik` (ekran kaydı `ekran_panik_taban`, bayrak
  1346457161, yeniden girişte yalnız seri).
- `runtime/kem_heap.kem` `kdl_dizi_oob` (sonsuz döngü, mesaj YOK) + `kem_panik`.
  ⚠ `kem_heap.kem` hem arm64 `kem_os` hem x86 birleşik birimlerine `cat` ile girer
  (`grep -n kem_heap.kem Makefile`); arm64 tarafında `kdl_panik` YOKTUR (D-592).
- `runtime/mimari/x86_64/kem_ps2_hw.kem` (D-752 yoklamalı i8042), `drivers/ps2/{klavye_tr,fare}.kem`,
  `drivers/acpi/madt.kem` (dalga 1 sans-IO ayrıştırıcı), `runtime/mimari/x86_64/kem_acpi_uefi.kem`.
- `tests/qemu/bm_panik.py`, `bm_ps2.py`, `uefi_kabuk_kahin.py`, `acpi_uefi_kahin.py`, `psf_kahin.py`.
- `LOOP.md` D-747..D-756; yol haritası `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` H1g, H2e, H3, H5, H6.

## 3. İzin genişletmesi (yalnız bu dosyalarda)
- `boot/start_x86_64.S`: IST yığını(ları) için `.bss` alanı; yeni kesme koçanları (IRQ1,
  IRQ12, LAPIC zamanlayıcı, sahte kesme vektörü); EOI'nin LAPIC'e taşınması. **PVH ve UEFI
  girişleri ve mevcut ring3 / kurtarılabilir-#PF yolları birebir korunur** (kapılar onları ölçer).
- `runtime/kdl_kesme.c`: YALNIZ `#if defined(__x86_64__)` IDT bloğu (IST alanı, yeni kapılar).
  Dosyanın geri kalanı (arm64 syscall'ları) YASAK. ⚠ Dosyayı ikili (bytes) modda düzenle.
- `runtime/mimari/x86_64/*.kem` (yeni dosyalar dahil: ör. `kem_apic_hw.kem`), `runtime/mimari/arm64/`
  içinde YALNIZ E4n kancasının arm64 karşılığı (davranış DEĞİŞMEZ: bugünkü gibi sessiz dur).
- `runtime/kem_heap.kem` içinde YALNIZ `kdl_dizi_oob` gövdesi (mimari kancasını çağırsın).
- `drivers/**` saf kalır (sans-IO); fare paketi/olay ayrıştırıcısı orada genişleyebilir.
- `test/ornekler/kem_os_x86_*.kem`, `test/bm_x86_*_harness.sh`, `tests/qemu/*`, `tools/codex/*`.
- Makefile (yeni `calistir_bm_x86_*` + `test_tumu` + `.PHONY` + x86 CAT listesi), `test/check_kapisi.sh`
  ve `test/baremetal_diff_harness.sh` sınıflandırma satırları.
- YASAK: `src/`, `selfhost/`, `linker/`, `boot/efi/`, diğer runtime C dosyaları. Derleyici
  hatası → `codex11_bulgular.md`'ye C11-xxx + küçük tekrar üretimi, dolan. Muafiyet listelerine
  (`bolge_operand ENV_MUAF`, `check_genis MUAF`, `yapi_diff`, `stdlib_ithal MUAF`) satır EKLEME.

## 4. İş listesi
### A — GDT + TSS + IST (H2e)
- TSS'e IST1 (en az #DF için; NMI ve #MC için ayrı IST önerilir — seçimi yaz) yığını koy,
  `kdl_idt_gate`'e IST parametresi ekle. Sabitler (Claude Linux çekirdek başlıklarından
  doğruladı, `arch/x86/include/asm/processor.h` `struct x86_hw_tss` + `desc_defs.h`
  `struct gate_struct`): 64 bit TSS'te `reserved1`(4) `sp0`(8) `sp1`(8) `sp2`(8)
  `reserved2`(8) → **IST1 ofset 0x24**, `ist[7]` 8'er bayt; kapıda `ist` = `bits` alanının
  **düşük 3 biti**. Yine de çalışan QEMU'da ÖLÇ (yanlış IST = #DF'de üçlü hata = sıfırlama).
- IST yığınlarının taşmasını gözetle (koruma sayfası ya da kanarya; seçimi yaz).
- Kanıt: aşağıdaki B'nin #DF şekli.

### B — istisna ekranı (H2e kanıtı) + E4n
- `kdl_istisna_isle` ekran açıksa `kdl_panik`'in kırmızı ekranını kullansın: "PANIK" + bir satır
  (`ISTISNA vek=0x.. hata=0x.. rip=0x.. cr2=0x..`, adlarıyla: #PF/#GP/#UD/#DE/#DF). Seri satırı
  BİREBİR korunur (bugünkü kâhinler okuyor). ⚠ İstisna yolunda **heap YOK** (heap bozuk
  olabilir; #DF'de yığın bozuk): mesajı sabit bir tampona yaz (`kis_buf_yaz8` deseni).
- Kasıtlı istisna çekirdekleri (ayrı açılışlar): #DE (sıfıra bölme — ⚠ KEMGU `a/0`'ı çalışma
  zamanında yakalar (D-502), gerçek `div` için `satıriçi_asm`), #UD (`ud2`), #GP (ayrıcalıklı
  olmayan bir ihlal ya da kanonik olmayan adres), #PF (eşlenmemiş adres, beklenmeyen),
  **#DF** (RSP'yi eşlenmemiş bir adrese koyup bir istisna tetikle → itme başarısız → #DF →
  IST1). Her biri: screendump kırmızı zemin + iki satır metin birebir (bağımsız Python
  modeliyle, `psf_kahin`), seri satırı, QEMU sıfırlanmadı (`-no-reboot` + açılış sayacı ile ÖLÇ).
- **E4n:** dizi sınır ihlali de aynı ekrana çıksın. `kdl_dizi_oob` mimari kancası çağırsın
  (`kdl_mimari_dizi_oob(i, boyut)` gibi); x86'da `kdl_panik("dizi sinir ihlali (i=.., boyut=..)")`
  — mesaj yine heap'siz; arm64 kancası bugünkü sessiz döngü. `kem_heap.kem`'i `cat` eden HER
  birleşik birim yeni sembolü bulmalı (`grep` ile say, bağlanmayan kalmasın). `calistir_kem_os_arm`
  (arm64, 24 faz) ve `calistir_qemu_cekirdek` YEŞİL kalmalı.
- Sabotaj: (1) IST'yi 0 yap → #DF şekli üçlü hata / sıfırlama → kapı KIRMIZI; (2) ekran yolunu
  kapat → screendump KIRMIZI; (3) E4n kancasını boş bırak → ekran KIRMIZI.

### C — MADT + IOAPIC + LAPIC (H3c, H3d, H3e) — UEFI yolu
- `drivers/acpi/madt.kem` ayrıştırıcısını UEFI'nin RSDP'sinden bulunan MADT'ye bağla: LAPIC
  adresi, IOAPIC(ler) + GSI tabanı, **kesme kaynağı geçersiz kılmaları** (ISA IRQ → GSI, kutup,
  tetik). QEMU q35'te IRQ0'ın hangi GSI'ye gittiğini ÖLÇ, varsayma. Kâhin: MADT'yi KEMGU'dan
  bağımsız Python RAM dökümünden okusun (`acpi_uefi_kahin.py` deseni) ve seri satırıyla karşılaştırsın.
- Adresleri eşle (önbelleksiz). ⚠ D-755: OVMF 64 bit BAR'ları 4 GiB üstüne koyuyor, UC 2 MiB
  eşleme var; LAPIC/IOAPIC'in eşli olduğunu ÖLÇ.
- 8259 PIC'i maskele (her iki yonga), IOAPIC yönlendirme tablosu (maske, vektör, tetik/kutup),
  LAPIC etkinleştir (sahte kesme vektörü), EOI LAPIC'e. LAPIC zamanlayıcıyı PIT ile kalibre et
  (H3e), kalibrasyon sonucunu seriye yaz. Kanıt: mevcut zaman/tik kapıları UEFI yolunda LAPIC ile
  yeşil; kabuk boşta bekleme (≥8 sn tuşsuz) düşmüyor.
- **PVH yolu:** değiştirmek zorunda değilsin; değiştirirsen mevcut PVH kapıları (`calistir_kem_os_x86`
  dahil) yeşil kalmalı. Seçimini yaz.
- Sabotaj: ISO geçersiz kılmasını yok say → zamanlayıcı kesmesi gelmez → kapı KIRMIZI.

### D — PS/2 kesmeleri + fare (H5b, H6a, H6b, H6f kısmen)
- IRQ1 (klavye) ve IRQ12 (fare) IOAPIC üzerinden; işleyici yalnız baytı **kesme-güvenli halkaya**
  koyar (tek üretici = ISR, tek tüketici = kabuk; taşmada say ve bildir, sessiz düşürme YOK).
  Yoklamalı yol (D-752) silinmez ama kesme yolu varsayılan olur; seçimi kabuk açılışında yaz.
- Aux port (`0xA8`, `0xD4` öneki), fare varsayılanları, akış etkinleştir; 3 baytlık paket,
  **senkron kurtarma** (bit 3 her ilk baytta 1 — bozuk akışta yeniden hizalan). Tekerlek (H6c)
  zaman kalırsa.
- Kabuk: fare olayını (dx, dy, düğmeler) seriye yazsın; ekranda basit bir imleç (H6e'nin
  yalnız temel hali; save-under ister — kural `psf_kahin`'e de eklenmeli ki kâhin aynı ekranı
  üretsin).
- Kâhin: QMP `input-send-event` ile `rel` hareket + düğme; klavye D-752'deki komut dizisiyle
  (bu kez kesme yolu), seri + ekran birebir. Hızlı yazma (ardışık 40+ tuş) halkayı taşırmamalı.
- Sabotaj: (1) IRQ12 maskeli bırak → fare olayı yok → KIRMIZI; (2) senkron kurtarmayı kapat +
  kâhine bir kayık bayt enjekte et → olaylar bozulur → KIRMIZI.

### E — zaman kalırsa
- H3f FADT (`8042` bayrağı: PS/2 var mı?) + H3g kapatma/yeniden başlatma (FADT reset yazmacı →
  8042 → üçlü hata sırası); kabukta `kapat` / `yenidenbaslat`. QEMU'da `-no-reboot` ile ÖLÇ.

## 5. Kapılar
Her işten sonra: yeni `calistir_bm_x86_*` kapıların + `calistir_bm_x86_panik`,
`calistir_bm_x86_ps2`, `calistir_bm_x86_kabuk`, `calistir_bm_x86_uefi_kabuk`,
`calistir_bm_x86_uefi_pc`, `calistir_kem_os_x86`, `calistir_qemu_cekirdek` (arm64 — E4n
kancası onu da etkiler). Bitirince `make test_tumu` (rc PIPESTATUS ile, atlamalar sayılmış),
`calistir_check_kapisi`, `calistir_baremetal_diff` dahil. Her iş için sabotaj; aralıklı
sınıflarda (kesme zamanlaması!) kaç açılışta kaç kırmızı olduğunu YAZ — kesme kâhinlerini en az
10 açılışla koş.

**Ölçüm tuzakları:** derle-çalıştır döngüsünde ikiliyi her koşumdan önce `rm -f`; `echo "$(cmd) $?"`
yazma; `--checkdump` tanı bassa da 0 döner ("OK" satırını ara); kâhin komutu ANINDA yazarsa
boşta bekleme kusurlarını göremez; QEMU üçlü hatada sessizce yeniden başlar — `-no-reboot` olmadan
"kapı geçti" bir İDDİADIR.

## 6. Bitirince
Draft PR `codex/kesme-dalga11`, gövde `codex11_ilerleme.md`; `codex11_bulgular.md`: ölçülen
donanım davranışları (MADT girdileri ve ISO'lar, IRQ0/1/12 GSI'leri, LAPIC kalibrasyonu, PS/2
fare kimliği ve paket biçimi), IST yerleşimi, derleyici bulguları (C11-xxx), tasarım seçimleri,
kalanlar. Auto-merge AÇMA, merge ETME.

## 7. Bu dalganın DIŞINDA (kaydedildi, yapma)
- BIOS/GRUB + multiboot2 yolu: **ertelendi (D-756)**; v1 yalnız UEFI.
- E4k (bölge modeli, uzun ömürlü durum): dil kararı escape v3, H9f metin düzenleyiciden önce.
- SMP / LAPIC INIT-SIPI (H10i), MSI/MSI-X (H7d), USB HID'i kesmeye geçirme.
