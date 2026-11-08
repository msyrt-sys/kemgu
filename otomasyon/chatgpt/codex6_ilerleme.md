# Dalga 6 ilerleme

Başlangıç: 2026-11-07 22:01:33 UTC. D-743 (fd449cc) origin/main 4fc5e85 üzerinde doğrulandı; 21:25–22:01 arasında beş dakikalık fetch döngüsüyle beklendi. Worktree: `/home/msyrt/Desktop/kemgu-codex6`, dal: `codex/usb-dalga6`.

- Taban: derleme rc=0; `make calistir_qtest` rc=0, NVMe ve AHCI C+self dört gerçek QEMU koşumu ve bağımsız imaj kâhini geçti.
- VARSAYIM: Görev 6’nın açık yol/Makefile/harness izinleri eski genel AGENTS.md sınırının bu göreve özgü istisnasıdır. NVMe/AHCI case dalları korunacak.

- [x] G0 · F001/F002/F003/F004/F005 dolanmaları temizlendi; gerekli C001 alias’ları gerekçesiyle korundu. Mevcut 117 köprü testi + kart ve NVMe/AHCI qtest C+self yeşil. Sabotaj: NVMe bayt kopyasında bit7 silindi → gerçek qtest rc=2 (iki derleyicide bayt10), sahte çekirdek rc=1; kaynak geri kondu → iki kapı rc=0. Eski testlerin beklentileri değiştirilmedi. Günlükler `build/codex6/G0-*.log`.

- [x] G1 · Sans-IO xHCI çekirdeği; gerçek QEMU C+self iki aygıt enumerasyonu 1.07 sn. Reset/CNR, DCBAA/scratchpad kurma, komut/olay/EP0 halkaları, port reseti, Address Device, 8+18 bayt aygıt ve tam yapılandırma, Set Configuration, Configure Endpoint. Yeni başlangıç/olay testinde geçerli CC1/IN-CC13; hatalı kod/yön/artık/adres/taşıyıcı ve DMA sınırları. Sabotaj ERST64→4: qtest rc=2, sahte başlangıç testi rc=1; geri yükleme → qtest rc=0 ve köprü kart+118 test rc=0. C6-001/C6-002 derleyici bulgularına yalnız sürücü dolanması uygulandı.

- [x] G2 · Gerçek QEMU iki aygıtın VID/PID, sınıf ve uçları ölçülüp QEMU kaynak tanımlarıyla karşılaştırıldı; gerçek DMA çıkış bağlamları Configured/Running/type/MPS denetlendi. Her derleyici için ayrıca klavyesiz taze QEMU olumsuzu: aygıt sayısı hatası, rc=1. Dal 1.12 sn. Sabotaj endpoint add maskesi→1: qtest rc=2, gerçek endpoint bağlamı reddedildi. Geri yükleme → qtest rc=0, köprü kart+118 test rc=0.

- [x] G3 · Gerçek USB BOT+SCSI üzerinden INQUIRY/READ CAPACITY, mkfs.fat imajına FAT32 ile 1300 bayt/3 kümeli DALGA6.TXT yazma, flush ve yeniden okuma; C+self ayrı QEMU, 4.37 sn. fsck.fat rc=0; bağımsız Python okuyucu iki FAT, zincir, içerik ve FSInfo/yedek sayacını doğruladı. Ölçümle Link cycle, 16 olay girdisi ve ilk TUR Unit Attention 6/41/0 düzeltildi. Üç ayrı sabotajın her biri qtest rc=2 ve sahte test rc=1; geri yükleme → iki kapı rc=0, kart+119 test. Yeni sahte test tek UA sonrası başarıyı ve sürekli UA için sınırlı hatayı korur. 22:34 UTC.

- [x] G4 · Gerçek QMP → USB interrupt IN → HID → Türkçe Q: “Ağaç İş”+Enter, iki derleyicide aynı 8 kod noktası; dal 1.65 sn. 19 gerçek rapor kalıcı sahte teste alındı; aktarım API’si sınır/yanlış durum testleri eklendi. İki sabotaj: CC13 kabulü ve ğ dönüşümü → qtest rc=2, ilgili sahte test rc=1. Geri yükleme → qtest rc=0, köprü kart+120 test rc=0. 22:39 UTC.

- [x] G5 · İsteğe bağlı gerçek usb-mouse: iki yön/iki düğme durumu, kısa IN raporu; C+self ayrı QEMU, 1.18 sn. Gerçek raporlar yeni sahte teste alındı. Y işaretini ters dönüştürmeme sabotajı → qtest rc=2, sahte rc=1. Geri yükleme → qtest rc=0, köprü kart+121 test rc=0. 22:43 UTC.

- [x] G6 · Gerçek C/self enumerasyonundan iki eşit187-isteklik iz alındı; tam sahte enumerasyon, bozuk kod5 ve kısa olay testleri geçti. ERST ve endpoint maskesi sabotajları hem qtest rc=2 hem yeni sahte test rc=1. Son qtest rc=0, köprü kart+122 test rc=0. G3/G4/G5 modelleri de gerçek davranışlarla eşlendi. C6-004 için derleyiciye dokunmadan tipli yerel dizi dolanması doğrulandı. Son incelemede G0’ın kaldırdığı tipleri yeniden üreten iki Python üreteci düzeltildi; çıktıları güncel test/fikstürlerle byte-byte aynı.

## Kapsam

G0–G6 tamamlandı; isteğe bağlı fare işi de dahil. Sans-IO xHCI enumerasyonu, gerçek USB BOT/FAT32 yazma-okuma, QMP Türkçe klavye ve fare doğrulaması; gerçek davranıştan üretilmiş sahte modeller. Derleyici/runtime ve mevcut NVMe/AHCI harness davranışı korunur. Donanım glue, fiziksel DMA doğrulaması ve eşzamanlı BOT/HID bu dalganın kapsamı dışındadır. Ayrıntılar: [bulgular](https://github.com/msyrt-sys/kemgu/blob/codex/usb-dalga6/otomasyon/chatgpt/codex6_bulgular.md).

## Test

- Son `make test_tumu`: **rc=0**, doğrudan `${PIPESTATUS[0]}` ile alındı. 22:47:42–23:00:47 UTC, 785 saniye. **FIXPOINT ✓, 97704 satır**; iki bootstrap/sürücü IR karşılaştırması kararlı.
- Köprü: kart + **122 test dosyası**, C+self. **5 yeni sürücü test dosyası** ve **4 yeni qtest dalı**. Gerçek qtest: NVMe/AHCI/xHCI/USB depo/klavye/fare, 12 olumlu ayrı QEMU koşumu; xHCI için ayrıca 2 eksik-aygıt olumsuzu. Yeni qtest dallarında atlama yok.
- Yeni dalların son toplam süreleri: xhci 1.27 sn, usb_depo 4.48 sn, usb_kbd 1.73 sn, usb_fare 1.27 sn. Her dal C ve self-host toplamı olarak 60 sn altında.
- Codegen198/198, geniş codegen62/62, modül59/59, yapısal176/176, sürücü17/17, checker304/304, bölge200/200; 38 C dosyasında0 uyarı. ASan131 PASS/0 FAIL; ASan matrisi12/12. Mevcut QEMU çekirdek: ARM64 2/2, x86_64 1/1. Yeni USB aygıt koşumları x86_64/q35 üzerinde.
- **65 korpus atlama olayı + 1 QEMU üst bellek alt senaryosu**, **52 korpus muafiyeti**; ayrıca ASan4 bilinen +2 sızıntı istisnası. Ayrıntılı döküm bulgularda. Lean/lake yok: 32 dosyada0 sorry/admit metinsel kapısı geçti; gerçek Lean tip denetimi çalışmadı.
- **11 gerçek sabotaj koşumu**, 10 eşlenmiş sahte kırmızı koşumu; dokuz farklı kuralın tümü son durumda gerçek+sahte testle korunur. Geri yükleme sonrası tüm kapılar yeşil. Eski testlerin beklenen değerleri değiştirilmedi.
- Günlük: `build/codex6/tumu.log`; SHA256 `bb82023f3d7d1b26716a2f9f78fc912f081a8539abf155a39b4a10ef36b72c97`. Çıkış kodu `build/codex6/tumu.rc`.

## Dil özellikleri

Mevcut `sonuç`, `seçimlik`, yapı/çeşit ve diziler kullanıldı. Sans-IO çekirdeğinde MMIO/DMA erişimi ve ISR yok; `tekkez`, `yetki`, `gerçekzamanlı` eklenmedi. Dört derleyici bulgusu minimal örnekleri ve zararsız dolanmalarıyla kaydedildi; derleyici değiştirilmedi.

## Dosya değişiklikleri (whitelist kontrol)

Görev6 §3 ve devralınan dalga1 izinleriyle36 dosya. Makefile yalnız dört yeni `calistir_qtest` çağrısı; `test/qtest_harness.sh` yalnız yeni case satırı. Bu satır çıkarıldığında eski harness byte-byte aynı.

```text
Makefile
drivers/acilis/multiboot2.kem
drivers/ahci/cekirdek.kem
drivers/nvme/cekirdek.kem
drivers/usb/README.md
drivers/usb/depo.kem
drivers/usb/xhci_cekirdek.kem
otomasyon/chatgpt/codex6_bulgular.md
otomasyon/chatgpt/codex6_ilerleme.md
test/qtest_harness.sh
tests/drivers/acilis/multiboot2_test.kem
tests/drivers/ahci/sahte_ahci.kem
tests/drivers/bulanik/dalga4_test.kem
tests/drivers/nvme/sahte_nvme.kem
tests/drivers/sim/depolama_matrisi_test.kem
tests/drivers/usb/depo_qemu_model_test.kem
tests/drivers/usb/fare_qemu_test.kem
tests/drivers/usb/hid_klavye_test.kem
tests/drivers/usb/hid_qemu_test.kem
tests/drivers/usb/sahte_depo.kem
tests/drivers/usb/sahte_xhci.kem
tests/drivers/usb/xhci_cekirdek_test.kem
tests/drivers/usb/xhci_qemu_fikstur.kem
tests/drivers/usb/xhci_qemu_model_test.kem
tests/qemu/fat_kahin.py
tests/qemu/usb_depo_qtest.kem
tests/qemu/usb_fare_qtest.kem
tests/qemu/usb_harness.py
tests/qemu/usb_kbd_qtest.kem
tests/qemu/xhci_iz.kem
tests/qemu/xhci_qtest.kem
tools/codex/dalga4_bulanik.py
tools/codex/dalga4_mb2.py
tools/codex/dalga6_hid_fikstur.py
tools/codex/dalga6_sabit.py
tools/codex/dalga6_xhci_fikstur.py
```

## Sonraki adım

Draft PR incelemesi, ardından fiziksel xHCI/DMA ve ortak olay halkasıyla eşzamanlı depo/HID bütünleştirmesi. Scratchpad>0 ve64 bayt bağlam gerçek aygıtta henüz ölçülmedi. Auto-merge açılmadı, merge yapılmadı, D numarası atanmadı.

Bitiş: 2026-11-07 23:01:23 UTC; çalışma alanı açılışından beri 59.8 dakika (ön koşul için36.5 dakikalık bekleme ayrıca). Atlanan iş yok; G5 dahil tüm liste tamamlandı.
