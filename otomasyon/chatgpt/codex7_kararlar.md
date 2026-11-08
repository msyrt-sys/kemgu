# Dalga 7 kararlar

- C6-001: görevde verilen alan-adresi/T022 kararı uygulanır; salt okunur `&` değişmez.

- Ölçüm notu C6-001: ilk hedefli kapı bitmeden sabotaj başlatıldı; çakışan günlükler `build/codex7/INVALID-overlap-*` olarak ayrıldı ve kanıt sayılmadı. Kaynak yedeği geri yüklendi, ilgili nesneler/ikili/IR silindi; yeşil ve sabotaj ölçümleri seri tekrarlandı ve tamamlandı.
- Ölçüm notu C6-001: geçici doğrulayıcının tek tanı varsayımı kaldırıldı; sözleşme T022 dışında kod olmaması, LLVM üretiminin reddi ve `dizi_yaz` yönlendirmesidir. Mevcut çağrı denetimi aynı konumdaki T022’yi iki kez üretir; iki derleyici birebir aynı. Mevcut fikstür beklentisi veya harness değiştirilmedi.

- Ek C6-001 ölçümü: jenerik iç içe alanın `b.ic.v.n` okuması C checker T007 üretiyor (self kabul ediyor). Adres ödüncünden bağımsız mevcut sorun; okuma `değişken sonuc: Hucre = b.ic.v` üzerinden yapılarak aynı beklenen değer korunuyor. Jenerik adres hesabı iki derleyicide onarıldı.

- C6-003 ölçüm düzeltmesi: yeni LLVM vaka çağrısı geçici yol kurulumundan önce eklenmişti; ilk8/8 kırmızı geçersiz sayıldı. Çağrı kurulum sonrasına taşındı; bağımsız iki-derleyici ölçümünde yerel/boş pozitifler42, altı alan biçimi1 dönüyor. Kırmızı kapı tekrarlandı.

- C6-002 kapsam ölçümü: iç içe `d.ic.xs = []` C derleyicisinde de hatalıydı (IR/link0, koşumSIGSEGV). Kök aynı alan ataması bağlamı: tek katmanlı yardımcı yerine lvalue çözümünün zaten taşıdığı Dizi AST tipi kullanıldı. Yeni fikstür ve LLVM beklenen42 testi önce kırmızı yapıldı; iki derleyici birlikte onarıldı.

- C6-004 ASan ölçümü: eski doğrudan/boş dönüşler ASan altında da2/1 verdi, sanitizer tanısı üretmedi. Yanlış değeri exit42 denetimi yakaladı. Onarım sonrası dört fikstürün iki derleyicide8/8 ASan+UBSan koşumu42 ve stderr boş. ASan sonucu, başlatılmamış tüm bellek okumalarının yokluğu şeklinde yorumlanmadı.

## T1 — entegrasyon bekliyor

Son fetch'te origin/main `5598106`; `8dcc116`, `041f4bf`, `dc8e1e9`, `1e0bb49` commit'lerinin hiçbiri main'in atası değil. Her işin güncel main'den bağımsız dal olması kuralı gereği, henüz derleyicide bulunmayan davranışa sürücüleri geçirmek doğru olmaz. Merge/auto-merge yetkisi kullanılmadı; T1 için kod değişikliği veya PR açılmadı.

Entegrasyon sonrası devam noktaları:

- C6-001: `drivers/usb/xhci_cekirdek.kem:42` komut halkası, `:53` kontrol halkası ve olay halkasının alan kopyala/geri-yaz yolları. Yalnız tanımlayıcı köklü alan ödüncü sadeleşir; dizi elemanı yolları T022 kararı gereği yerel+dizi_yaz biçiminde kalır.
- C6-002: aynı dosya`:122` tipli boş kuyruk dolanması.
- C6-003: aynı dosya`:94` ve`:258` uç dizisinin yerel kopyası üzerinden döngüler.
- C6-004: `tests/drivers/usb/xhci_qemu_fikstur.kem` iki tipli ara dönüş ve bunu üreten `tools/codex/dalga6_xhci_fikstur.py:21`.

T1'de gerçek QEMU'nun6 dalı ve bare-metal NVMe kapısı tekrar yeşil olmalı. Bu rapor mevcut dolanmaların yanlış olduğu iddiası değildir; main'in mevcut derleyicisi hâlâ onlara ihtiyaç duyar.

VARSAYIM: son belge dalı `codex/d7-ozet` olarak adlandırıldı. Zorunlu dört kusurda60 dakika takılma sınırına ulaşılmadı; takılı kalan zorunlu iş yok. Ek jenerik okuma T007 farkı bu dalganın dışında ölçüm bulgusu olarak bırakıldı; dil yüzeyine ilişkin yeni karar verilmedi.
