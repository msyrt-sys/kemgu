# KEMGU-OS x86 çok işlemci desteği

## İşlemci keşfi

Başlangıç işlemcisi (BSP) UEFI'nin ACPI kökünden MADT'yi bulur. Tür 0 kaydındaki yerel APIC kimliği yönlendirme adresidir. ACPI işlemci tanıtıcısı ve KEMGU çekirdek numarası ayrı alanlardır. BSP kendi kimliğini xAPIC MMIO sayfasındaki 0x20 yazmacının üst baytından okur. Saf süzgeç bit 0 (etkin) veya bit 1 (çevrimiçi yapılabilir) taşıyan yerel APIC kayıtlarını alır. İki bit de kapalıysa kayıt listeye girmez. [ACPI 6.6, MADT ve yerel APIC yapıları](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt)

KEMGU sıfır numarasını BSP'ye verir. Diğer numaraları kabul edilen kayıtların MADT sırasıyla dağıtır. BSP'nin yokluğu, yinelenen etkin kimlik, geçersiz kimlik ve 256'yı aşan yerel kayıt sayısı açık hatadır. Keşif çekirdeği kimlik listesini fiziksel ayırıcıdan aldığı bir sayfaya yazar. İşlemci keşfi ile uygulama işlemcisinin (AP) görev yürütmeye başlaması ayrı aşamalardır.

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

## AP başlatma tasarımı

BSP düşük bellekte UEFI'nin kullanılabilir bildirdiği hizalı bir sayfayı ayırır. INIT ve iki SIPI gönderiminden sonra AP'nin kendi açılış kaydını yayımlaması zaman sınırıyla beklenir. Trampolin BSP'nin sayfa tablosuna geçer ve her AP için ayrılan yığınla KEMGU girişini çağırır. Vektör, gerçek kip giriş adresini kodlar. Bu akış için mimari başvuru Intel'in sistem programlama kılavuzundaki çok işlemcili başlatma ve yerel APIC bölümleridir. [Intel SDM, Cilt 3](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)

## Çekirdek başına durum ve zamanlama tasarımı

Her çekirdek için çekirdek numarası, APIC kimliği, yığınlar, TSS, GDT, yerel zamanlayıcı ve çalışan süreç tutulur. GS tabanı bu kayda erişim sağlar. Ortak IDT, çekirdek başına TSS üzerinden o çekirdeğin IST yığınlarını seçer. Zamanlayıcıda süreç tek iş parçacıklıdır. Hazır kuyrukları çekirdek başınadır. Boş çekirdek başka bir kuyruktan hazır süreç alabilir.

## Kilitler, IPI ve adres alanı tasarımı

İlk sürümde sistem çağrıları ve sürücüler büyük çekirdek kilidi altında çalışır. Çerçeve ayırıcısı ve heap girişleri kendi paylaşımlarını korur. Kilit sırası ve kesme durumunun korunması edinim noktalarında denetlenir. Çağrı kutusu, yeniden zamanlama ve durdurma için ayrı IPI vektörleri ayrılır. Bir eşleme kaldırıldığında o adres alanını kullanan diğer çekirdeklerin TLB geçersizleme onayları beklenir.

## Donanım ve ölçüm kapsamı

Dalga x86-64 UEFI ve xAPIC yolunu hedefler. ARM SMP, x2APIC, NUMA, işlemci hotplug, süreç içi iş parçacıkları ve fiziksel makine koşumu ayrı çalışmalardır. ARM ana makinedeki x86 TCG, paralel host iş parçacıkları kullansa da kesme gecikmeleri ve bellek işlemlerinin serpiştirilmesi gerçek x86 donanımından farklıdır.
