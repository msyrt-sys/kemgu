# KEMGU-OS x86 çok işlemci desteği

## İşlemci keşfi

Başlangıç işlemcisi (BSP) UEFI'nin ACPI kökünden MADT'yi bulur. Tür 0 kaydındaki yerel APIC kimliği yönlendirme adresidir. ACPI işlemci tanıtıcısı ve KEMGU çekirdek numarası ayrı alanlardır. BSP kendi kimliğini xAPIC MMIO sayfasındaki 0x20 yazmacının üst baytından okur. Saf süzgeç bit 0 (etkin) veya bit 1 (çevrimiçi yapılabilir) taşıyan yerel APIC kayıtlarını alır. İki bit de kapalıysa kayıt listeye girmez. [ACPI 6.6, MADT ve yerel APIC yapıları](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt)

KEMGU sıfır numarasını BSP'ye verir. Diğer numaraları kabul edilen kayıtların MADT sırasıyla dağıtır. BSP'nin yokluğu, yinelenen etkin kimlik, geçersiz kimlik ve 256'yı aşan yerel kayıt sayısı açık hatadır. Gerçek UEFI çekirdeği tek BSP için sabit bir kimlik yuvası kullanır. Çok işlemcili açılışta listeyi fiziksel ayırıcıdan aldığı bir sayfaya yazar. İşlemci keşfi ile uygulama işlemcisinin (AP) görev yürütmeye başlaması ayrı aşamalardır.

```sh
make -j10 --output-sync=target calistir_bm_x86_smp_kesif
```

Kapı C ve self-host derleyicilerinin ürettiği ayrı UEFI imajlarını q35 üzerinde 1, 2, 4 ve 8 vCPU ile açar. Kâhin ham MADT'yi QMP ile fiziksel bellekten okur ve sağlama toplamını hesaplar. Her CPU'nun QOM `apic-id` değeri, konuk seri satırı ve KEMGU'nun bellek kaydı karşılaştırılır. `query-cpus-fast` içindeki üst düzey `thread-id` alanı host iş parçacığıdır. TCG için `thread=multi` kullanılır ve her vCPU'nun farklı host iş parçacığında olduğu sınanır. [QEMU vCPU ve QMP alanları](https://www.qemu.org/docs/master/system/cpu-hotplug.html)

| QEMU vCPU | MADT APIC kimlikleri | Her iki derleyicide ayrı host iş parçacığı | MADT bayrakları |
|---:|---|---:|---|
| 1 | 0 | 1 | Her kayıtta 1 |
| 2 | 0, 1 | 2 | Her kayıtta 1 |
| 4 | 0–3 | 4 | Her kayıtta 1 |
| 8 | 0–7 | 8 | Her kayıtta 1 |

## AP başlatma

BSP, firmware'in aktardığı bellek tanımlarını adım boyuyla gezer. Türü 7 (`EfiConventionalMemory`) olan tek bir tanım içinde, sıfır sayfasından sonra ve 1 MiB altında iki bitişik 4 KiB sayfa seçer. KEMGU fiziksel çerçeve ayırıcısı bu alt MiB'yi yönetmez. İlk sayfa trampolin, ikincisi parametreler içindir. Uygun çift bulunmazsa BSP seri günlüğe sebebi yazar ve tek işlemciyle açılışa devam eder.

`boot/ap_x86_64.S` ilk sayfaya kopyalanan 16, 32 ve 64 bit kodu ve geçici GDT'yi içerir. BSP seçilen fiziksel adrese göre GDT işaretçisini ve iki uzak atlamayı yamar. Parametrelerde BSP'nin CR3'ü, CR4'ü, AP'nin yığın tepesi, KEMGU giriş adresi ve çekirdek numarası bulunur. AP önce korumalı kipe, ardından PAE ve uzun kipe geçer. Her AP için dört çerçevelik yığın ayrılır. BSP tahsis edilen aralığı ve kendi yığınıyla çakışmadığını SIPI öncesinde denetler.

BSP her hedefi gerçek xAPIC kimliğiyle seçer. ICR teslim biti için 100 ms sınır vardır. INIT sonrasında en az 10 ms, iki SIPI arasında en az 200 µs beklenir. Süre PIT ile kalibre edilmiş TSC'den ölçülür. AP KEMGU girişinde gerçek yığın işaretçisini, CR3'ü ve kendi LAPIC kimliğini yazar. Onay sözcüğünü en son yayımlar. BSP bir saniyelik sınır içinde bu onayı almadan parametre sayfasını sonraki AP için yeniden kullanmaz. [Intel SDM, Cilt 3](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)

```sh
make -j10 --output-sync=target calistir_bm_x86_smp_ap
```

Kapı olağan UEFI çekirdeğini 2, 4 ve 8 vCPU ile iki derleyicide açar. QMP, her vCPU'dan CR0, EFER, CS, CR3 ve RSP okur. Kâhin uzun kipi, ortak açılış CR3'ünü, ayrık yığınları, firmware'deki gerçek sayfa türünü ve AP'lerin bellek kayıtlarını doğrular. Ayrı bir dört işlemcili koşum olağan UEFI kabuğunun komut istemine kadar ulaşıp düzenli kapandığını gösterir.

## Çekirdek başına veri ve kesmeler

Her işlemcinin çekirdek GS tabanı kendi `CekirdekVeri` kaydını gösterir. `cekirdek_no()` ilk sekiz baytı GS üzerinden okur. Kayıtta APIC kimliği, yerel tık, çalışan süreç, yığın aralığı, TSS, GDT, üç IST tepesi ve CR3 bulunur. BSP açılışta statik kaydı ve var olan tabloları kullanır. Her AP'nin fiziksel ayırıcıdan alınan 13 bitişik sayfasının ilki veri ve tablolar, kalan 12 sayfası üç ayrı 16 KiB IST yığını içindir. Normal AP yığını bu alanın dışındadır.

BSP'nin IDT'si ortaktır. AP kendi GDT'sini, 0x08 kod seçicisini ve 0x28 TSS seçicisini yükler. Üç IST sırasıyla çift hata, NMI ve makine denetimi içindir. Panik yolu etkin TR ve GDTR'den o işlemcinin TSS'ine ulaşır ve üç yığının alt sınırındaki kanaryayı denetler. Normal kullanıcı zaman dilimi kurulurken süreç kimliği ve CR3 kayda yazılır. Ring3 girişinde `swapgs` kullanıcı tabanını seçer. Kullanıcıdan gelen sistem çağrısının dağıtımı çekirdek tabanını kullanır. Dilim bittiğinde önceki GS ve kesme durumu geri yüklenir.

AP'ler ortak sayfa tablolarını kullanırken BSP'nin PAT ve CR0 önbellek ayarlarını devralır. Yerel zamanlayıcıların bölücüsü ve başlangıç değeri BSP'nin PIT kalibrasyonundan gelir. BSP vektör 48 üzerinden sistem saatini de ilerletir. AP'ler vektör 49 üzerinden yalnız kendi tıklarına yazar ve kendi LAPIC'lerine EOI verir.

```sh
make -j10 --output-sync=target calistir_bm_x86_smp_tik
```

Kapı dört vCPU'da gerçek GS, GDT, TR ve IDT tabanlarını QMP ile okur. TSS tanımlayıcılarını, on iki IST aralığını ve kanaryaları fiziksel bellekte doğrular. QEMU duraklatılarak alınan iki örnekte BSP en az 150 tık ilerler. AP farklarının BSP farkına göre sapma sınırı %20'dir. Ayrı kabuk senaryosu bir ve dört vCPU'da kullanıcı ELF'sini çalıştırıp sistem çağrısı, kullanıcı GS tabanı ve dönüşten sonraki BSP GS tabanını doğrular.

## Bilet kilidi

Bir kilit en az 8 bayta hizalı 32 bayt kaplar. İlk iki 32 bit sözcük alınacak ve sunulan bilettir. İzleyen üç 64 bit sözcük kilit sırasını, sahibin çekirdek numarası artı biri ve sahibin önceki sırasını tutar. Sahip alanında sıfır boş demektir. Bir çekirdek aynı anda bir bekleyen bilet tutabilir. Bu sınır altında 32 bit sarma, FIFO sırasını korur.

`kilit_kur` kilidi paylaşmadan önce bir kez çağrılır. `smp_kilit_al` atomik `lock xaddl` ile bilet alır. Bilet sunulmadıysa çekirdeğin çekişme sayacını artırıp bekler. Kısa sorgulama döngüsünün her 1.024 başarısız karşılaştırmasında `pause` çalışır. `smp_kilit_birak` önce sahip alanını sıfırlar, ardından sunulan bileti ilerletir. Sunulan sayaca yalnız kilidin sahibi yazar. `kilit_dene` tek bir `lock cmpxchgl` ile boş kilidi alabilir. Başarısız deneme kuyrukta bilet bırakmaz.

Kesme işleyicisinin de kullandığı kilitlerde `kilit_al_kesmesiz` önce RFLAGS değerini saklar ve yerel kesmeleri kapatır. `kilit_birak_kesmesiz` önce kilidi bırakır, sonra saklanan IF bitini geri yükler. İç içe çağrıda iç kilidi bırakmak kesmeleri erken açmaz. `KEMGU_KILIT_DENETIM=1` ile oluşturulan UEFI çekirdeği kilidi ancak daha büyük sıra numarasıyla edinir ve bırakırken sahibi ve sırayı doğrular. Çekirdeğin etkin sırası GS+112, çekişme sayacı GS+120 konumundadır.

| Bölüm 12 kapısında sınanan kilit | Sıra |
|---|---:|
| Paylaşılan sayı ve dış kilit | 10 |
| İç içe kesme durumu ve 32 bit sarma | 20 |

`make calistir_bm_x86_smp_kilit` iki derleyicide sekiz istemcili FIFO modelini 100.000 adım yürütür. QEMU'da dört ve sekiz vCPU'nun her biri ortak sayacı bir milyon kez kilit altında artırır. İlk 8.192 artışta bütün vCPU'lar, kalanında ikili gruplar yarışır. QMP her çekirdeğin iki aşamadaki katkısını, biletleri, toplamı ve çekişme sayaçlarını fiziksel bellekten okur. Sekiz vCPU'nun deney boyunca birlikte yarıştığı ayrı C koşumu 378,842 saniyede 8.000.000 artış verdi. Bu süre ARM üzerinde çok iş parçacıklı TCG ölçümüdür.

## Fiziksel çerçeveler ve heap

Çok işlemcili açılış, çekirdek kayıtları ve IPI kutuları için ayrılan fiziksel alanın sonuna iki bilet kilidi yerleştirir. Çerçeve ayırıcısının sırası 20, heap'in sırası 30'dur. Her iki girişte kesmeler saklanır, bırakırken önceki IF geri yüklenir. BSP, kilitleri AP'lere SIPI göndermeden kurar. Tek işlemcide kilit tabanı sıfırdır.

`uefi_cerceve_ayir` ve `uefi_cerceve_iade` bir aralığın tamamı için sahipliği değiştirir. Süreçlerin, RAM diskin ve sayfa tablolarının kullandığı ortak bit eşlemine bu girişlerden erişilir. `uefi_bos_say` aynı kilitle tutarlı bir anlık sayım yapar. Kilitli x86 imajının `malloc` ve `free` girişleri, aynı serbest liste ve bump algoritmasını tek bir heap kilidiyle çağırır. ARM ve konak bileşimleri paylaşılan dosyanın yalın girişlerini kullanır. `test/x86_heap_bagla.py` x86 SMP bileşiminde iki geçişi doğrular ve ilgili KEMGU işlevlerine bağlar. Bileşimin son baytları çekirdek önbelleği anahtarına girer.

`make calistir_bm_x86_smp_bellek` dört gerçek vCPU'da çekirdek başına 10.000 tahsis ve iade yapar. Aynı döngü `malloc(32)` ile aldığı bloğu kullanır ve serbest bırakır. Host konuk başlamadan ayırıcının fiziksel bit eşlemini okur. Sonunda aynı bitlerin geri geldiğini, ayrı sahiplik tablosunun boşaldığını ve 80.000 olayda her iadenin o anda sayfanın sahibi olan çekirdekten geldiğini doğrular. İlk turda dört farklı çerçeve ve dört farklı heap bloğu aynı anda tutulur. Tek vCPU'daki ayrı kabuk ölçümü 50 kullanıcı sürecini çalıştırır.

## İşlemciler arası kesmeler

SMP açılışında çekirdek kayıtlarının ardından her hedef için 128 bayt ayrılır. Kayıt +56 kutunun fiziksel adresidir. Kutu, çekirdek çevrimdışı veya tek vCPU açılışında kullanılmaz. Hedef numarası MADT dizisinden gerçek xAPIC kimliğine çevrilir. Gönderen, ICR yüksek/düşük yazım çifti sırasında kendi kesmelerini kapatır ve önceki IF durumunu geri yükler.

| Vektör | Hedefteki iş |
|---:|---|
| 80 | Kayıtlı çekirdek işlevini çalıştırır. İşlem 1 çağrı verisine hedef kimliğini ekler. İşlem 2 hedefin yerel zamanlayıcı tıkını okur. |
| 81 | Hedefin yeniden zamanlama istek sayısını artırır. |
| 82 | Duruşu kaydeder, EOI verir, kesmeleri kapalı olarak HLT döngüsüne girer. |

| Bayt | 64 bit alan |
|---:|---|
| 0 | Durum. 0 boş, 1 yazılıyor, 2 teslim bekliyor, 3 onaylandı. |
| 8, 16, 24 | Bilet, gönderen çekirdek, vektör |
| 32, 40 | İşlev girdisi ve dönüş değeri |
| 48, 56, 64 | Teslim sayısı, yeniden zamanlama sayısı, durdu bilgisi |
| 72 | Çekirdek işlev numarası |

`ipi_cagir(hedef, islem, veri)` ve `ipi_gonder(hedef, tur)` bir bilet döndürür. Atomik karşılaştırma/değiştirme boş kutuyu edinir. Yanlış argüman −1, dolu kutu −2, önceki ICR aktarımının süreyi aşması −3 verir. `ipi_bekle(hedef, bilet, ms)` doğru gönderen ve bilet için sonucu okur, sonra kutuyu boşaltır. Süre aşımı −4 döndürür ve kutu sahipliği korunur. Aynı bileti yeniden beklemek mümkündür. `ipi_hepsine(tur)` diğer çevrimiçi çekirdeklere sırayla gönderir, her teslimi bekler ve onay sayısını döndürür. Sabit sayıda RAM erişimi yapan kesme işleyicisi kullanıcı kipinden giriş ve çıkışta GS tabanını değiştirir.

`make calistir_bm_x86_smp_ipi` bütün sıralı çekirdek çiftlerini, kendine gönderim dahil, biner kez çalıştırır. QMP her sonuçtaki hedefi, kaynağı ve sırayı, çekirdek başına teslimleri ve tüm gidiş-dönüş sürelerini fiziksel bellekten okur. Dört vCPU'nun ardışık kimliklerine ek olarak iki soket × üç çekirdek topolojisinde APIC kimlikleri 0, 1, 2, 4, 5, 6'dır. TSC/ms PIT kalibrasyonundan gelir. Süreler ARM ana makinede çok iş parçacıklı x86 TCG'ye aittir.

## TLB geçersizleme ve çerçeve iadesi

`smp_alan_degistir` yerel kesmeleri saklar, GS+128 sürümünü tek yapar, donanım CR3'ünü ve GS'deki CR3/PID çiftini değiştirir, ardından çift sürümü yayımlar. Bu yol PCID kullanmaz. Kök değiştirmek önceki küresel olmayan çevirileri boşaltır. Bir tarama sürerken geçişte yakalanan çekirdek de hedeflere dahil edilir.

`smp_esleme_kaldir` kullanıcıya açık dört düzeyli, 4 KiB, küresel olmayan yaprağı doğrular. Atomik karşılaştırma ve değiştirme ile tek bir sahip PTE'yi sıfırlayabilir. Yerel INVLPG'den sonra aynı kökü kullanabilecek uzak çekirdeklere vektör 83 gider. Kutu+32 sanal adresi, +80 kökü, +88 gerçek uzak INVLPG sayısını taşır. İşleyici donanım CR3 hâlâ eşleşiyorsa INVLPG çalıştırır ve ardından onay yayımlar. Büyük sayfa, küresel eşleme ve belirtilen aralığın dışı hata döndürür.

Dolu kutuda −2, süre aşımında −4 döner. Çağıran çerçeveyi ve `SmpTlbIade` kaydını saklar. Aynı gönderici `smp_tlb_tamamla` ile beklemeyi kaldığı hedeften sürdürür. Fiziksel adres son onaydan sonra bir kez döner. Çağıran bu süre boyunca kökü ve ara tabloları korur. Kabuğun sonlanmış süreç temizliği ayrıca eski kökün hiçbir işlemcide etkin olmadığını denetler. Süreç yaşam döngüsünde yalnız tüm adres alanı serbest bırakılır.

`make calistir_bm_x86_smp_tlb` iki çekirdeğin paylaştığı kullanıcı sayfasının iki okumasını, ayrı kökte kalan iki çekirdeği, gerçek PTE'yi, uzak onayı ve sıkıştırılmış fiziksel bitmap'teki tahsis bitini QMP ile okur. AP1'in sonraki okuması normal sayfa hatası işleyicisine girer. CR2 hem seri günlükte hem donanım yazmacında doğrulanır. Aynı kapı dört vCPU'lu süreç kabuğunda kullanıcı PTE izinlerini, NX ve koruma sayfasını ve yüz çocuk sürecin yalıtımını da sınar.

## Kilitler, IPI ve adres alanı tasarımı

İlk sürümde sistem çağrıları ve sürücüler büyük çekirdek kilidi altında çalışır. Çerçeve ayırıcısı ve heap girişleri kendi paylaşımlarını korur. Kilit sırası ve kesme durumunun korunması edinim noktalarında denetlenir. Çağrı kutusu, yeniden zamanlama ve durdurma için ayrı IPI vektörleri ayrılır. Bir eşleme kaldırıldığında o adres alanını kullanan diğer çekirdeklerin TLB geçersizleme onayları beklenir.

## Donanım ve ölçüm kapsamı

Dalga x86-64 UEFI ve xAPIC yolunu hedefler. ARM SMP, x2APIC, NUMA, işlemci hotplug, süreç içi iş parçacıkları ve fiziksel makine koşumu ayrı çalışmalardır. ARM ana makinedeki x86 TCG, paralel host iş parçacıkları kullansa da kesme gecikmeleri ve bellek işlemlerinin serpiştirilmesi gerçek x86 donanımından farklıdır.
