# Dalga 16 ilerlemesi

23 Kasım 2026 saat 22.06–23.36 TRT arasında D-766 beş dakikada bir denetlendi. Başlangıç sürümü `4f59349f`. C derleyicisi, self-host derleyici ve C çalışma zamanı nesnesi derlendi.

D-766 tabanında `make calistir_bm_x86_v1 calistir_bm_x86_uefi_pc calistir_bm_x86_saglamlik calistir_kem_os_x86` 24 Kasım saat 00.03’te 0 döndürdü. v1 ve v1-kesinti toplam 40 açılış yaptı.

## Bölüm 1 — klavye

8042 öz-testi, port testleri, kilitler, PS/2 ve HID LED komutları, Pause/PrintScreen, Num Lock ile sayısal tuş takımı, TR/US düzeni, Ctrl+Boşluk ve ön plan sürecine Ctrl+C eklendi. Kesme halkası tanısı `girdisay` komutuyla çalışır. Kısayol olarak Ctrl+Boşluk seçildi. Her iki kaynağın 500/50 ms zamanlamasını korumak için yazılım tekrarı kullanıldı.

`calistir_bm_x86_klavye16` iki derleyicide de PS/2, USB ve 8042’siz USB açılışlarını geçti. QMP ile gönderilen 38 AltGr tuşunun UTF-8 çıktısı kabuk satırında karşılaştırıldı. Her iki derleyicide 1.600 basışın 1.022’si okundu, 578’i kayıp sayıldı. Dört sabotaj son kaynakların ayrı kopyalarında yeniden uygulandı. Dördü de kırmızı; ölçümler bulgular dosyasında.

Yeni klavye kapısı ile §7’deki sekiz regresyon ve dört saf sürücü kapısı 0 döndürdü. Tek make çağrısı 1247 saniye sürdü. Taban derlemesinden itibaren bölüm süresi 48 dakika.

## Bölüm 2 — PCI, ECAM ve köprüler

Genel 16/32 bit port erişimi, MCFG pencereleri, segment 0 ECAM ve CF8/CFC yedeği, köprü arkası tarama ve BAR boyutlandırma eklendi. Düz Q35, iki root-port, iç içe köprü ve i440FX için C ve self-host ile sekiz açılış geçti. Konuk envanteri, BAR’lar, iki seviyeli köprü numaraları, ECAM ve gerçek sayfa öznitelikleri QMP ile karşılaştırıldı. Sıfırlanmış alt veri yolu numarası QEMU üzerinde yeniden kuruldu. Her topolojide SATA ve NVMe dosyaları host imajıyla eşleşti.

Üç sabotaj da kırmızı döndü. İşlev kaydırmasında iki PCI işlevi eksildi, alt veri yolu sabotajında arkadaki üç işlev kayboldu, üst BAR yarısı atılınca gerçek MMIO erişimi sayfa hatası verdi. Ayrıntılar bulgular dosyasında.

PCI kapısı, PC matrisi, dört saf sürücü kapısı ve sekiz regresyonun son koşumları yeşil. Birleşik koşumda yalnız girdi15 rc 1 verdi (make rc 2). Tuşu bırakma QEMU sanal saatine bağlandıktan sonra girdi15 iki derleyicide aynı kabul aralığıyla bağımsız olarak rc 0 verdi. Birleşik koşum süresi 1283 saniye, bölüm süresi 44 dakika.

## Bölüm 3 — kesme kaydı ve disk bildirimleri

IRQ1, IRQ12, LAPIC zamanlayıcısı ve xHCI MSI-X kayıt arayüzüne bağlandı. 66–79 aralığında dinamik vektör ayırma ve bırakma eklendi. AHCI komutları MSI, NVMe CQ1 ise MSI-X bildirimi bekliyor. Geciken bildirimin ardından sürücü sınırlı beklemeden yoklamaya dönüyor. Donanım raporunda vektör, işleyici ve kesme sayısı var.

Yeni MSI kapısı iki derleyiciyle normal ve AHCI MSI yeteneği atlanan dört açılışta geçti. İki gerçek sabotaj kırmızı verdi. Yanlış MSI vektöründe zaman aşımından sonra SATA dosyası yoklama ile yine okundu. İkinci işleyicinin aynı vektöre bağlanmasına izin verilince çakışma testi hata verdi. PC matrisinin 72 açılışı ve §7 regresyonları yeşil. Birleşik koşum 1281 saniye, bölüm 21 dakika.

## Bölüm 4 — disk kurtarma

AHCI hatasında port durduruluyor, hata durumu temizleniyor, BSY/DRQ sürüyorsa COMRESET uygulanıyor ve motor yeniden başlatılıyor. NVMe başarısız tamamlanmayı kuyruktan tüketiyor. USB BOT, yeniden denemeden önce REQUEST SENSE gönderiyor. İlk komuta ek olarak en fazla üç deneme yapılır. Kalıcı sektör hatasının ardından her diskte bağımsız dosya aynı açılışta okundu.

C ve self-host için altışar blkdebug senaryosu geçti. Üç disk türünde tek seferlik hatada bir, kalıcı hatada dört başarısız fiziksel okuma ölçüldü. Yeniden denemeyi sıfırlama ve AHCI portunu yeniden başlatmama sabotajlarının ikisi de kırmızı. Disk kâhini, dört saf sürücü kapısı ve sekiz regresyon yeşil. İlk toplu koşumda eski USB model testi bir REQUEST SENSE beklediğinden kırmızı verdi. Model kalıcı hata için dört REQUEST SENSE ve sonraki geçerli okumayı denetleyecek şekilde güncellendi. Üç ilgili saf test hedefi yeniden yeşil tamamlandı. Toplam test süresi 1391 saniye, bölüm 23 dakika.
