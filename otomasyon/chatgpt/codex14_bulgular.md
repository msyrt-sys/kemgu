# Dalga 14 ölçümleri

## Zamanlayıcı ve adres alanı

- Bir işlemcili x86_64 UEFI yolunda, kullanıcı kipinden gelen LAPIC kesmesinde 15 genel yazmaç ile RIP, CS, RFLAGS, RSP ve SS saklanıyor. Kesme çekirdek kipinde gelirse mevcut çağırana dönüyor. CR3 süreç kaydında tutuluyor. Çekirdeğe dönüşte eski CR3 ve CR0 geri yükleniyor.
- LAPIC saatinin hedef periyodu 10 ms. Süreç süresi için kullanıcı kipinde tamamlanan kesme dilimleri sayılıyor. Saat ve uyku davranışı dalga 13 ile uyumlu.
- 256 MiB OVMF konukta ilk ölçümde 49.761 çerçeve boştu. Bölüm 1 düzeninde bir canlı süreç 512 veri sayfası ve dört tablo sayfası tüketti. Biten çocuğun sonucu `bekle` için kayıtlı kalırken çerçeveleri genel ayırıcıya dönüyor.
- Klavye beklemesi için PS/2 IRQ1 halkası veya bekleyen xHCI aktarımının tamamlanma biti kullanılıyor. xHCI sürücüsü mevcut olay halkasını kullanıyor. Çekirdek hazır iş yokken LAPIC veya aygıt kesmesiyle uyanıyor.
- Yeni süreç çağrıları ve ikili kayıt biçimleri syscall ABI belgesinde 58–60 numaralarda yer alıyor.

### C14-001 — modülden içe alınan çıplak işlev

Aşağıdaki kaynak için C ön yüzü x86_64 LLVM IR üretti (rc=0). Self-host aynı çağrıyı E013 ile reddetti (rc=1). Her iki komut da depo kökünde çalıştırıldı.

```kem
kullan drivers::bellek::cerceve::{ CerceveAyirici, ayir_ardisik };
çıplak işlev c14_al(d: &değişken CerceveAyirici) -> tam64 {
    eşleş ayir_ardisik(d,1) { hiç => { ver 0; } değer(t) => { ver t; } }
}
işlev main() -> tam32 { ver 0; }
```

```sh
build/kemgu --llvm --mimari x86_64-none build/c14_import.kem
build/codegen --llvm --mimari x86_64-none build/c14_import.kem
```

Beklenen iki derleyicide de başarıydı. Self-host tanısı `E013 3 25`. Çağıran UEFI işlevi normal KEMGU işlevi olarak tanımlandı.

### USB klavye açılışındaki aralıklı hata

Dalga 13 süreç regresyonunun self-host serisinde sekizinci açılışta, ilk kullanıcı programı çalışmadan, seri uçta `KABUK HATA xhci interrupt IN yok` görüldü. Aynı kaynakların yeniden derlendiği 10 açılışın tamamı geçti. QMP bağlantısının kapanması bu çalışmada bir sonuçtu. Altta yatan neden doğrulanmadı.

### Çerçeve kâhininin düzeltilmesi

İlk bölümdeki 49 binli mutlak sayılar tarihsel ham çıktıdır. Kâhin Dizi<dtam8> bit eşleminin tamamını okumuyordu. Çalışma zamanı her elemanı 4 baytlık yuvada tutar. Kâhin tüm yuvaların düşük baytını sayacak şekilde düzeltildi. İlk düzeltilmiş C koşumunda 100 çocuk öncesinde ve sonrasında 47.902 boş çerçeve ölçüldü. Fark, çekirdek ELF boyu ve firmware yerleşimine göre açılışlar arasında değişebilir.

### Yükleyici bütçesi

Yeni ET_EXEC girişi 0x54200000, kod/veri üstü 0x54600000, heap sonu 0x54e00000, yığın tabanı 0x54f00000 ve pencere sonu 0x55000000. Üç üst düzey tablo ve yedi PT dahil 3.594 fiziksel çerçeve ayrılır. Eski giriş 0x54000000 için 516 çerçeveli düzen korunur. FAT32 okuma planı segmentin disk aralıklarını tutar, en büyük veri okuması 16 KiB'dır. Tüm yüklenebilir baytları 16 KiB'a sığan son program için bir adet 16 KiB tampon yeniden kullanılır. Veri bölümündeki değişiklik planı ve tamponu geçersiz kılar. Büyük ELF bu tamponu kullanmaz. Kabuğun heap yüksek su izi FAT/USB katmanının tahsislerini de içerir.

Büyük ELF bağlanmadan önceki 50 tekrar self-host denemesinde süreç kapsamındaki büyüme 4.262.960 bayttı. Sınırlı sıcak tampon ile aynı 50 tekrar +0 bayt oldu. Bir self-host regresyon serisinde ilk programın FAT okuma planı reddedildi. Temiz tekrar 10/10 geçti. Nedenin derleyicide olduğu doğrulanmadı.

### Ekran ve giriş

GOP metin hücreleri 8×16 piksel. Ön plan değişiminde ekran kabuk ızgarasından yeniden çizilir. Olay maskesinde 1 klavye, 2 fare. Klavye ve fare ayrı bekleyen xHCI TRB'lerine sahiptir. Tüketici tamamlamaları slot, uç ve TRB adresiyle ayırır ve depolama sürücüsüyle gerçek halka başını paylaşır. Q35'te MSI-X tablosu 64 bit BAR üzerinden yerleştirildi. Vektör 65 bildirimi cihazı ve LAPIC'i onaylar; HID ayrıştırması kesme dışında yapılır. QEMU deneyi, iki giriş yolunda da sekiz saniye bekleme, piksel eşitliği ve kabuğa dönüşü içerir. Fiziksel denetleyicide henüz ölçüm yok.

### Düzenleyici tamponu

Tek büyüyen dizi UTF-8 baytlarını saklar. Ekleme ve silme son eki kaydırır; imleç bir kod noktasının başında kalır. Satır ve sütun karakter sayısı üzerinden hesaplanır. Bellekte 256 KiB, FAT32 kaydında 64 KiB sınır uygulanır. Büyük bir kayıtta dosyayı açıp kısaltmadan önce boy denetlenir. Kayıt başarısı dosyanın yazılması, kapatılması ve dosya sisteminin eşitlenmesiyle bildirilir.
