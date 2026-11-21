2026-11-21T17:09:04+03:00

## Kapsam

x86_64 üzerinde sekiz süreçli çalıştırma, parçalı ELF yükleme, ekran ve girdi ABI’si, Türkçe metin düzenleyici, tıklanabilir dosya gezgini ve bunları içeren UEFI USB imajı. C ve self-host ile tek QEMU açılışında v1 senaryosu tamamlandı. Aralıklı açılış hatalarının adetleri aşağıda yer alıyor. Fiziksel deneme için H11b yönergesi ve sonuç formu teslim edildi.

## Taban

Dal D-762 üzerinden açıldı (9aca00f). Değişiklik öncesi süreç, ABI, UEFI PC ve x86 çekirdek hedefleri çalıştırıldı. Süreç kapısı C ve self-host ile toplam 20 OVMF açılışından geçti.

## Bölüm 1 — zaman paylaşımlı süreçler

Kullanıcı kesme çerçevesi 160 bayt. Her süreç kendi CR3'ü ile çalışıyor. Sekiz kayıt aynı anda canlı kalabiliyor. Klavye, çocuk ve LAPIC zaman aşımı beklemelerinde sistem çağrısı işlemciyi bırakıyor. Hazır süreç yokken çekirdek kesmeye kadar uyuyor. Çocuklar çalışma dizinini devralıyor ve kendilerine ait fd tablosunu kullanıyor.

İlk QEMU ölçümünde 100 çocuk başlatılıp toplandı. İlk kâhin her iki uçta 49.761 bildirdi. Bu mutlak sayı, bit eşlemi sayımındaki hata nedeniyle bölüm 2’de düzeltildi. C ve self-host ile arka planda hesap sürerken kabuk girdiyi işledi, ön planda merhaba çalıştı. QMP ile iki giriş yolunun sekiz saniyelik boş beklemesi denendi. İlk hesap testinde doğru toplam 7.200.000.060.000.000 ve 19 kullanıcı zaman dilimi ölçüldü.

Kusur enjeksiyonları ilk koşumda beklenen sonucu verdi. Kaydedilen RAX bozulunca hesap çıkış 1 verdi. Çerçeve iadesi kaldırılınca fiziksel bit eşlemi farklı kaldı. Uyandırma kapatılınca klavye testinde saat aşaması zaman aşımına uğradı.

Çok süreç kapısı C 30/30, self-host 30/30 açılışta geçti (hesap, arka plan ve 100 çocuk için ayrı 10'ar açılış). Adres alanını değiştirmeyen yalıtılmış denemede işlemci kullanıcı girişini reddetti ve çocuk kâhini kırmızı oldu. Dalga 13 matrisinde C 10/10 geçti. Self-host ilk seri 7 başarılı açılıştan sonra USB klavye başlatırken sona erdi, temiz yeniden derlemede 10/10 geçti. 50 tekrarda başlangıç ve son bit eşlemi aynıydı, süreç heap farkı 0 ve kabuk heap'i 4.919.792 → 16.069.072 bayt oldu. ABI, UEFI PC/kabuk, x86 kabuk/blok/disk/istisna/APIC/PS2 IRQ, x86 çekirdek, sürücü diff/koşum ve köprü kapıları geçti. Başlangıçtan bölüm kapılarına yaklaşık 60 dakika.

## Bölüm 2 — büyük programlar

336.744 baytlık ELF, 320 KiB salt okunur tablo, 1.280 KiB BSS ve yazılıp tekrar okunan 4 MiB kullanıcı heap'i C ve self-host ile 10'ar açılışta doğrulandı. Her açılışta Dalga 13 düzeniyle bağlanan ikinci bir ELF de çalıştı. Büyük yüklemede kabuk heap artışı yaklaşık 8,2 MB ölçüldü. Yükleyici diski en çok 16 KiB'lık parçalarla işler. Parça hedefi bir bayt kaydırıldığında program 11 döndürdü, BSS eşlemesi 1 MiB'a indirildiğinde #PF oluştu. Her iki kâhin kırmızı oldu.

Süreç matrisi C 10/10, self-host temiz tekrarı 10/10. İlk self-host serisinde bir aralıklı ELF okuma hatası görüldü. UEFI PC/kabuk, disk/blok, istisna/APIC/PS2 IRQ, x86 kabuk/çekirdek, ABI ve üç saf sürücü kapısı geçti. Bit eşlemi kâhininin önceki mutlak sayıları düzeltildi, bulgular belgesinde açıklanmıştır. Uygulama ve QEMU tekrarları bölüm 1'in uzun koşumlarıyla örtüştü; son tekrarlar 18:33'te tamamlandı.

## Bölüm 3 — ekran ve fare

61–66 numaralı çağrılarla ön plan süreci hücre çizebiliyor, imleci yönetebiliyor, klavye, fare veya zaman aşımını bekleyebiliyor. PS/2 ve USB için iki derleyici × iki giriş yolu × on açılışın tamamı geçti. USB koşumu gerçek MSI-X sayacının arttığını da denetledi. Satır ve sütunun ters çevrilmesi, kenetlemenin kaldırılması ve kabuk ekranının geri çizilmemesi ayrı ayrı kırmızı verdi. Dalga 13 süreç matrisi 20/20 geçti. §7 regresyonunda blok hedefinin eksik ithali düzeltildi. Ayrı blok ve PS/2 IRQ tekrarları yeşil. Ekran kapılarının ilk tam serisi 18:08, MSI-X eklenmiş son seri 18:45.

## Bölüm 4 — metin düzenleyici

`duzenle` yeni veya var olan bir dosyayı açar. Türkçe karakter silme, bölme, birleştirme, yatay ve dikey kaydırma saf tampon testinde C ve self-host ile geçti. OVMF düzenleyici kapısı C 10/10, self-host 10/10. Kâhin dosyanın UTF-8 baytlarını, FAT durumunu, ekranı ve sekiz saniye boşta heap'i denetledi. İki ayrı içerik kusuru kaydedilen baytları bozdu. Eşitleme kaldırılınca FAT kirli kaldı. §7 süreç 20/20, kalan on regresyon hedefi ve üç saf sürücü kapısı geçti. İlk düzenleyici ölçümlerinden son regresyon serisine yaklaşık 55 dakika.

## Bölüm 5 — dosya gezgini

`dosyalar` bağlama noktalarını, dizinleri, dosya boylarını ve üst dizini gösteriyor. Ön planı çocuğa atomik olarak veren 67 numaralı çağrı, düzenleyicinin ebeveyni beklemeye girmeden önce çalışabilmesi yarışını giderdi. Gezgin kapısında C 10/10; self-host ilk seride 4 başarıdan sonra QMP bağlantısı kesildi, taze seride 10/10. Salt okunur NVMe senaryosu C 10/10; self-host ilk seride 2 başarıdan sonraki açılış ilk ELF okuma planında durdu, taze seride 10/10. Kâhin kaydın reddini, çift Ctrl+Q ile çıkışı, USB'deki Türkçe kaydı ve iç disk özetini doğruladı. Tıklama satırını kaydırmak ve üst dizin yolunu eski bırakmak ayrı ekran uyuşmazlıklarına yol açtı. §7 süreç 20/20 ve diğer regresyon hedefleri geçti. İlk iki derleyicili gezgin denemesinden son salt okunur seriye yaklaşık 39 dakika.

## Bölüm 6 — kabuk

Geçmiş etkin kabukta 32 satır saklıyor. Tab ortak öneki tamamlıyor, birden çok adayda adları gösteriyor. `temizle`, `yardim`, `sonuc` ve belgelenen `/bin` araması eklendi. Sol/sağ okla ortadan düzenleme seri çıktı ve GOP ile karşılaştırıldı. QMP her geçmiş adımında görülen satırı bekledi. C 10/10 ve self-host 10/10 geçti. Geçmiş ofsetinin ve ortak önekin bozulması ayrı piksel uyuşmazlıkları üretti. §7 süreç 20/20, diğer regresyonlar ile saf sürücü diff/koşum/köprü yeşil. İlk iki derleyicili kabuk denemesinden son matrise yaklaşık 31 dakika.

## Bölüm 7 — v1 oturumu ve USB imajı

Aynı OVMF açılışında USB, NVMe ve AHCI bağlıyken düzenleyici, gezgin, ikinci kayıt, sayım, iç diskten okuma, arka planda hesap, süreç listesi, sonlandırma ve kapanış çalıştı. İlk tam C serisi hesap programına uzun çalışma argümanı veriyordu. Varsayılan hesap süresi uzatılınca tam olarak `baslat hesap &` ile de C ve self-host onar kez geçti. Son kaydın hemen ardından güç kesilen senaryo ayrıca C 10/10 ve self-host 10/10 geçti. İkinci kaydın eşitlemesini kaldırmak FAT kirli-bit kâhinini kırmızıya çevirdi. Kapanışta beklenen UTF-8 baytları ile ESP ve iki iç diskin SHA256 özetleri denetlendi.

İmaj 135.283.200 bayt. Üretici ana ve yedek GPT'nin başlık ve tablo CRC'lerini, iki FAT bölümünü ve kurulan her ELF'in baytlarını kontrol ediyor. H11b belgesinde aygıt seçme, yazdıktan sonra okuma, fiziksel deneme adımları ve sonuç formu var. §7 süreç 20/20, diğer regresyon hedefleri yeşil. İlk doğrulanmış imajdan son yalın komut serisine yaklaşık 37 dakika.

## Bölüm 8 — kaynak ve süre

Süreç ölçüm kapısı C ve self-host ile onar açılışta yüzer çocuk başlatıp topladı. Her süreç 3.594 fiziksel çerçeve aldı ve hepsini geri verdi. Düzenleyici kapısı her derleyici için 10 açılış × 5 kayıt tamamladı. Tek açılışta 50 kayıt istendiğinde iki derleyici de dokuzuncu kayıttan sonra 24 MiB ölçüm eşiğini geçti. Çerçeve sayısı her kayıtta başlangıca döndü. Ham CSV üreten kapı, kaynak eğrileri ve derleme süreleri bulgular belgesinde. Süreç matrisi ilk self-host serisinde 2 başarıdan sonra ELF okuma hatası verdi, taze seri 10/10. Disk regresyonundaki ilk QMP kesintisi ve kopyalama sırasında eksik kalan yerel nesne dosyasının yol açtığı bağlama hatasından sonra disk/blok ve IRQ grupları yeniden çalıştırıldı ve geçti. Kalan §7 kapıları yeşil.

Nihai imaj 13 ELF içeriyor. Üretilmiş imajın birebir kopyası SHA256 `17d542655eb92371fa86baf5cfcedfdc5d95eb55169ce84d35f59f745d7e68ac` ile açılarak v1 senaryosu tamamlandı; özgün imajın özeti korundu. Tip kontrol kapısı 358/379, 21 mevcut muafiyet ve 0 kırmızı. Bare-metal paritesi 23/23, 0 atlama. Sürücü alt-makefile'ı geçti. `calistir_sifir_uyari` 38 dosyada sıfır C derleyici uyarısı bildirdi.

### Yeni QEMU kapılarının doğrulanmış tam serileri

| Kapı | C | Self-host |
|---|---:|---:|
| Çok süreç (hesap, arka plan, 100 çocuk) | 30/30 | 30/30 |
| Büyük ve eski ELF | 10/10 | 10/10 |
| Ekran, PS/2 ve USB fare | 20/20 | 20/20 |
| Düzenleyici | 10/10 | 10/10 |
| Gezgin, yazılabilir ve salt okunur disk | 20/20 | 20/20 |
| Kabuk | 10/10 | 10/10 |
| V1, normal kapanış ve son kayıtta güç kesme | 20/20 | 20/20 |
| Kaynak, 100 çocuk ve 5 düzenleyici çevrimi | 20/20 | 20/20 |
| **Toplam** | **140/140** | **140/140** |

Her satırın iki derleyici için tamamlanmış serisi gösterildi. Önceki başarısız denemeler ilgili bölümde ve bulgular belgesinde yer alıyor. Düzenleyici açılış medyanı C 218,72 ms, self-host 194,99 ms. Bölüm 8'in uzun QEMU tekrarları yaklaşık yarım saat sürdü.


## Test

Süreç, ABI, UEFI PC/kabuk, disk, blok, istisna, APIC, PS/2 IRQ, x86 kabuk ve x86 çekirdek regresyonları her bölümde çalıştırıldı. Saf sürücü değişiklikleri ayrıca `calistir_surucu_diff`, `calistir_surucu_kosum` ve `calistir_chatgpt_kopru` ile denetlendi. Sekiz yeni Makefile hedefi, yukarıdaki tabloda 14 x86_64/OVMF senaryosunu çalıştırıyor. Düzenleyicinin saf tamponu konakta iki derleyiciyle çalışıyor. ARM64 ve x86_64 birleşik birimleri bare-metal parite kapısından geçti.


Seri `make -k test_tumu 2>&1 | tee build/tumu.log` koşumu 2 saat 51 dakika 26 saniyede tamamlandı. `PIPESTATUS=(0 0)` ve son satır `Tum testler gecti!`. 106 önkoşul hedefi tamamlandı. Yeni 14 QEMU senaryosunun her biri C ve self-host ile onar kez geçti (280/280). Koşum D-762 tabanlı ayrı bir çalışma kopyasında yapıldı. Çalıştırılan değiştirilmiş kaynaklar, teslim edilen kaynaklarla bayt bayt karşılaştırıldı.

Ek paralel tam koşumda (`make -j6 -k test_tumu`) `PIPESTATUS=(2 0)` çıktı. Tek başarısız hedef v1 oldu. Self-host güç kesme serisinin dördüncü açılışında konuk ilk ELF öncesi USB klavye aktarım hatası verdi. Hedefin yeni ikililerle tekrarı 40/40 geçti. Klavye kaydının açılışta değiştiği ayrı QEMU gözlemi bulgular belgesinde. Aynı altı işli tam takımın ikinci çalıştırması 36 dakika 40 saniyede 106 hedefi tamamladı. Bu çalıştırmada yeni QEMU senaryoları 280/280 geçti, `PIPESTATUS=(0 0)` döndü. Paralel koşumlarda hedefler farklı UEFI geçici dizinleri kullandı ve ortak derleyiciyi üreten önkoşul önce tamamlandı.

Tam takımdaki mevcut atlamalar, ana kapı ve korpus girdisi bazında aşağıda. Aynı girdinin bir kapı detayında ve özetinde görünmesi tek sayıldı.

| Kapı | Atlanan | Ayrıntı |
|---|---:|---|
| ASan uçtan uca | 45 | Kapının SKIP sayacı. Ayrıca 4 bilinen sonuç, 2 sızıntı muafiyeti var. |
| Geniş codegen | 8 | Host üzerinde bağlanamayan bare-metal/MMIO örnekleri |
| Semantik codegen | 4 | 2 yabancı mimari ve bağımsız C referansı bağlanamayan 2 çapraz modül |
| Sürücü paritesi | 1 | Ek olarak 1 mevcut muafiyet |
| Yapısal IR paritesi | 2 | Ek olarak 24 mevcut muafiyet |
| Sürücü host-mock | 5 | C referansı tip kontrolünde reddedilen girdiler |
| ARM konuk üst bellek deneyi | 1 | Konuk RAM’i 1 GiB sınırının altında ya da eşit |
| **Ana kapılarda toplam** | **66** | **Dalga 14’ün yeni x86_64 kapılarında 0 atlama** |

Self-driver, semantik codegen korpusunu iki sürücüyle ayrıca tekrarlar. Geniş tip karşılaştırmasında 18 ve genel tip kapısında 21 mevcut muafiyet raporlandı. Dalga muafiyet listelerine satır eklemedi.

## Dil özellikleri

Düzenleyici tamponu ve gezgin görünümü mevcut dizi, yapı, `sonuç` ve `eşleş` özelliklerini kullanır. Yeni xHCI MSI-X işleyicisi `gerçekzamanlı çıplak` olarak tanımlandı. Kesme içinde üç MMIO onayı ve bildirim sayacı var. Bu dalgada eklenen `tekkez` ve `yetki` sayısı sıfır.

## Dosya değişiklikleri (whitelist kontrol)

`git diff --name-only origin/main..HEAD` çıktısındaki 39 yol §4 ve iki teslim raporu ile eşleşti.

```text
Makefile
belgeler/KEMGU_OS_Syscall_ABI.md
belgeler/X86_Fiziksel_Donanim_Kontrol_Listesi.md
boot/start_x86_64.S
drivers/bellek/cerceve.kem
drivers/duzenle/tampon.kem
drivers/elf/elf64.kem
drivers/girdi/satir.kem
drivers/usb/hid_klavye.kem
linker/user-x86_64.ld
otomasyon/chatgpt/codex14_bulgular.md
otomasyon/chatgpt/codex14_ilerleme.md
runtime/kdl_kesme.c
runtime/kullanici/x86_64/kem_basla.kem
runtime/kullanici/x86_64/kem_sis.kem
runtime/mimari/x86_64/kem_bellek_harita.kem
runtime/mimari/x86_64/kem_surec_hw.kem
test/bm_x86_surec_harness.sh
test/kullanici/buyukelf.kem
test/kullanici/dosyalar.kem
test/kullanici/duzenle.kem
test/kullanici/ekranden.kem
test/kullanici/hesap.kem
test/kullanici/kaynakden.kem
test/kullanici/surecden.kem
test/ornekler/kem_os_x86_blok_ortak.kem
test/ornekler/kem_os_x86_kabuk.kem
test/ornekler/kem_os_x86_vfs_ortak.kem
tests/qemu/dalga12_kahin.py
tests/qemu/duzenle_kahin.py
tests/qemu/kabuk14_kahin.py
tests/qemu/kaynak14_kahin.py
tests/qemu/surec_elf.py
tests/qemu/surec_imaj.py
tests/qemu/surec_kahin.py
tests/qemu/usb14_imaj.py
tests/qemu/v1_kahin.py
tools/codex/dalga14_sabotaj.py
tools/codex/duzenle_tampon_test.kem
```

## Sonraki adım

H11b yönergesiyle gerçek bir x86_64 UEFI makinesinde GOP, giriş aygıtları ve disk sonuçları kaydedilebilir. E4k çalışması için dokuz kesintisiz kaydın heap eğrisi ve yüz çocuğun çerçeve ölçümleri bulgular belgesinde yer alıyor.
