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

### Gezgin ve ön plan

`onplan_baslat` mevcut yükleyiciyle aynı yolu ve argv doğrulamasını kullanır. Çağıranın ön plan sahibi olması gerekir. Çocuk daha ilk talimatından önce ekranı devralır. Ebeveyn `bekle` ile sonucu alır, dizini tekrar okur ve seçim satırını korur. Eski `baslat` arka planda çalıştırma davranışını korur. Gezginin 512 satırlık sınırına `../` dahildir; 511 gerçek girdiyi aşan dizinde -34 döner. Silmeden yazma yetkisi yoklanarak salt okunur dosyanın durumu açılışta görünür.

### V1 oturumu ve kayıt kalıcılığı

İç diskler salt okunur kaldı. USB'nin FAT32 veri bölümünde son içerik `Ağaç ve İş\nİş\nAğaç\nSon` oldu. Her düzenleyici kaydından sonra bağımsız FAT okuyucusu ve fsck çalıştı. Normal kapanışa ek olarak ikinci kayıttan hemen sonra QEMU kapatıldı. Bu ölçüm sonradan yapılacak kabuk eşitlemesinin ilk yazma hatasını gizlemesini önlüyor. Fiziksel USB'ye yazma veya gerçek PC açılışı yapılmadı.

### Kaynak ve süre ölçümü

Ana makine aarch64, konuk x86_64 QEMU/TCG + OVMF. Zaman ve heap tablosundaki tipik değerler on temiz açılışın medyanlarıdır. Her çocuk başlarken ve ebeveyni topladığında QMP, UEFI ayırıcısının bit eşlemini ve çekirdek heap yüksek su izini okudu. Son çocukla birlikte ölçüm programı da çıkıyor. Aşağıdaki heap sütunları çocuğun başladığı anda ölçüldü.

| Döngü | C çekirdek heap (bayt) | Self-host çekirdek heap (bayt) |
|---:|---:|---:|
| 0, kabuk | 4.394.944 | 4.657.408 |
| 1 | 4.788.448 | 4.985.328 |
| 25 | 4.788.448 | 5.050.912 |
| 50 | 4.919.616 | 5.182.080 |
| 75 | 5.050.784 | 5.313.248 |
| 100 | 5.247.536 | 5.510.000 |

Her süreç 3.594 çerçeve (14.721.024 bayt) kullandı. C ile kabukta 47.896, ebeveyn tek başınayken 44.302, ebeveyn ve çocuk birlikteyken 40.708 boş çerçeve ölçüldü. Self-host karşılıkları 47.856, 44.262 ve 40.668. Her iki derleyicide yüzüncü çocuk ve ebeveyn çıkınca başlangıç sayısı geri geldi. 1.000'er çocuk için konuk içinde ölçülen `baslat` medyanı bir LAPIC tiki (hedef periyot 10 ms). Ana makineden QMP başlatma tuşunu gönderip seri bildirimini almaya kadar C 93,33 ms, self-host 93,14 ms ölçüldü. Bu ikinci süre giriş ve seri aktarımını içerir.

50 ardışık düzenleyici aç/kaydet/kapat istendi. Bir sonraki kayıt için yer koruyan 24 MiB eşiği iki derleyicide de dokuzuncu kayıttan sonra aşıldı. Aşağıdaki sürekli oturumlarda her kapanıştan sonra dosya yeniden okundu ve boş çerçeve sayısı başlangıca döndü. Ayrı olarak onar taze açılışta beşer kayıtla derleyici başına 50 kaydın tümü ve fsck doğrulandı.

| Aynı açılışta tamamlanan kayıt | C heap (bayt) | Self-host heap (bayt) |
|---:|---:|---:|
| 0 | 4.526.112 | 4.788.576 |
| 1 | 6.821.616 | 7.280.928 |
| 2 | 9.248.288 | 9.707.568 |
| 3 | 11.674.928 | 12.134.208 |
| 4 | 14.101.568 | 14.560.848 |
| 5 | 16.528.208 | 16.987.488 |
| 6 | 18.954.848 | 19.414.128 |
| 7 | 21.381.488 | 21.840.768 |
| 8 | 23.873.712 | 24.267.408 |
| 9 | 26.300.352 | 26.694.048 |

Birleşik UEFI kabuk birimi 6.360 → 7.253 satır. Aynı ana makinede self-host `--llvm --mimari x86_64-none` duvar süresi 27,85 → 33,92 saniye (birer ölçüm, eşzamanlı işler vardı). Her açılışta ham veriler ilgili derleme dizinine `kaynak-çocuk-NN.csv` ve `kaynak-düzenleyici-NN.csv` olarak yazılır.

### Donanım kapsamı ve aralıklı sonuçlar

Q35 xHCI üzerinde vektör 65 doğrulandı. MSI-X kurulamadığında mevcut USB uyumluluk yolu, aktarım halkasını çekirdeğin uyanışlarında denetler. Fiziksel makinede MSI-X, GOP ve aygıt uyumu H11b listesindeki açık deneylerdir. Bölüm 8 süreç matrisi ilk self-host denemesinde üçüncü açılışta ilk ELF'i `ELF kisa okuma` ile reddetti. Aynı matrisin taze self-host serisi 10/10 geçti. Ek düzenleyici süre ölçümünde ilk self-host serisinin üçüncü açılışında `ELF okuma plani` görüldü. Bu durumlar kullanıcı programına denetim verilmeden önce oluştu.

Düzenleyiciyi çağıran komutun Enter tuşundan, bağımsız piksel modeliyle eşleşen ilk GOP görüntüsünün alınmasına kadar geçen süre, 50'şer aç/kaydet/kapat çevriminde C için medyan 218,72 ms (189,98–243,58), self-host için 194,99 ms (190,91–231,14). Ölçüm QMP girişini ve görüntünün ana makineye aktarımını içerir; model karşılaştırmasının işlem süresi bu değerden ayrıldı. Yeniden derlenen self-host süre serisi 10/10 geçti.

İlk ELF yüklemesini ayrıca `SUREC_DERLEYICILER=build/codegen SUREC_SENARYO=baslat SUREC_TEKRAR=60 bash test/bm_x86_surec_harness.sh` ile nihai kaynakta 60 açılışta denedim. 60/60 geçti. Dolayısıyla önceki aralıklı reddin nedeni ve hangi disk/derleyici koşuluna bağlı olduğu açık kaldı.

### Birleşik birimlerin ayrı derleme maliyetleri

D-762 kaynak ağacı ve nihai ağaç aynı self-host ikilisi ile yeniden ölçüldü. Kullanıcı birimleri ELF üreticisindeki dört ortak kaynağı içeriyor. Tablodaki kullanıcı süreleri üç çalıştırmanın medyanı, ABI ve blok süreleri birer çalıştırmadır. Süreler yalnız LLVM IR üretimine aittir.

| Birim | D-762 satır | Nihai satır | D-762 sn | Nihai sn |
|---|---:|---:|---:|---:|
| bolme | 828 | 852 | 0,172 | 0,182 |
| buyukelf | — | 912 | — | 0,206 |
| dosyalar | — | 966 | — | 0,235 |
| duzenle | — | 962 | — | 0,400 |
| ekranden | — | 863 | — | 0,246 |
| hesap | — | 872 | — | 0,172 |
| kaynakden | — | 877 | — | 0,181 |
| kedi | 842 | 866 | 0,177 | 0,180 |
| merhaba | 852 | 876 | 0,170 | 0,183 |
| say | 857 | 881 | 0,174 | 0,202 |
| surecden | — | 898 | — | 0,214 |
| tus | 854 | 878 | 0,195 | 0,178 |
| yazdene | 887 | 911 | 0,198 | 0,184 |
| UEFI blok | 4755 | 4878 | 9,099 | 9,374 |
| süreç ABI | 3967 | 3967 | 3,648 | 3,688 |

### Ek USB klavye tanısı

Paralel tam takımda C ile normal kapanış ve güç kesme 20/20 geçti. Self-host normal kapanış 10/10 geçti. Güç kesme serisinde üç başarıdan sonraki dördüncü açılışta konuk `KABUK_HAZIR` yazdı, ilk kullanıcı programı başlamadan `KABUK HATA USB klavye aktarimi` bildirdi ve QMP kapandı. Böylece v1 hedefi 34. açılışta durdu. Aynı hedefi temiz ikililerle yeniden çalıştırınca 40/40 geçti.

Ayrı bir tanı kopyasında ilk ELF senaryosuna USB fare de bağlandı. Klavye + fare + depolama ile 34 başarıdan sonraki açılışta HID interrupt IN uç seçimi başarısız oldu. Uç kimliğini aygıt yapılandırmasında saklama denemesi de dördüncü açılışta durdu. Üç aygıtın kayıtları karşılaştırıldığında, bir başka serinin 19. açılışında ikinci yuvanın DMA alanı başlangıçtaki 66.191.360 yerine çekirdek yığınına denk gelen 35.479.504 değerini, uç kimliği ise 3 yerine 0 değerini taşıyordu. Diğer iki yuvanın bu alanları korunmuştu. Aktarım daha kurulmadan reddedildi. Ek kayıt denetimleriyle altı başarılı açılıştan sonra bu kez `xhci halka dolu veya bozuk` görüldü. Tanı değişiklikleri ve sonuç vermeyen uç önbelleği teslim edilen kaynaklara alınmadı.

Self-host çekirdeğin makine kodunda `xhci_adim` içindeki aygıt eklemesinde yapı yığına yazıldıktan sonra RSP eski değere dönüyor. Kaynak adresi bir yazmaçta tutulurken kopya için yığın yeniden ayrılıyor. Kesmenin bu aralığa denk gelip gelmediği ölçülmedi. Bağımsız, yalnız 18 tamsayı alanı içeren bir kayıt örneğinde C ve self-host ikisi de derlendi, aynı makine kodu örüntüsü görülmedi. Bu nedenle USB ve ilk ELF okumalarında görülen tüm hataların kök nedeni açık. Tanı için en küçük mevcut sistem koşumu `SUREC_DERLEYICILER=build/codegen SUREC_SENARYO=v1-kesinti SUREC_TEKRAR=10 bash test/bm_x86_surec_harness.sh`. C karşılığı aynı komutta `build/kemgu`. Her açılışın kaydı sağlam olarak doğrulayıp kapanması beklenir.
