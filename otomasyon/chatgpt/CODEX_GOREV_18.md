# CODEX GÖREVİ 18 — test altyapısı (hız, sağlamlık, kapı seçimi) + x86 çok çekirdek (SMP)

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## ⏱ Çalışma biçimi — bu dalga da uzundur
- Bu dalga **yirmi bölümdür, HEPSİ zorunlu ve sırayla yapılır.** İlk sekiz bölüm test altyapısıdır,
  sonraki on iki bölüm x86 SMP'dir. Altyapı bölümleri SMP bölümlerinin koşum süresini doğrudan kısaltır;
  sırayı bozma. **30+ saat normaldir.**
- Bir bölümü bitirince **durma, rapor yazıp bekleme, özet sunup onay isteme — sonraki bölüme geç.** "Bu kadar
  yeterli" deme; yirmi bölümün hepsi bitene ya da her biri için takılma notu yazılana kadar çalış.
- Her bölümün sonunda: (1) o bölümün kapıları + sabotajları, (2) ayrı commit
  (`ALTYAPI(codex): Dalga 18 bölüm N — ...` ya da `SURUCU(codex): Dalga 18 bölüm N — ...`), (3)
  `codex18_ilerleme.md`'ye kısa not (yapılan, ölçümler, sabotaj sonuçları, süre). Dalga yarıda kesilse bile
  biten bölümler kullanılabilir olmalı.
- Gerçekten takılırsan (60 dk ilerleme yok): `codex18_bulgular.md`'ye yaz, bölümün yapılabilen kısmını
  kapılı bırak ve **sonraki bölüme geç**.
- Her 5 bölümde bir (5, 10, 15) ara regresyon: `make -j10 --output-sync=target test_tumu`; kırmızı varsa
  bir sonraki bölüme geçmeden önce kökünü bul.
- **Her sabotajı gerçekten uygula, çıktısını yaz.** Uygulamadığın sabotaj varsa bunu AÇIKÇA yaz; "benzer bir
  sabotaj" ile değiştirme. Dalga 17'de "uygulanmadı" denen on üç sabotajı Claude uyguladı: ikisi ilk denemede
  **SESSİZ** kaldı (korpus korunan şekli üretmiyordu — TCP bilinmeyen seçenek boy 0, DNS iki-işaretçi
  döngüsü), üçü **ölçülmüş fazlalık** çıktı (iki koruma birbirini yedekliyor). Sabotaj sessiz kalırsa önce
  korpusu/kapıyı düzelt; fazlalıksa ikisini birden kaldırıp ayrıca ölç ve ikisini de yaz.
- **Draft PR'ı açtıktan sonra son commit'i değiştirip zorla gönderme**; ek değişiklik gerekiyorsa yeni commit.
- Draft PR'ı yalnız yirmi bölüm bittikten (ya da her biri için takılma notu yazıldıktan) sonra aç.

---

## 0. Hedef

**(A) Test altyapısı (Bölüm 1-8).** Tam takım dalga 17 sonunda seri koşumda 5 sa 25 dk sürüyordu; Claude
çekirdek önbelleği, paralel güvenlik ve eşzamanlı derleyici turlarıyla bunu **`-j10` ile 33 dk 41 sn**'ye
indirdi (D-772). Bu dalga o işi tamamlar: kalan seri darboğazları böler, kendi çekirdeğini derleyen
harness'ları ortak önbelleğe bağlar, kırılgan zamanlamaları ve sabit beklemeleri temizler, paralel güvenliği
**statik olarak** denetler, kapı başına süre bütçesi koyar, değişikliğe göre kapı seçimini kurar ve CI'yı
paralel koşturur. Hedef: tam takım **`-j10` ile ≤ 25 dk**, hiçbir kapı ≤ 10 dk'yı aşmasın, takım `-j20` stres
koşumunda da yeşil kalsın.

**(B) x86 SMP (Bölüm 9-20).** KEMGU-OS x86 bugün tek çekirdekte koşar. Bu dalga uygulama işlemcilerini (AP)
UEFI yolundan uyandırır, her çekirdeğe kendi GDT/TSS/IDT/yığın/LAPIC zamanlayıcısını verir, çekirdek
kilitlerini, işlemciler arası kesmeleri (IPI) ve TLB vurgulamasını (shootdown) ekler, fiziksel çerçeve
ayırıcısını ve heap'i SMP-güvenli yapar, kullanıcı süreçlerini birden çok çekirdekte zamanlar ve hepsini
**bağımsız kâhinle** (QMP + seri + host hesabı) `-smp 1/2/4/8` matrisinde kanıtlar.

Kapsam DIŞI: ARM SMP (ayrı dalga; `test/bare_metal/smp*_arm.c` yalnız başvuru), NUMA, x2APIC (QEMU'da
denenebiliyorsa yalnız ölç ve yaz), CPU hotplug, gerçek zamanlı çekirdek önceliği, kilitsiz (lock-free) veri
yapıları, süreç başına çok iş parçacığı (thread) — süreç tek iş parçacıklı kalır, paralellik süreçler arasıdır.

---

## 1. Bugünkü durum (Claude ölçtü — varsayma, yine de kendin doğrula)

### Test altyapısı (D-771, D-772)
- Makine: DGX Spark, **20 çekirdek** (10 Cortex-X925 + 10 Cortex-A725), QEMU 8.2.2. x86 konuklar bu ARM
  makinede **her zaman TCG** ile koşar (KVM mimari değiştirmez). `/dev/kvm` vardır ama x86 için kullanılamaz.
- `make test_tumu_olcum` (D-772): her tarif satırı `test/zaman_kabuk.sh` üzerinden koşar, sonunda
  `build/zaman_raporu.txt`: duvar saati, kapı başına toplam süre, en yavaş 15 kapı, başarısız kapılar,
  `build/` boyutu. Paralel: `make -j10 --output-sync=target test_tumu_olcum`.
- Son ölçüm (`-j10`, 154 kapı, rc=0): duvar **33 dk 41 sn**, kapı toplamı 3 sa 48 dk. En uzunlar:
  `bm_x86_ag_saglamlik` 16,6 dk · `bm_x86_kaynak` 12,9 · `bm_x86_ps2_irq` 12,7 · `bm_x86_kabuk14` 11,5 ·
  `bm_x86_v1` 10,8 · `bm_x86_ps2_karma` 10,3 · `bm_x86_cok_surec` 9,8 · `kem_os_pc_matris` 9,0 dk.
  **Kendi ölçümünle başla** (Bölüm 1); bu sayılar yük altında değişir.
- **Çekirdek önbelleği** `test/bm_x86_uefi_harness.sh` içinde: anahtar, çıktıyı belirleyen her girdinin
  içeriğinin SHA-256'sı (derleyici ikilisi, MSI yamasından sonraki birleşik kaynak, awk süzgeci, bağlayıcı
  betiği, altı başlangıç nesnesi, IR derleme komutu, clang/ld.lld sürüm satırı). İsabet önbelleksiz
  derlemeyle bayt bayt aynıdır; kapı `calistir_cekirdek_onbellek`. `KEMGU_CEKIRDEK_ONBELLEK=0` kapatır,
  `KEMGU_CEKIRDEK_ONBELLEK_KOK` kökü değiştirir. Self-host çekirdek derlemesi 74,5 sn, C 10,7 sn.
- **Önbelleğe BAĞLI OLMAYAN, çekirdeği kendisi derleyen sekiz harness:** `bm_x86_{kabuk,ahci,usb,panik,ps2,
  ekran,nvme,istisna}_harness.sh` — hepsi `build/kemgu` ile ve kendi `build/_bm_<ad>_$$` dizininde
  (`trap rm` var).
- Paralel güvenlik onarımları (D-772): UEFI harness'ı açılış kipinde koşum başına `mktemp` dizini kullanır
  (yalnız-derle kipinde `build/_bm_uefi` korunur — `tools/codex/dalga11_*_sabotaj.py` buna dayanır);
  `ps2_karma` ayrı PC imajı (`build/kem_os_pc_ps2karma.img`); `selfhost/konak.kem` süreç başına geçici ad;
  süreç harness'ı (`bm_x86_surec_harness.sh`) iki derleyicinin turunu **eşzamanlı** koşar
  (`SUREC_PARALEL=0` sıralı). `surec`/`nic`/`msi` harness'ları başarılı turun dizinini siler (D-771; önceden
  tam koşum başına ~25 GB birikiyor ve disk doluyordu).
- **Bilinen kırılganlık:** `calistir_bm_x86_program15` RTC denetimi ("saat 4 sn içinde 1 sn ilerlemeli",
  `tests/qemu/program15_kahin.py` `saat()`) bir tam koşumda kırmızı, tek başına 2/2 yeşil. `tests/qemu/*.py`
  içinde 58 `sleep` çağrısı var (Claude saydı; hepsi kusur değildir).
- **CI** (`.github/workflows/ci.yml`): Linux ve Windows işleri `test_tumu`'yu **seri** koşar; Windows'ta QEMU
  yok (x86 kapıları "ATLANDI"). Windows'ta yerel Python vardır: MSYS `/usr/bin` araçlarını çağıramaz ve
  **adında nokta olan programa `.exe` eklemez** (`ld.lld` → WinError 2; `shutil.which` ile çöz — D-771).

### SMP
- `drivers/acpi/madt.kem` var: `MadtGirdisi { tur; islemci; apic; bayraklar; … }`; tür 0 yerel APIC
  girdileridir, `bayraklar` bit 0 etkin, bit 1 çevrimiçi-yapılabilir. `runtime/mimari/x86_64/kem_apic_hw.kem`
  LAPIC zamanlayıcısını ve IOAPIC'i kurar (D-757: OVMF'de LAPIC 0xFEE00000, IOAPIC 0xFEC00000).
- Satır içi asm çekirdek `.kem`'lerinde kullanılabilir (`satıriçi_asm { mimari: x86_64 şablon: r#"…"# … }`;
  bkz. `runtime/mimari/x86_64/kem_bariyer.kem`). **AS001:** bir dosyada yalnız tek mimarinin asm'i olabilir.
  Atomik işlemler (`lock xadd`, `lock cmpxchg`, `xchg`, `pause`) bu yolla yazılır.
- **Başvuru prototipi (taşıma, çağırma):** `test/bare_metal/smp_x86.c` ve `smp4_x86.c` — PVH/`-kernel`
  yolunda INIT-SIPI-SIPI, 0x8000'e kopyalanan gerçek kip → korumalı → uzun kip trampolini. UEFI yolunda düşük
  bellekte 0x8000'in boş olduğu **garanti değildir**: trampolin sayfası UEFI bellek haritasından seçilir.
- QEMU (Claude doğruladı): `qemu-system-x86_64 -M q35 -smp 4 -accel tcg,thread=multi` bu ARM makinede uyarısız
  açılıyor. Çok iş parçacıklı TCG'nin gerçekten etkin olduğunu ve çekirdek sayısını **QMP
  `query-cpus-fast`** ile kendin doğrula (`tests/qemu/donanim_kahin.py` bu çağrıyı zaten kullanıyor).
  x86 konukta TCG, ARM ana makinede güçlü bellek sıralamasını taklit eder; yarışlar yine görünür ama
  zamanlaması gerçek donanımdan farklıdır — her SMP stres sonucunun yanına bunu yaz.
- **ABI:** son kullanılan çağrı **90** (`paket_hedef`); 40-119 v2 x86_64'e ayrılmıştır. Bu dalganın yeni
  çağrıları **91-99**.
- **Heap (E4k — açık, Claude'da):** çekirdek heap'i (`kem_bump`) geri verilmiyor ve **kilitsiz**. SMP'de
  heap'e yalnız kilitle girilir (Bölüm 14); çekirdek başı veriler, yığınlar, trampolin ve IPI kutuları
  **fiziksel çerçeve ayırıcısından sabit boyutlu, açılışta** ayrılır.

---

## 2. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep -E "D-772|D-773" | head -3   # İKİSİ de yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex18 -b codex/altyapi-smp-dalga18 origin/main && cd ../kemgu-codex18
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make -j10 --output-sync=target test_tumu_olcum          # TABAN: build/zaman_raporu.txt'yi codex18_bulgular.md'ye kopyala
date > otomasyon/chatgpt/codex18_ilerleme.md
```
Tek dal, tek draft PR. ⚠ Claude bu dalga sırasında derleyicide (E4k) çalışabilir; **dalını dalga ortasında
main ile birleştirme** — Claude entegrasyonda birleştirir. Tam takımı her zaman `-j10 --output-sync=target`
ile koş; seri koşum yalnız Bölüm 6'nın karşılaştırması için.

---

## 3. Okunacaklar
- `LOOP.md` D-765..D-773 (özellikle D-771/D-772: disk sızıntısı, önbellek, paralel yarışlar ve ölçümler),
  `otomasyon/chatgpt/codex17_bulgular.md`.
- `Makefile` (`test_tumu`, `test_tumu_olcum`, `calistir_cekirdek_onbellek`, `bm_x86_*` hedefleri),
  `test/bm_x86_*_harness.sh`, `test/zaman_kabuk.sh`, `test/zaman_raporu.py`,
  `test/cekirdek_onbellek_harness.sh`, `tests/qemu/*.py`, `.github/workflows/ci.yml`.
- `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` (H10i SMP), `KEMGU_OS_Mimari.md`, `KEMGU_OS_Syscall_ABI.md`,
  `KEMGU_OS_Sabotaj_Envanteri.md`, `X86_Fiziksel_Donanim_Kontrol_Listesi.md`.
- `drivers/acpi/madt.kem`, `runtime/mimari/x86_64/{kem_apic_hw,kem_mmu_hw,kem_ring3_hw,kem_gorev_hw,
  kem_bellek_harita,kem_bariyer,kem_uefi}.kem`, `runtime/kem_gorev_cekirdek.kem`, `runtime/kem_heap.kem`,
  `boot/start_x86_64.S`, `runtime/kdl_kesme.c` (x86 IDT bloğu), `test/ornekler/kem_os_x86_kabuk.kem`.
- Başvuru (kopyalama, çağırma): `test/bare_metal/smp_x86.c`, `smp4_x86.c`, `smp_ticket_arm.c`,
  `smp_mcs_arm.c`, `smp_barrier_arm.c`, `smp_queue_arm.c`.
- Intel SDM Cilt 3 §8.4 (MP başlatma), §10 (APIC), §4.10.4 (TLB geçersizleme) — algoritma için.

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
**Altyapı (Bölüm 1-8):**
- `test/*.sh`, `test/*.py`, `test/olcum/*`, `tests/qemu/*`, `tools/codex/*`, yeni `tools/kapi/*`.
- `Makefile` (yeni hedefler, `test_tumu`, `.PHONY`, kapı bölme). Bir kapıyı bölerken **toplam kapsam
  azalmaz**: bölünen her parça aynı denetimleri yapar; eski hedef adı parçaları önkoşul olarak tutan bir
  şemsiye hedef olarak kalır (CI ve belgeler eski adı kullanabilir).
- `.github/workflows/ci.yml`: YALNIZ `make` çağrılarına `-j` / `--output-sync` eklemek ve ölçüm raporunu
  yazdırmak. İş adlarını, kurulum adımlarını, tetikleyicileri DEĞİŞTİRME.
- `.gitignore`, `belgeler/KEMGU_Test_Altyapisi.md` (yeni).

**SMP (Bölüm 9-20):**
- `runtime/mimari/x86_64/*.kem`, `runtime/kullanici/x86_64/*.kem`, yeni `drivers/smp/` (saf, sans-IO:
  kilit modelleri, MADT işlemci süzgeci, IPI kutusu biçimi).
- **Yeni `boot/ap_x86_64.S`:** AP trampolini (16 bit gerçek kip → 32 bit → 64 bit). Makefile'da x86 çekirdek
  nesne listesine eklenir. `boot/start_x86_64.S`: YALNIZ yeni kesme vektörü koçanları (IPI vektörleri);
  `runtime/kdl_kesme.c`: YALNIZ `#if defined(__x86_64__)` IDT bloğu (ikili modda düzenle).
- `runtime/kem_gorev_cekirdek.kem`: YALNIZ x86'ya özgü dallar (`#mimari` ayrımı mevcut desene uyarak);
  **arm64 davranışı DEĞİŞMEZ** — her değişiklikten sonra `calistir_kem_os_arm` + `calistir_qemu_cekirdek`.
- `runtime/kem_heap.kem`: YALNIZ kilit sarmalı (heap algoritması E4k'dır, Claude'da). Kilit, tek çekirdekte
  (`-smp 1` ve ARM) davranışı ve heap sayılarını DEĞİŞTİRMEMELİ — kapılar ölçer.
- `test/ornekler/kem_os_x86_*.kem`, `test/kullanici/*.kem`, `test/bm_x86_*_harness.sh`, `tests/qemu/*`,
  `tests/drivers/**`.
- `belgeler/KEMGU_OS_*.md` (yeni `KEMGU_OS_SMP.md` dahil), `X86_Fiziksel_Donanim_Kontrol_Listesi.md`,
  `KEMGU_OS_Donanim_Yol_Haritasi.md` (H10i ve yeni H14 bölümü + işaretler).

**YASAK:** `src/`, `selfhost/`, `boot/efi/`, `linker/` (hepsi), runtime C dosyaları (`kdl_kesme.c`'nin x86 IDT
bloğu hariç), `test/bare_metal/*.c`. Derleyici hatası → `codex18_bulgular.md`'ye C18-xxx + küçük tekrar
üretimi (C ve self-host komutu, beklenen/gerçek sonuç), dolan. Muafiyet listelerine satır EKLEME.

---

## 5. Tasarım ilkeleri (önceki dalgalarınkiler geçerli, ek olarak)
1. **Hız bir doğruluk özelliği değildir; doğruluğu bozan hız kabul edilmez.** Her hızlandırma ya ürünü bayt
   bayt aynı bırakır (önbellek) ya da aynı denetimleri yapar (bölme, paralellik). Bölünen kapının parçaları
   toplamda eskisiyle **aynı kâhin çağrılarını** yapar; Bölüm 4 bunu sayarak kanıtlar.
2. **Paralel güvenlik bir varsayım değil, ölçümdür:** `build/` altında sabit adla yazılan her yol, sabit QEMU
   soket/port/dosya adı, ortak imaj ya da kilit dosyası envantere girer ve ya koşum başına benzersiz yapılır
   ya da (gerçekten paylaşılması gerekiyorsa) `flock` ile korunur.
3. **Bekleme olaya bağlanır, süreye değil:** sabit `sleep` yerine "şu satır/sayaç/QMP durumu gelene kadar,
   en fazla T" beklemesi. Zaman eşikleri yük altında (paralel QEMU) ölçülerek seçilir ve gerilemeyi yakalar.
4. **SMP'de tek doğru kaynak yok sayılmaz:** çekirdek sayısı MADT'den okunur ve QMP `query-cpus-fast` ile
   host'ta ayrıca doğrulanır; çekirdek başına sayaçlar (zamanlayıcı tıkı, IPI, kilit bekleme) konuğun kendi
   raporuna değil, QMP bellek okuması + seri günlüğün host'ta bağımsız hesabına dayanır.
5. **Kilit sırası belgelenir ve denetlenir.** Her çekirdek kilidinin bir sıra numarası vardır; hata ayıklama
   kipinde (`KEMGU_KILIT_DENETIM`) ters sırada edinme panik verir. Kesme işleyicisinde alınan kilitler
   kesmeleri kapatarak alınır (`kilit_al_kesmesiz`).
6. **Tek çekirdekte davranış değişmez:** `-smp 1` ile bütün mevcut kapılar birebir yeşil; heap ve boş çerçeve
   sayıları önceki dalgayla aynıdır (ölç, yaz).
7. ABI geriye uyumlu büyür (5-90 değişmez; yeni çağrılar 91-99); `tests/qemu/surec_abi.py` tutarlılık kapısı
   her yeni çağrıyı kapsar.

---

## 6. İş listesi — yirmi bölüm, HEPSİ zorunlu, sırayla

### Bölüm 1 — ölçüm tabanı, süre bütçesi ve süre gerileme kapısı
- `make -j10 --output-sync=target test_tumu_olcum`'u **üç kez** koş; kapı başına süre ortalaması ve en
  büyük sapmayı `build/` altında `zaman_taban.tsv` olarak üret, özetini `codex18_bulgular.md`'ye yaz.
- `test/kapi_butce.tsv` (izlenen): kapı adı → süre bütçesi (ölçülen en yüksek değerin 1,5 katı, en az 60 sn)
  ve kategori (host / qemu-x86 / qemu-arm / derleme).
- `test/zaman_raporu.py`'yi genişlet: bütçeyi aşan kapıları `⚠ BÜTÇE` ile işaretle; `ZAMAN_BUTCE_SIKI=1`
  ise aşım çıkış kodu 3 versin. Bütçesi olmayan yeni kapı da işaretlenir ("bütçe yok").
- Kapı `calistir_kapi_butcesi` (host): rapor ayrıştırıcısını sabit bir örnek `zaman.tsv` ile sınar (aşım,
  bütçe yok, başarısız kapı, boş kayıt).
- Sabotaj: (a) bir kapının süresine yapay `sleep 120` ekle → sıkı kipte rc=3; (b) bütçe tablosundan bir satırı
  sil → "bütçe yok" uyarısı. İkisini de yaz.

### Bölüm 2 — geçici dizin ve disk denetimi (bütün harness'lar)
- Her `test/*.sh` ve `tests/qemu/*.py` için: oluşturduğu dizin/dosyalar, başarıda temizleniyor mu,
  başarısızlıkta teşhis için kalıyor mu. Envanter `codex18_bulgular.md`'de tablo.
- Eksik temizlikleri tamamla (başarıda sil, başarısızlıkta bırak ve yolu bas). Python `tempfile` dizinleri
  `build/` altında açılır (tmpfs/`/tmp` değil).
- `test_tumu_olcum` sonunda `build/` büyümesi raporlanır; kapı `calistir_disk_denetimi`: tam takımdan önce ve
  sonra `build/` boyutu ve `build/*.XXXXXX` biçimli geride kalan dizin sayısı; başarılı takım sonunda geride
  kalan dizin **0** olmalı (önbellek kökü `build/cekirdek_onbellek` hariç).
- Sabotaj: `bm_x86_msi_harness.sh`'taki `rm -rf "$w"`'yi kaldır → kapı geride kalan dizinleri adıyla
  bildirip kırmızı.

### Bölüm 3 — ortak çekirdek önbelleği: sekiz harness
- Önbellek mantığını `test/bm_x86_uefi_harness.sh`'tan **tek kaynaklı** bir kütüphaneye taşı
  (`test/cekirdek_onbellek.sh`, `source` ile) — anahtar kuralı ve ürün listesi iki yerde YAZILMAZ (D-407).
  Anahtar hesaplaması, girdi listesi parametre alır (her harness kendi nesne listesini ve bağlayıcı betiğini
  verir).
- Sekiz harness (`kabuk, ahci, usb, panik, ps2, ekran, nvme, istisna`) bu kütüphaneyi kullanır. Birleşik
  kaynağı yamalayan harness'lar anahtarı **yamadan sonra** hesaplar.
- `calistir_cekirdek_onbellek` genişler: sekiz harness'tan en az ikisinin yolu (biri yamalı kaynaklı)
  isabet + bayt eşitliği + kaynak/derleyici/nesne değişince yeniden derleme. **Nesne değişikliği** de sınanır
  (başlangıç nesnesinin bir kopyasına bir bayt ekleyip o kopyayı veren bir yol).
- Ölçüm: tam takımda isabet/derleme sayısı (öncesi/sonrası) ve kazanç.
- Sabotaj: (a) anahtardan nesne listesini çıkar → nesne değişikliği isabet verir → kapı kırmızı; (b) yamalı
  harness'ta anahtarı yamadan ÖNCE hesapla → yamasız kaynakla aynı anahtar → kapı kırmızı.

### Bölüm 4 — kritik yolu bölme (≤ 10 dk kuralı)
- 8 dk'yı aşan her kapıyı **senaryo, derleyici ya da açılış tekrarı** ekseninde parçalara böl
  (`calistir_bm_x86_ag_saglamlik_<parça>` gibi); eski ad şemsiye hedef kalır. Hedef: hiçbir parça > 10 dk.
- **Kapsam kanıtı:** her bölünen kapı için kâhin çağrı listesi (betik adı + argümanlar + derleyici) önce ve
  sonra üretilir ve **multiset olarak eşittir**. Bunu yapan küçük araç `tools/kapi/kapsam_listesi.py`
  (Makefile'ı `make -n` ile açar) + kapı `calistir_kapi_kapsam`.
- Parçalar aynı kaynağı paylaşıyorsa (aynı imaj/ELF), paylaşım önbellekten ya da salt-okunur kopyadan olur.
- Sabotaj: bir parçadan bir senaryoyu düşür → `calistir_kapi_kapsam` eksik çağrıyı adıyla bildirip kırmızı.

### Bölüm 5 — kırılgan zamanlamalar ve sabit beklemeler
- `program15` RTC denetimini düzelt: "saat ilerledi" sorusunu QMP `qom-get` ile host saatine karşı ya da iki
  ardışık okumanın aralığına bağla; yük altında (`-j20`) **20/20** yeşil, ve RTC tıkı gerçekten durdurulunca
  (sabotaj) kırmızı.
- `tests/qemu/*.py` ve harness'lardaki her `sleep`'i sınıflandır: (i) olay beklemesine çevrilebilir, (ii)
  gerçek protokol zamanı (DHCP T1, TCP RTO…), (iii) gerekli değil. (i) ve (iii)'ü kaldır; (ii)'yi sabit
  tabloda topla ve gerekçesini yaz.
- Ortak bekleme yardımcısı (`tests/qemu/bekle.py`): koşul + üst sınır + son durum dökümü (zaman aşımında ne
  beklendiği ve en son ne görüldüğü basılır).
- Ölçüm: değiştirilen kapıların süresi öncesi/sonrası; her biri 3 koşum.
- Sabotaj: (a) RTC güncellemesini konukta durdur → `program15` kırmızı; (b) bir olay beklemesinin koşulunu
  hiç gerçekleşmeyecek bir dizeye çevir → kapı üst sınırda düşer ve son durumu basar (asılmaz).

### Bölüm 6 — paralel güvenlik: statik denetim + stres
- `tools/kapi/paylasim_denetimi.py`: harness'ları ve Makefile tariflerini tarar; `build/` altında **sabit
  adla** yazılan yolları, sabit TCP/UDP port numaralarını, sabit QEMU soket/seri/QMP dosya adlarını, ortak
  imajları listeler; aynı sabit yolu yazan iki farklı kapıyı **çakışma** olarak bildirir. Bilinçli
  paylaşımlar (önbellek kökü, `flock`'lu kaynaklar) açık bir izin listesindedir ve listedeki her satırın
  gerekçesi vardır.
- Bulunan çakışmaları gider (koşum başına benzersiz ad ya da `flock`). Kapı `calistir_paylasim_denetimi`
  (host, hızlı) `test_tumu`'ya girer.
- Stres: `make -j20 --output-sync=target test_tumu` **iki kez**; `-j10` ile aynı sonuç (yeşil) ve süre
  tablosu `codex18_bulgular.md`'ye. Bir de **seri** koşum yap ve duvar saatini yaz (karşılaştırma için tek
  seferlik).
- Sabotaj: (a) iki kapıya aynı sabit çalışma dizinini ver → statik denetim kırmızı; (b) aynı değişiklikle
  `-j10` koşumu da kırmızı (yarışın gerçekten oluştuğunu göster). İkisini de yaz.

### Bölüm 7 — değişikliğe göre kapı seçimi
- `tools/kapi/kapi_secici.py`: `git diff --name-only <taban>` → koşulacak kapılar. Eşleme bildirimsel bir
  tabloda (`test/kapi_haritasi.tsv`: yol deseni → kapı listesi). Kurallar:
  - eşlenmeyen her dosya → **tam takım** (güvenli varsayılan; sessizce kapı atlanmaz);
  - `src/`, `selfhost/`, `runtime/` (C), `Makefile`, `boot/`, `linker/` → tam takım;
  - yalnız belge (`belgeler/`, `*.md` — `LOOP.md` dahil) → yalnız belge kapıları (`calistir_bm_x86_belge15`,
    `calistir_belge_kapisi` vb. — listeyi ölç);
  - `drivers/<alan>/` → o alanın host testleri + o sürücüyü kullanan QEMU kapıları (tabloyu kaynak
    içe aktarmalarından türeten bir yardımcıyla üret, elle yazma).
- Hedef `make test_secili TABAN=origin/main` (seçilen kapıları `-j10` ile koşar, seçimi ve gerekçesini basar).
- Kapı `calistir_kapi_secici` (host): sabit örnek değişiklik kümeleri için beklenen seçimler (belge-yalnız,
  tek sürücü, derleyici, eşlenmeyen dosya → tam takım).
- **Geriye dönük doğrulama:** son 30 commit için seçicinin seçtiği kapılar, o commit'te değişen koda dokunan
  her QEMU kapısını içermeli; kaçırılan varsa tabloyu düzelt ve yaz.
- Sabotaj: tablodan bir sürücü satırını sil → o sürücü değişince kapı seçilmez → `calistir_kapi_secici`
  kırmızı.

### Bölüm 8 — CI paralelliği ve belge
- `.github/workflows/ci.yml`: Linux ve Windows `make` çağrılarına `-j$(nproc)` (Windows'ta `NUMBER_OF_PROCESSORS`)
  ve `--output-sync=target`; ölçümlü koşumu Linux işinde kullan ve `build/zaman_raporu.txt`'yi adım
  çıktısına bas. Windows yolunda paralellikte yeni kırmızı çıkarsa kökünü bul (MSYS + yerel Python
  tuzakları — §1).
- `belgeler/KEMGU_Test_Altyapisi.md`: ölçümlü koşum, bütçe, önbellek, paralel güvenlik kuralları (yeni
  harness yazarken kontrol listesi), kapı seçici, CI, bilinen sınırlar. CLAUDE.md'ye DOKUNMA (Claude günceller).
- Ölçüm: CI'da iki işin süresi öncesi/sonrası (Actions günlüğünden).

### Bölüm 9 — SMP planı, işlemci keşfi ve `-smp` kâhin ortamı
- `belgeler/KEMGU_OS_SMP.md` iskeleti: hedef, AP başlatma akışı, çekirdek başı veri, kilitler ve sıraları,
  IPI vektörleri, TLB vurgulama, zamanlayıcı modeli, kapsam dışı.
- MADT'den işlemci listesi: etkin ve çevrimiçi-yapılabilir yerel APIC girdileri (tür 0); BSP'nin APIC
  kimliği LAPIC ID yazmacından (0x20) okunur. Saf `drivers/smp/islemci.kem` süzgeç + host testi (bozuk
  girdiler, yinelenen APIC kimliği, 0 işlemci, 256'dan fazla).
- Kâhin ortamı: `tests/qemu/smp_kahin.py` yardımcıları — `-smp N -accel tcg,thread=multi`, QMP
  `query-cpus-fast` (çekirdek sayısı ve thread-id'ler), çekirdek başı sayaçları QMP bellek okumasıyla okuma.
  Çok iş parçacıklı TCG'nin etkin olduğunu (thread-id'lerin farklı olduğunu) ölç ve yaz.
- Kapı `calistir_bm_x86_smp_kesif`: `-smp 1,2,4,8`'de konuğun bildirdiği işlemci listesi = QMP'nin listesi.
- Sabotaj: MADT süzgecinde "çevrimiçi-yapılabilir" bitini yok say → `-smp` ile uyuşmazlık kırmızı (QEMU'nun
  bu biti nasıl verdiğini önce ölç; ayırt etmiyorsa sabotajı değiştir ve bunu yaz).

### Bölüm 10 — AP trampolini ve INIT-SIPI-SIPI (UEFI yolu)
- `boot/ap_x86_64.S`: 16 bit giriş → geçici GDT → korumalı kip → PAE + uzun kip (BSP'nin CR3'ü) → 64 bit
  `.kem` giriş noktası (`kem_ap_giris(cekirdek_no)`). Trampolin konumdan bağımsız yazılır ya da kopyalama
  sırasında yamanır; parametre sayfası (CR3, GDT işaretçisi, yığın tepesi, giriş adresi, çekirdek no)
  trampolinin yanında.
- Trampolin sayfası **UEFI bellek haritasından** 1 MiB altında, 4 KiB hizalı, `EfiConventionalMemory`
  türünde seçilir (SIPI vektörü = sayfa >> 12, < 0x100). Bulunamazsa açık hata ve tek çekirdekle devam.
- INIT-SIPI-SIPI: Intel SDM §8.4.4 sırası ve beklemeleri (INIT sonrası ~10 ms, SIPI'ler arası ~200 µs —
  PIT kalibreli TSC ile); ICR teslim durumu biti zaman aşımlı beklenir. Her AP çevrimiçi olunca paylaşılan
  diziye kendi APIC kimliğini yazar; BSP zaman aşımlı bekler.
- Kapı `calistir_bm_x86_smp_ap`: `-smp 2,4,8` × iki derleyici; QMP'den tüm vCPU'ların uzun kipte olduğu
  (`query-cpus-fast` + gerekirse `info registers` HMP ile CR0/EFER) ve konuğun yazdığı APIC kimlikleri.
- Sabotaj: (a) ikinci SIPI'yi kaldır — QEMU tek SIPI ile de başlatıyorsa sessiz kalır: ölç, yaz, ve yerine
  SIPI vektörünü yanlış sayfaya çevir → AP'ler çevrimiçi olmaz, kırmızı; (b) AP'ye BSP'nin yığınını ver →
  bozulma ya da zaman aşımı, kırmızı.

### Bölüm 11 — çekirdek başı veri, GDT/TSS/IDT, AP LAPIC zamanlayıcısı
- Çekirdek başı yapı (`CekirdekVeri`: no, APIC kimliği, yığın, TSS, IST yığınları, sayaçlar, şu anki süreç)
  **GS tabanında** (`IA32_GS_BASE` MSR 0xC0000101) — `cekirdek_no()` tek komutla okunur. Kullanıcı kipine
  geçişte `swapgs` disiplini (mevcut ring3 yolunu bozmadan; tek çekirdekte de doğru olmalı).
- Her AP'nin kendi TSS'i (+ IST1-3 — D-757 deseni) ve GDT girdisi; IDT ortak.
- Her AP'de LAPIC zamanlayıcısı BSP'nin PIT kalibrasyonunu kullanır; çekirdek başı tık sayacı.
- Kapı `calistir_bm_x86_smp_tik`: `-smp 4`'te her çekirdeğin tık sayacı ilerler, oranlar ±%20 içinde
  (yük altında ölç); QMP bellek okumasıyla doğrulanır.
- Sabotaj: (a) AP'lerde LAPIC zamanlayıcısını kurma → o çekirdeklerin sayacı 0, kırmızı; (b) tüm çekirdeklere
  aynı TSS → IST yığını paylaşılır; çift hata senaryosunda (D-757 kasıtlı #DF) ikinci çekirdekte bozulma —
  kapı bunu ayırt etmiyorsa senaryoyu güçlendir.

### Bölüm 12 — çekirdek kilitleri
- `runtime/mimari/x86_64/kem_kilit_hw.kem`: bilet kilidi (`lock xadd` + `pause`), `kilit_al/birak`,
  `kilit_al_kesmesiz/birak_kesmesiz` (RFLAGS.IF kaydı), deneme (`kilit_dene`), sıra numarası ve hata ayıklama
  kipinde sıra denetimi (§5.5). Okuyucu-yazıcı kilidi yalnız gerçekten gerekiyorsa.
- Saf model: `drivers/smp/kilit_model.kem` — bilet kilidinin durum makinesi; host'ta iki derleyiciyle
  rastgele serpiştirme (10⁵ adım) ile karşılıklı dışlama ve açlıksızlık (FIFO) özellikleri.
- QEMU stresi `calistir_bm_x86_smp_kilit`: `-smp 4/8`, her çekirdek ortak sayacı kilitle 10⁶ kez artırır;
  son değer = çekirdek × 10⁶ (QMP okuması). Kilitsiz sürümün kayıp verdiğini de göster (bilinçli kontrol
  deneyi, kapı dışında bir kez; TCG'de kayıp görünmüyorsa bunu yaz — zayıf kanıttır).
- Sabotaj: (a) `lock` önekini kaldır → kapı kırmızı (TCG'de görünmüyorsa açıkça yaz ve stresi güçlendir:
  daha çok çekirdek, daha çok tekrar, `thread=multi` doğrulaması); (b) sıra denetiminde ters edinme →
  hata ayıklama kipinde panik.

### Bölüm 13 — işlemciler arası kesmeler (IPI)
- IPI vektörleri (yeni koçanlar `boot/start_x86_64.S`'de): çağrı kutusu (bir çekirdekte işlev çalıştır),
  yeniden zamanla, durdur. Çekirdek başı sabit boyutlu IPI kutusu (fiziksel çerçeveden), dolu kutu belgelenmiş
  hata.
- `ipi_gonder(hedef, tur)`, `ipi_hepsine(tur)`, `ipi_bekle(…, zaman aşımı)`.
- Kapı `calistir_bm_x86_smp_ipi`: her çekirdekten her çekirdeğe 1000 çağrı; hepsi teslim, sıra çekirdek
  çifti başına korunur, kayıp 0, gecikme dağılımı (medyan, %99) yazılır.
- Sabotaj: (a) EOI'yi atla → ikinci IPI gelmez, kırmızı; (b) hedef APIC kimliği yerine çekirdek numarası kullan
  (APIC kimlikleri ardışık değilse — QEMU'da ardışıksa sessiz kalır: ölç; ardışık değil yapacak bir
  topoloji (`-smp sockets=2,cores=2,threads=1` vb.) ile tekrarla).

### Bölüm 14 — SMP-güvenli fiziksel çerçeve ayırıcısı ve heap kilidi
- Fiziksel çerçeve ayırıcısına kilit (+ isteğe bağlı çekirdek başı küçük önbellek; varsa boş çerçeve sayımı
  QMP ile tutarlı kalmalı). Heap (`kem_bump`) girişine kilit — algoritmaya dokunma.
- Kapı `calistir_bm_x86_smp_bellek`: `-smp 4`'te her çekirdek 10⁴ çerçeve al/geri ver; sonda boş çerçeve sayısı
  başlangıçla aynı, aynı çerçeve iki çekirdeğe verilmedi (çerçeve başına sahiplik bitmap'i, QMP ile).
  `-smp 1` heap ve boş çerçeve sayıları dalga 17 ile aynı (ölç, yaz).
- Sabotaj: ayırıcı kilidini kaldır → çift verme ya da sayı farkı, kırmızı (TCG'de görünmüyorsa stresi
  güçlendir ve yaz).

### Bölüm 15 — TLB vurgulama
- Bir çekirdek bir süreç adres alanında eşlemeyi kaldırınca, o adres alanını kullanan diğer çekirdeklere IPI
  ile `invlpg` (ya da CR3 yeniden yükleme) ve onay beklemesi. Hangi çekirdeğin hangi CR3'ü kullandığı çekirdek
  başı veride.
- Kapı `calistir_bm_x86_smp_tlb`: iki çekirdekte aynı süreç adres alanından bir sayfa okunur; biri eşlemeyi
  kaldırır; diğerinin bir sonraki erişimi sayfa hatası verir (kâhin seri + QMP). W^X ve süreç yalıtımı
  kapıları `-smp 4`'te yeşil.
- Sabotaj: vurgulamayı atla → diğer çekirdek eski eşlemeden okumaya devam eder; kapı kırmızı.

### Bölüm 16 — zamanlayıcı: süreçler birden çok çekirdekte
- Çekirdek başı çalışmaya hazır kuyruk + **büyük çekirdek kilidi** (sistem çağrıları ve sürücüler tek kilit
  altında — ilk SMP sürümü için kabul edilen model; belgeye gerekçesiyle yaz). Boşta kalan çekirdek başka
  kuyruktan iş çalar. Süreç göçünde CR3/TSS/GS doğru güncellenir.
- Kabukta `islemci` komutu ve `donanim` raporunda çekirdek başı satırlar: çekirdek, APIC kimliği, tık,
  çalıştırılan süreç sayısı, boşta oranı.
- ABI 91-99: `islemci_sayisi`, `islemci_no` (çağıran sürecin o anki çekirdeği), `yakinlik_ayarla` (süreci bir
  çekirdeğe bağla; geçersiz çekirdek belgelenmiş hata), `islemci_kayit` (64 bayt çekirdek başı rapor).
  `KEMGU_OS_Syscall_ABI.md` + `surec_abi.py`.
- Kapı `calistir_bm_x86_smp_surec`: `-smp 4`'te dört hesap süreci (CPU yoğun) aynı anda; QMP + seri ile
  süreçlerin farklı çekirdeklerde koştuğu, duvar saatinin `-smp 1`'e göre kısaldığı (eşik ölçülerek; TCG
  `thread=multi` gerçekten paralelse) ve her sürecin sonucunun host hesabıyla aynı olduğu. Mevcut
  `cok_surec`, `boru`, `kabuk14`, `dosyalar` senaryoları `-smp 4`'te de yeşil.
- Sabotaj: (a) iş çalmayı kapat → tek çekirdek dolu, diğerleri boşta; kapı yük dağılımından kırmızı;
  (b) göçte GS tabanını güncelleme → yanlış çekirdek verisi, kırmızı.

### Bölüm 17 — paralel kullanıcı programı ve verim ölçümü
- `test/kullanici/paralel.kem`: N süreç başlatıp (mevcut süreç oluşturma + boru ABI'si) bir hesabı (ör. büyük
  bir dizide SHA-256 parçaları ya da asal sayma) bölüştürür, sonuçları birleştirir; sonuç host'ta
  `hashlib`/Python ile bağımsız hesaplanır.
- Ölçüm: `-smp 1/2/4/8` × N=1/2/4/8 süre tablosu; hızlanma eğrisi `codex18_bulgular.md`'ye. TCG'nin gerçek
  paralelliği sınırlıysa bunu sayılarla yaz; eşik koyma, yalnız doğruluğu kapıla.
- Kapı `calistir_bm_x86_smp_paralel`: sonuç doğruluğu her matris noktasında; süre yalnız raporlanır.
- Sabotaj: birleştirmede bir parçayı atla → sonuç host hesabından farklı, kırmızı.

### Bölüm 18 — SMP sağlamlığı
- Kesme altında kilit: aygıt kesmesi (NVMe/AHCI MSI, klavye) herhangi bir çekirdeğe gelebilir; kesme
  yönlendirmesi ve kilitler bununla doğru. Kesme dengesi isteğe bağlı; önce tek çekirdeğe yönlendirmeyi
  açıkça belgele.
- Ölü kilit dedektörü: kilit beklemesi T'yi aşarsa (hata ayıklama kipinde) kilit adı, sahibi ve bekleyen
  çekirdek seri günlüğe; sürüm kipinde yalnız sayaç.
- Panik bütün çekirdekleri durdurur (IPI) ve panik ekranı tek kez çizilir.
- Kapı `calistir_bm_x86_smp_saglamlik`: `-smp 4`'te disk G/Ç + klavye + ağ + dört hesap süreci birlikte
  10 dk değil **en çok 4 dk** (bütçeye uy); heap/boş çerçeve sabit, kayıp kesme 0, ölü kilit 0. Kasıtlı panik
  senaryosunda bütün vCPU'lar durur (QMP `query-cpus-fast` + halt durumu).
- Sabotaj: (a) bir kesme işleyicisinde kesmesiz olmayan kilit al → ölü kilit dedektörü tetiklenir; (b) panikte
  IPI'yi atla → diğer çekirdekler koşmaya devam eder, kırmızı.

### Bölüm 19 — PC matrisi, regresyon ve fiziksel kontrol listesi
- `kem_os_pc_matris`'e `-smp 1/2/4` ekseni (bütçeyi aşmayacak biçimde örnekleyerek — tam çarpım gerekmez;
  seçimi ve gerekçesini yaz).
- Tüm mevcut x86 QEMU kapıları `-smp 1` ile birebir yeşil (varsayılan değişmedi); seçili 10 kapı `-smp 4` ile
  de yeşil — listesi ve sonuçları.
- `X86_Fiziksel_Donanim_Kontrol_Listesi.md`'ye SMP adımları: gerçek makinede çekirdek sayısının MADT ve
  `islemci` komutuyla doğrulanması, `paralel` programının koşumu, beklenen çıktı.
- ARM: `calistir_kem_os_arm` + `calistir_qemu_cekirdek` yeşil (paylaşılan dosyalar değiştiyse).

### Bölüm 20 — belgeler, yol haritası, sabotaj envanteri
- `KEMGU_OS_SMP.md` tamamlanır: AP başlatma, trampolin sayfası seçimi, çekirdek başı veri ve GS, kilitler ve
  sıra tablosu, IPI vektörleri ve kutu biçimi, TLB vurgulama, zamanlayıcı ve büyük çekirdek kilidi, ABI 91-99,
  ölçümler (hızlanma, IPI gecikmesi), TCG sınırları, kapsam dışı.
- `KEMGU_OS_Mimari.md`, `KEMGU_OS_Syscall_ABI.md`, `KEMGU_OS_Program_Yazma.md` (`islemci_no`/`yakinlik_ayarla`
  örneği; örnek kapıda derlenip çalışır), kabukta `yardim`.
- Yol haritası: H10i işaretlenir, yeni H14 (SMP) bölümü kanıt kapılarıyla. Sabotaj envanterine bu dalganın
  satırları (uygulanmayan varsa AÇIKÇA; sessiz kalan ve fazlalık çıkanlar ayrı işaretle).
- `belgeler/KEMGU_Test_Altyapisi.md` son hâli (Bölüm 1-8 ölçümleriyle).

---

## 7. Kapılar (her bölümden sonra)
O bölümün yeni kapıları + `calistir_cekirdek_onbellek`, `calistir_bm_x86_v1`, `calistir_bm_x86_uefi_pc`,
`calistir_bm_x86_saglamlik`, `calistir_bm_x86_cok_surec`, `calistir_bm_x86_boru`, `calistir_bm_x86_program15`,
`calistir_kem_os_x86`; paylaşılan çekirdek dosyaları (`kem_gorev_cekirdek.kem`, `kem_heap.kem`) değişirse
`calistir_kem_os_arm` + `calistir_qemu_cekirdek`; sans-IO değişikliklerinde `calistir_surucu_diff`,
`calistir_surucu_kosum`, `calistir_chatgpt_kopru`. Bölüm 5, 10, 15 ve 20'den sonra
`make -j10 --output-sync=target test_tumu_olcum` (rc PIPESTATUS ile, raporu `codex18_ilerleme.md`'ye),
`calistir_check_kapisi`, `calistir_baremetal_diff`. Yirminci bölümden sonra ayrıca bir `-j20` koşumu.

**Windows CI notları (D-768, D-771):** CI'da QEMU yok, x86 kapıları "ATLANDI" der — ama **derleme-yalnız**
yolları (`UEFI_YALNIZ_DERLE=1`) ve QEMU'suz Python adımları Windows'ta da koşar. Oralarda: MSYS `/` önekli
bayrakları yol sanır (`-` önekli biçim); Python yerel Windows Python'udur, MSYS yollarını açamaz ve adında
nokta olan programa `.exe` eklemez (`shutil.which` kullan); metin fikstürleri CRLF'e dönebilir. Yeni bir host
aracı gerektiren kapı, araç yoksa **bildirerek** atlar. QEMU'ya girdi hazırlayan hedefleri (ELF, imaj, depo)
QEMU yoksa önkoşul yapma (D-771 `PAKET_DEPOSU_ON` deseni).

**Ölçüm tuzakları:** derle-çalıştır döngüsünde ikiliyi ve kullanıcı ELF'lerini her koşumdan önce `rm -f`;
disk imajı her koşumda yeniden üretilir; `echo "$(cmd) $?"` yazma; `--checkdump` tanı bassa da 0 döner;
QEMU üçlü hatada sessizce yeniden başlar (`-no-reboot` + tek açılış işareti); kâhin anında gönderirse bekleme
kusurları görünmez; Türkçe kaynak okuyan her Python betiği `encoding="utf-8"` kullanır; sabotaj geri
alındıktan sonra ilgili `.o`/ikiliyi `rm -f` et — **önbellek kökünü de** (`build/cekirdek_onbellek`)
sabotajdan sonra temizle ya da anahtarın sabotajı kapsadığını doğrula; `make | tail` çıkış kodunu gizler
(`${PIPESTATUS[0]}`); test koşumu izlenen belgeleri DEĞİŞTİRMEMELİ (raporları `build/` altına yaz); paralel
koşumda portları ve dosya adlarını koşum başına benzersiz seç; zaman eşiklerini `-j10` yükünde ölçerek seç;
**SMP stres sonuçları TCG'de gerçek donanımdan farklı serpiştirilir** — "TCG'de görünmedi" bir kanıt değildir,
yaz ve stresi güçlendir.

---

## 8. Bitirince
Draft PR `codex/altyapi-smp-dalga18`, gövde `codex18_ilerleme.md` (yirmi bölümün her biri için: yapılanlar,
kapılar, sabotaj sonuçları — uygulanmayan sabotaj varsa AÇIKÇA, süre). `codex18_bulgular.md`: taban ve son
süre tabloları (seri / `-j10` / `-j20`), kapı başına bütçe tablosu, önbellek isabetleri, bölünen kapıların
kapsam kanıtı, sabit bekleme sınıflandırması, paylaşım envanteri, kapı seçicinin geriye dönük sonucu, SMP
ölçümleri (çekirdek keşfi, AP başlatma süresi, tık oranları, kilit stresi, IPI gecikmesi, hızlanma eğrisi,
TCG sınırları), derleyici bulguları (C18-xxx), kalanlar ve VARSAYIM'lar. Auto-merge AÇMA, merge ETME; PR
açıldıktan sonra commit'leri yeniden yazma.

## 9. Bu dalganın DIŞINDA
- E4k (heap geri kazanımı), E4q (derleme süresi), H1b (ayrık derleme), derleyici: Claude.
- ARM SMP ve ARM konukların KVM'de koşturulması (ayrı dalga), x2APIC, NUMA, CPU hotplug, süreç içi iş
  parçacıkları, kilitsiz yapılar, kesme dengeleme (isteğe bağlı, yalnız belgele), IPv6/TLS, pencere yöneticisi,
  ses, KEMFS, BIOS/GRUB, Secure Boot, fiziksel makine koşumları (Mehmet).
