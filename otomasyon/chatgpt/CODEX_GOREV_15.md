# CODEX GÖREVİ 15 — v1 sonrası: yol haritası denetimi, sağlamlık, teşhis, QEMU matrisi, bulanıklaştırma, giriş ayrıntıları, yeni programlar, boru hatları, belgeler

Dalga 1'in kuralları geçerlidir (`otomasyon/chatgpt/CODEX_GOREV.md` §2, §3, §5 — yeniden
oku), aşağıdaki **izin genişletmesi** dışında. Dalga 6'nın gerçek-QEMU kâhin kuralı da
geçerlidir (`CODEX_GOREV_6.md` §0). Soru sorma, durma; takılırsan (60 dk) kaydet ve geç.

## ⏱ Bu dalga ÇOK UZUNDUR — çalışma biçimi
- Dalga 14 sekiz bölümü yaklaşık bir günde bitirdi ve iyi sonuç verdi. Bu dalga **on bir bölümdür, HEPSİ
  zorunlu ve sırayla yapılır.** Beklenen süre dalga 14'ün en az iki katıdır; 12+ saat normaldir.
  Bir bölümü bitirince **durma, rapor yazıp bekleme — sonraki bölüme geç.**
- Her bölümün sonunda: (1) o bölümün kapıları + sabotajları, (2) ayrı commit
  (`SURUCU(codex): Dalga 15 bölüm N — ...`), (3) `codex15_ilerleme.md`'ye kısa not (yapılan, ölçümler,
  sabotaj sonuçları, süre). Dalga yarıda kesilse bile biten bölümler kullanılabilir olmalı.
- Gerçekten takılırsan (60 dk ilerleme yok): `codex15_bulgular.md`'ye yaz, bölümün yapılabilen kısmını
  kapılı bırak ve **sonraki bölüme geç**.
- Draft PR'ı yalnız on bir bölüm bittikten (ya da her biri için takılma notu yazıldıktan) sonra aç.

---

## 0. Hedef

v1 bitiş tanımı (yol haritası H0f) QEMU'da tamamlandı (D-763): USB'den açılan sistem, Türkçe konsol ve
klavye, fare, FAT32 okuma/yazma, kabuk, ayrı derlenmiş kullanıcı programları, metin düzenleyici ve dosya
gezgini. Sıradaki adım **gerçek bir PC'de ilk denemenin başarılı ve teşhis edilebilir olması**: her donanım
beklemesi zaman aşımlı olmalı, başarısız bir deneme ekranda ve diskte okunabilir bir iz bırakmalı, açılış
yapılandırma matrisi QEMU'da taranmış olmalı, bozuk girdiler çökertmemeli. Yanında sistemi günlük
kullanıma yaklaştıran ayrıntılar: fare tekerleği, tuş tekrarı, boru hatları ve yönlendirme, birkaç yeni
program, belgeler.

Yol haritası: H10g, H10h, H4i, H12a, H12b, H12c, H12d, H12e, H6c, H6d, H5f ve yol haritasının genel
denetimi.

---

## 1. Bugünkü durum (Claude ölçtü — varsayma, yine de kendin doğrula)

- Dalga 14 (D-763) çıktıları: zaman paylaşımlı çok süreç (`baslat`, `bekle`, `ps`, `sonlandir`; ABI 58-60),
  parçalı ELF yükleme ve geniş kullanıcı penceresi (`0x54200000` girişli düzen), ekran/fare/olay çağrıları,
  `duzenle`, `dosyalar`, kabuk geçmişi/Tab/program araması, `make kem_os_usb.img`, tek açılışta v1 senaryosu,
  `belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md`. Ölçümler `otomasyon/chatgpt/codex14_bulgular.md`'de.
- Kullanıcı programları: `test/kullanici/{merhaba,kedi,say,yazdene,tus,bolme,hesap,buyukelf,dosyalar,duzenle,
  ekranden,kaynakden,surecden}.kem`. ABI belgesi `belgeler/KEMGU_OS_Syscall_ABI.md`.
- x86 QEMU kapıları: `calistir_bm_x86_{nvme,ahci,usb,ekran,kabuk,panik,istisna,ps2,uefi,uefi_kabuk,uefi_pc,
  apic,ps2_irq,ps2_karma,guc,blok,disk,surec_abi,surec,kaynak,v1,kabuk14,dosyalar,duzenle,ekran_fare,
  buyuk_elf,cok_surec}`.
- **Dalga 14'teki aralıklı USB klavye / "ELF kısa okuma" hatalarının kökü bulundu ve D-763'te onarıldı:**
  self-host derleyici döngüdeki geçici yapı yuvalarının yığınını erken geri alıyordu; LLVM büyük yapı
  kopyasını geri almadan sonraya erteleyince araya giren bir kesme veriyi eziyordu (DMA alanında çekirdek
  yığını değeri — senin gözlemin). Yığın artık döngü gövdesinin sonunda geri alınıyor. Bu dalgada aralıklı
  bir kırmızı görürsen bunu tekrar varsayma; ölç ve yaz.
- **Heap (E4k — açık, Claude'da):** çekirdek heap'i 28 MB ve dosya/kabuk işlemleri geri verilmeyen tahsis
  yapıyor (dalga 14: düzenleyicide tek oturumun 9. kaydı 24 MiB eşiğini aşıyor). Uzun senaryoları açılış
  başına sınırlı tut (taze açılışlara böl), ölç ve yaz; derleyiciyi düzeltmeye çalışma.
- Yol haritasında birçok madde aslında yapılmış ama işaretlenmemiş görünüyor (ör. H5/H7/H8 sürücü ve dosya
  sistemi maddeleri, H9e-i, H10c-f, H11a-b). Bölüm 1 bunu temizler.

---

## 2. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-764     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex15 -b codex/saglamlik-dalga15 origin/main && cd ../kemgu-codex15
make -j8 build/kemgu build/codegen build/kdl_runtime.o
make calistir_bm_x86_v1 calistir_bm_x86_uefi_pc calistir_bm_x86_cok_surec calistir_kem_os_x86   # taban
date > otomasyon/chatgpt/codex15_ilerleme.md
```
Tek dal, tek draft PR. ⚠ Claude bu dalga sırasında derleyicide (E4k) çalışabilir; **dalını dalga ortasında
main ile birleştirme** — Claude entegrasyonda birleştirir.

---

## 3. Okunacaklar
- `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` (tamamı), `LOOP.md` D-747..D-763,
  `otomasyon/chatgpt/codex12_bulgular.md`, `codex13_bulgular.md`, `codex14_bulgular.md`.
- `belgeler/KEMGU_OS_Syscall_ABI.md`, `belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md`.
- `test/ornekler/kem_os_x86_kabuk.kem`, `kem_os_x86_vfs_ortak.kem`, `kem_os_x86_blok_ortak.kem`,
  `runtime/mimari/x86_64/*.kem`, `drivers/**`, `test/kullanici/*.kem`, `runtime/kullanici/x86_64/*.kem`.
- `tests/qemu/*.py` (özellikle v1, dalga12, surec, psf kâhinleri), `test/bm_x86_*_harness.sh`.

---

## 4. İzin genişletmesi (yalnız bu dosyalarda)
- `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md` (Bölüm 1: işaretler ve kanıt notları), yeni belgeler
  `belgeler/KEMGU_OS_*.md` (Bölüm 11).
- `boot/start_x86_64.S`: YALNIZ yeni bir kesme vektörü gerekirse koçan eklemek; mevcut yollar birebir korunur.
- `runtime/kdl_kesme.c`: YALNIZ `#if defined(__x86_64__)` IDT bloğu (ikili modda düzenle).
- `runtime/kem_gorev_cekirdek.kem` (sistem çağrısı tablosu; arm64 davranışı değişmez).
- `runtime/mimari/x86_64/*.kem`, `runtime/kullanici/x86_64/*.kem`, `linker/user-x86_64.ld`.
- `drivers/**` saf (sans-IO); yeni modüller serbest.
- `test/ornekler/kem_os_x86_*.kem`, `test/kullanici/*.kem`, `test/bm_x86_*_harness.sh`, `tests/qemu/*`,
  `tests/drivers/**`, `tools/codex/*`.
- Makefile (yeni hedefler + `test_tumu` + `.PHONY` + x86 CAT listeleri), `test/check_kapisi.sh`,
  `test/baremetal_diff_harness.sh` sınıflandırma satırları; `belgeler/KEMGU_OS_Syscall_ABI.md`.
- YASAK: `src/`, `selfhost/`, `boot/efi/`, diğer `linker/` dosyaları, diğer runtime C dosyaları. Derleyici
  hatası → `codex15_bulgular.md`'ye C15-xxx + küçük tekrar üretimi (C ve self-host komutu, beklenen/gerçek
  sonuç), dolan. Muafiyet listelerine satır EKLEME.

---

## 5. Tasarım ilkeleri (önceki dalgalarınkiler geçerli, ek olarak)
1. **Her bekleme sınırlı:** donanım bekleyen her döngünün bir zaman aşımı ve okunabilir bir hata mesajı
   vardır; hiçbir aygıt hatası sistemi sessizce asmaz.
2. **Başarısız deneme iz bırakır:** gerçek PC'de bir şey çalışmazsa kullanıcı ekranda ve USB bellekte neyin
   bulunduğunu, neyin bulunamadığını okuyabilmeli.
3. ABI geriye uyumlu büyür (5-60 değişmez; yeni çağrılar 61-79); belge ↔ dağıtıcı ↔ sarmalayıcı tutarlılık
   kapısı her yeni çağrıyı kapsar.
4. Kabuk ve programlar aynı sürücü/dosya katmanını kullanır (D-407).
5. Sessiz kısaltma yok; her sınır belgelenmiş hata kodu döner.

---

## 6. İş listesi — on bir bölüm, HEPSİ zorunlu, sırayla

### Bölüm 1 — yol haritası denetimi
- `belgeler/KEMGU_OS_Donanim_Yol_Haritasi.md`'deki **her işaretlenmemiş madde** için: yapılmış mı, kısmen
  mi, yapılmamış mı? Kanıt = bir kapı adı ve/veya LOOP D-numarası ve/veya dosya yolu. Yapılmış olanları
  `[x]` yap ve kanıtı maddenin başına kısa not olarak yaz (mevcut `✓ D-7xx (...)` deseni); kısmen olanlara
  neyin eksik olduğunu yaz. Ertelenmiş (⏸) maddelere dokunma.
- Bir tablo: `codex15_ilerleme.md`'de madde → durum → kanıt. Kanıtsız işaretleme YOK; emin değilsen ölç
  (ilgili kapıyı koştur).
- Sabotaj: yok (belge işi) — ama işaretlediğin her madde için kanıt kapısının gerçekten yeşil olduğunu
  bu bölümde bir kez koşturarak doğrula.

### Bölüm 2 — sağlamlık turu (H10g)
- Çekirdekteki ve sürücülerdeki **bütün bekleme döngülerinin envanteri** (yoklama döngüleri, `iken` ile
  durum biti bekleyen her yer): dosya, satır, neyi beklediği, sınırı var mı. Sınırsız olanlara zaman aşımı +
  açık hata mesajı ekle (`KABUK HATA <aygıt> <ne> zaman asimi` biçiminde tutarlı).
- QEMU'da beklenmedik durumlar: xHCI yokken, AHCI portunda disk yokken, NVMe hiç yanıt vermeyen
  yapılandırmada (QEMU'nun sunduğu en yakın şekil — seçimini yaz), USB bellek çıkarılmış gibi (QMP
  `device_del`) → kabuk açık bir hatayla devam eder, asılmaz.
- Kâhin: her senaryoda seri çıktıda beklenen hata satırı + kabuk istemi + `-no-reboot` ile tek açılış.
- Sabotaj: bir zaman aşımını kaldır → ilgili senaryo zaman aşımıyla KIRMIZI.

### Bölüm 3 — teşhis paketi (H10h)
- Kabuk komutu `donanim` ve açılışta (yazılabilir bir bağlama varsa) `donanim.txt`: CPU (CPUID satıcı/ad),
  bellek haritası özeti, GOP kipi, ACPI tablo listesi (imza + boy), MADT özeti, PCI aygıt listesi (bus:dev.fn,
  satıcı:aygıt, sınıf), USB aygıtları (yuva, sınıf, VID:PID), bağlanan diskler ve bölümler, klavye/fare kaynağı.
- Fiziksel kontrol listesine "sonucu bana getir: `donanim.txt`" adımı ekle.
- Kâhin: QEMU'da dosya ana makinede bağımsız Python ile okunur; QMP `info pci`, `info usb` ve bilinen ACPI
  tablo listesiyle karşılaştırılır.
- Sabotaj: PCI satırında sınıf kodunu bir bayt kaydır → kâhin KIRMIZI.

### Bölüm 4 — çekirdek günlük halkası (H4i)
- Sabit boyutlu (ör. 64 KiB) çekirdek günlük halkası; çekirdek seri mesajları (açılış satırları, hata
  satırları, uyarılar) halkaya da yazılır. Kabukta `log` (ekranda sayfalı göster) ve `log kaydet <yol>`.
- Halka dolunca en eski satırlar düşer ama düşen satır sayısı sayılır ve gösterilir (sessiz kayıp yok).
- Kâhin: QEMU'da `log kaydet` → host'ta dosya, seri çıktının ilgili satırlarıyla birebir.
- Sabotaj: halka sarımında baş indeksini yanlış ilerlet → kâhin KIRMIZI.

### Bölüm 5 — QEMU açılış matrisi (H12a)
- `calistir_kem_os_pc_matris`: açılış aygıtı {USB bellek, NVMe, AHCI} × klavye {PS/2, USB} × RAM {256M, 1G,
  4G} × bölüm tablosu {GPT, MBR} uzayından en az 12 yapılandırma (kombinasyon seçimini ve gerekçesini yaz).
  Her biri: açılış, kabuk, kısa v1 alt senaryosu (dosya yaz/oku, bir program çalıştır), kapanış; host'ta
  fsck + içerik + salt okunur disklerin SHA256'sı.
- Her yapılandırma C ve self-host ile ≥3 açılış; aralıklı kırmızıyı sayıyla yaz.
- Sabotaj: MBR yolunda bölüm başlangıç LBA'sını bir sektör kaydır → MBR yapılandırmaları KIRMIZI.

### Bölüm 6 — bulanıklaştırma (H12b)
- Host'ta (sans-IO ayrıştırıcılar, C ve self-host ikisiyle): bozuk ACPI tabloları (RSDP/XSDT/MADT/FADT;
  uzunluk, sağlama, girdi sınırları), bozuk ELF dosyaları (başlık, program başlıkları, örtüşme, taşma),
  bozuk BMP dosyaları, bozuk GPT/MBR; her biri için sabit tohumlu ≥1000 örnek. Hiçbiri çökmez/asılmaz;
  her biri açık hata ya da geçerli sonuç. Mevcut 1000 FAT örneği (D-759) bu takıma katılır.
- QEMU'da: diskte bozuk bir ELF → `calistir` açık hata, kabuk devam.
- Sabotaj: ELF program başlığı sınır denetimini kaldır → bulanıklaştırıcı çökmeyi yakalar → KIRMIZI.

### Bölüm 7 — giriş ayrıntıları (H6c, H6d, H5f)
- Fare tekerleği: PS/2 IntelliMouse (200-100-80 örnekleme dizisi → kimlik 3, 4 baytlık paket) ve USB HID
  tekerleği; `olay_bekle` fare olayına tekerlek alanı (ABI belgesine yaz). `dosyalar` ve `duzenle`
  tekerlekle kaydırır.
- Fare hızlanma eğrisi + hassasiyet (kabukta ayarlanabilir) + ekran sınırına kenetleme (mevcut).
- Tuş tekrarı (yazılım): basılı tutulan tuş belirli gecikmeden sonra belirli hızda tekrar eder; PS/2 ve USB
  için aynı davranış (değerleri belgeye yaz).
- Kâhin: QMP `input-send-event` (fare `rel` + tekerlek düğmeleri; tuş bas ve gecikmeli bırak).
- Sabotajlar: tekerlek işaretini ters çevir → kaydırma yönü KIRMIZI; tekrar gecikmesini sıfırla → tekrar
  sayısı KIRMIZI.

### Bölüm 8 — yeni kullanıcı programları
- `goster`: BMP görüntüleyici (`drivers/ekran/bmp.kem`), ekrana sığdır, ok tuşlarıyla kaydır, `q` çıkış.
- `bul`: dosyalarda metin arama (grep benzeri; Türkçe büyük/küçük harf duyarsız seçenek, satır numaraları,
  birden çok dosya).
- `hesapla`: tam sayı ve kesirsiz aritmetik ifade hesaplayıcı (öncelik, parantez, hata mesajları).
- `saat`: tam ekran canlı saat (`olay_bekle` zaman aşımıyla saniyede bir güncelle; tuşla çıkış).
- Her biri için host birim testleri (saf mantık modülü varsa) + QEMU kâhini (seri + ekran).
- Sabotaj: her programdan birer işlevsel hata (ör. `bul`'da satır numarasını bir kaydır) → KIRMIZI.

### Bölüm 9 — boru hatları ve yönlendirme
- Kabukta `a | b` (en az iki aşama; üç aşama önerilir), `komut > dosya`, `komut >> dosya`, `komut < dosya`.
- Çekirdekte sınırlı tamponlu boru nesnesi (fd'ye bağlanır; yazan dolunca bekler, okuyan boşken bekler —
  Bölüm 1'deki çok süreç uykusunu kullan); aşamalardan biri bitince diğerine dosya sonu.
- Yeni çağrılar belgeye (61+): ör. `boru(fd_ptr)`, `fd_kopyala(eski, yeni)` ya da `baslat`'a fd eşlemesi
  parametresi — tasarımı seç ve gerekçesini yaz.
- Kâhin: `kedi notlar.txt | bul ağaç | say` gibi zincirler; çıktı dosyaya yönlendirilip host'ta doğrulanır.
- Sabotaj: boru tamponunun dolu kontrolünü kaldır → veri kaybı KIRMIZI.

### Bölüm 10 — açılış süresi + sabotaj envanteri (H12d, H12c)
- Açılış süresi: firmware devrinden kabuk istemine kadar aşamalar (bellek haritası, ACPI, PCI, USB, diskler,
  kabuk) seri zaman damgalarıyla; QEMU'da medyan + eşik kapısı (eşiği ölçümle seç, gerilemeyi yakalasın).
- Sabotaj envanteri: dalga 9-15'in sabotajlarını tek tabloda topla (`belgeler/KEMGU_OS_Sabotaj_Envanteri.md`:
  kimlik, bölüm/madde, ne bozuldu, hangi kapı kırmızı oldu, kaç açılışta).
- Sabotaj: bir aşamaya yapay gecikme ekle → süre kapısı KIRMIZI.

### Bölüm 11 — belgeler (H12e)
- `belgeler/KEMGU_OS_Program_Yazma.md`: kullanıcı programı yazma kılavuzu (derleme komutu, linker betiği,
  çalışma zamanı, sistem çağrısı sarmalayıcıları, argv, dosya/ekran/olay örnekleri, sınırlar). Kılavuzdaki
  her örnek bir kapıda derlenip çalıştırılsın (belge eskimesin).
- `belgeler/KEMGU_OS_Mimari.md`: açılıştan kabuğa akış, bellek düzeni, sürücü katmanları (sans-IO + yürütücü),
  süreç modeli, dosya katmanı, kesmeler — mevcut kodla tutarlı, dosya yollarıyla.
- `belgeler/KEMGU_OS_Klavye_Duzeni.md`: Türkçe Q düzeni tablosu (tarama kodu → karakter, değiştiriciler).
- Kabukta `yardim <komut>` bu belgelerdeki komut açıklamalarıyla tutarlı.

---

## 7. Kapılar (her bölümden sonra)
O bölümün yeni kapıları + `calistir_bm_x86_v1`, `calistir_bm_x86_uefi_pc`, `calistir_bm_x86_cok_surec`,
`calistir_bm_x86_duzenle`, `calistir_bm_x86_dosyalar`, `calistir_bm_x86_kabuk14`, `calistir_bm_x86_surec`,
`calistir_kem_os_x86`; `kem_gorev_cekirdek.kem` değişirse `calistir_kem_os_arm` + `calistir_qemu_cekirdek`;
sans-IO değişikliklerinde `calistir_surucu_diff`, `calistir_surucu_kosum`, `calistir_chatgpt_kopru`.
On birinci bölümden sonra `make test_tumu` (rc PIPESTATUS ile, atlamalar sayılmış), `calistir_check_kapisi`,
`calistir_baremetal_diff`.

**Ölçüm tuzakları:** derle-çalıştır döngüsünde ikiliyi ve kullanıcı ELF'lerini her koşumdan önce `rm -f`;
disk imajı her koşumda yeniden üretilir; `echo "$(cmd) $?"` yazma; `--checkdump` tanı bassa da 0 döner;
QEMU üçlü hatada sessizce yeniden başlar (`-no-reboot` + tek açılış işareti); kâhin anında gönderirse bekleme
kusurları görünmez; Türkçe kaynak okuyan her Python betiği `encoding="utf-8"` kullanır (D-761: Windows CI).

---

## 8. Bitirince
Draft PR `codex/saglamlik-dalga15`, gövde `codex15_ilerleme.md` (on bir bölümün her biri için: yapılanlar,
kapılar, sabotaj sonuçları, süre). `codex15_bulgular.md`: ölçülen davranışlar ve VARSAYIM'lar, bekleme
envanteri, matris sonuçları, bulanıklaştırma istatistikleri, açılış süresi dağılımı, derleyici bulguları
(C15-xxx), kalanlar. Auto-merge AÇMA, merge ETME.

## 9. Bu dalganın DIŞINDA
- E4k (bölge modeli), E4q (derleme süresi), hata yalıtımı testleri (H9j/E3f): Claude.
- Pencere yöneticisi, ağ, ses, SMP, KEMFS (H8i), BIOS/GRUB (D-756), Secure Boot.
