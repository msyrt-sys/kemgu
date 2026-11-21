2026-11-21T17:09:04+03:00

## Taban

Dal D-762 üzerinden açıldı (9aca00f). Değişiklik öncesi süreç, ABI, UEFI PC ve x86 çekirdek hedefleri çalıştırıldı. Süreç kapısı C ve self-host ile toplam 20 OVMF açılışından geçti.

## Bölüm 1 — zaman paylaşımlı süreçler

Kullanıcı kesme çerçevesi 160 bayt. Her süreç kendi CR3'ü ile çalışıyor. Sekiz kayıt aynı anda canlı kalabiliyor. Klavye, çocuk ve LAPIC zaman aşımı beklemelerinde sistem çağrısı işlemciyi bırakıyor. Hazır süreç yokken çekirdek kesmeye kadar uyuyor. Çocuklar çalışma dizinini devralıyor ve kendilerine ait fd tablosunu kullanıyor.

İlk QEMU ölçümünde 100 çocuk başlatılıp toplandı. Gerçek UEFI ayırıcısında başlangıçta ve sonda 49.761 boş çerçeve vardı. C ve self-host ile arka planda hesap sürerken kabuk girdiyi işledi, ön planda merhaba çalıştı. QMP ile iki giriş yolunun sekiz saniyelik boş beklemesi denendi. İlk hesap testinde doğru toplam 7.200.000.060.000.000 ve 19 kullanıcı zaman dilimi ölçüldü.

Kusur enjeksiyonları ilk koşumda beklenen sonucu verdi. Kaydedilen RAX bozulunca hesap çıkış 1 verdi. Çerçeve iadesi kaldırılınca fiziksel bit eşlemi farklı kaldı. Uyandırma kapatılınca klavye testinde saat aşaması zaman aşımına uğradı.

Çok süreç kapısı C 30/30, self-host 30/30 açılışta geçti (hesap, arka plan ve 100 çocuk için ayrı 10'ar açılış). Adres alanını değiştirmeyen yalıtılmış denemede işlemci kullanıcı girişini reddetti ve çocuk kâhini kırmızı oldu. Dalga 13 matrisinde C 10/10 geçti. Self-host ilk seri 7 başarılı açılıştan sonra USB klavye başlatırken sona erdi, temiz yeniden derlemede 10/10 geçti. 50 tekrarda 49.721 boş çerçeve korundu, süreç heap farkı 0 ve kabuk heap'i 4.919.792 → 16.069.072 bayt oldu. ABI, UEFI PC/kabuk, x86 kabuk/blok/disk/istisna/APIC/PS2 IRQ, x86 çekirdek, sürücü diff/koşum ve köprü kapıları geçti. Başlangıçtan bölüm kapılarına yaklaşık 60 dakika.

## Bölüm 2 — büyük programlar

336.744 baytlık ELF, 320 KiB salt okunur tablo, 1.280 KiB BSS ve yazılıp tekrar okunan 4 MiB kullanıcı heap'i C ve self-host ile 10'ar açılışta doğrulandı. Her açılışta Dalga 13 düzeniyle bağlanan ikinci bir ELF de çalıştı. Büyük yüklemede kabuk heap artışı yaklaşık 8,2 MB ölçüldü. Yükleyici diski en çok 16 KiB'lık parçalarla işler. Parça hedefi bir bayt kaydırıldığında program 11 döndürdü, BSS eşlemesi 1 MiB'a indirildiğinde #PF oluştu. Her iki kâhin kırmızı oldu.

Süreç matrisi C 10/10, self-host temiz tekrarı 10/10. İlk self-host serisinde bir aralıklı ELF okuma hatası görüldü. UEFI PC/kabuk, disk/blok, istisna/APIC/PS2 IRQ, x86 kabuk/çekirdek, ABI ve üç saf sürücü kapısı geçti. Bit eşlemi kâhininin önceki mutlak sayıları düzeltildi, bulgular belgesinde açıklanmıştır. Uygulama ve QEMU tekrarları bölüm 1'in uzun koşumlarıyla örtüştü; son tekrarlar 18:33'te tamamlandı.

## Bölüm 3 — ekran ve fare

61–66 numaralı çağrılarla ön plan süreci hücre çizebiliyor, imleci yönetebiliyor, klavye, fare veya zaman aşımını bekleyebiliyor. PS/2 ve USB için iki derleyici × iki giriş yolu × on açılışın tamamı geçti. USB koşumu gerçek MSI-X sayacının arttığını da denetledi. Satır ve sütunun ters çevrilmesi, kenetlemenin kaldırılması ve kabuk ekranının geri çizilmemesi ayrı ayrı kırmızı verdi. Dalga 13 süreç matrisi 20/20 geçti. §7 regresyonunda blok hedefinin eksik ithali düzeltildi. Ayrı blok ve PS/2 IRQ tekrarları yeşil. Ekran kapılarının ilk tam serisi 18:08, MSI-X eklenmiş son seri 18:45.

## Bölüm 4 — metin düzenleyici

`duzenle` yeni veya var olan bir dosyayı açar. Türkçe karakter silme, bölme, birleştirme, yatay ve dikey kaydırma saf tampon testinde C ve self-host ile geçti. OVMF düzenleyici kapısı C 10/10, self-host 10/10. Kâhin dosyanın UTF-8 baytlarını, FAT durumunu, ekranı ve sekiz saniye boşta heap'i denetledi. İki ayrı içerik kusuru kaydedilen baytları bozdu. Eşitleme kaldırılınca FAT kirli kaldı. §7 süreç 20/20, kalan dokuz regresyon hedefi ve üç saf sürücü kapısı geçti. İlk düzenleyici ölçümlerinden son regresyon serisine yaklaşık 55 dakika.

## Bölüm 5 — dosya gezgini

`dosyalar` bağlama noktalarını, dizinleri, dosya boylarını ve üst dizini gösteriyor. Ön planı çocuğa atomik olarak veren 67 numaralı çağrı, düzenleyicinin ebeveyni beklemeye girmeden önce çalışabilmesi yarışını giderdi. Gezgin kapısında C 10/10; self-host ilk seride 4 başarıdan sonra QMP bağlantısı kesildi, taze seride 10/10. Salt okunur NVMe senaryosu C 10/10; self-host ilk seride 2 başarıdan sonraki açılış ilk ELF okuma planında durdu, taze seride 10/10. Kâhin kaydın reddini, çift Ctrl+Q ile çıkışı, USB'deki Türkçe kaydı ve iç disk özetini doğruladı. Tıklama satırını kaydırmak ve üst dizin yolunu eski bırakmak ayrı ekran uyuşmazlıklarına yol açtı. §7 süreç 20/20 ve diğer regresyon hedefleri geçti. İlk iki derleyicili gezgin denemesinden son salt okunur seriye yaklaşık 39 dakika.
