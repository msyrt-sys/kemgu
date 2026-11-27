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

## Kilitler, IPI ve adres alanı tasarımı

İlk sürümde sistem çağrıları ve sürücüler büyük çekirdek kilidi altında çalışır. Çerçeve ayırıcısı ve heap girişleri kendi paylaşımlarını korur. Kilit sırası ve kesme durumunun korunması edinim noktalarında denetlenir. Çağrı kutusu, yeniden zamanlama ve durdurma için ayrı IPI vektörleri ayrılır. Bir eşleme kaldırıldığında o adres alanını kullanan diğer çekirdeklerin TLB geçersizleme onayları beklenir.

## Donanım ve ölçüm kapsamı

Dalga x86-64 UEFI ve xAPIC yolunu hedefler. ARM SMP, x2APIC, NUMA, işlemci hotplug, süreç içi iş parçacıkları ve fiziksel makine koşumu ayrı çalışmalardır. ARM ana makinedeki x86 TCG, paralel host iş parçacıkları kullansa da kesme gecikmeleri ve bellek işlemlerinin serpiştirilmesi gerçek x86 donanımından farklıdır.
