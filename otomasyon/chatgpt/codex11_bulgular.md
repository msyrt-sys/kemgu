# Dalga 11 bulguları ve tasarım seçimleri

- IST1 #DF, IST2 NMI, IST3 #MC için ayrı 16 KiB yığın; alt sınır kanaryası. PVH/UEFI girişleri ve ring3/kurtarılabilir #PF yolları korunacak.
- PVH zamanlayıcı PIC olarak korunacak; UEFI yolu MADT/IOAPIC/LAPIC kullanacak.

- QEMU ölçümü: kanonik olmayan `0x8000000000000000` adresini okumak #GP(0) üretirken CR2’yi de bu adrese değiştirdi. CR2, #GP adresini bildiren kayıt olmadığından fikstür bunun yerine GDT dışı seçici `7*8=0x38` yükler; beklenen hata kodu 0x38. Beklenti üretim çıktısına uydurulmadı, ayırt edici mimari ihlal seçildi.

- Taban gerçek QEMU/OVMF RAM dökümü: MADT 120 bayt, LAPIC 0xfee00000; IOAPIC kimlik 0, adres 0xfec00000, GSI tabanı 0. ISO: IRQ0→GSI2 bayrak 0; IRQ5/9/10/11→aynı GSI, bayrak 0x0d (yüksek etkin, seviye). IRQ1/12 için ISO yok: GSI1/GSI12. LAPIC NMI tüm işlemciler, LINT1.
- A/B: #DF CR2=0x60000ff8 (eşlenmemiş 0x60001000 RSP’ye ilk 8 baytlık itme), vektör8/hata0; QMP RSP IST1 aralığında. Her üç 16 KiB IST’nin alt kanaryası 0x4b454d4755495354; panik girişinde üretim denetimi ve QMP ham bellek karşılaştırması var. Koruma sayfası yerine kanarya seçildi; taşmayı erişim anında engellemez, panik girişinde ve kâhinde bildirir.
- E4n: etkin heap CAT listeleri 16 kez tarandı (Makefile 2, harness/sınıflandırma 14); ARM64 UART parçası sessiz kancayı, x86 MMU parçası ekran kancasını taşır. İstisna mesajı .bss 256 bayt; 255 bayt + NUL sınırı; iç içe giriş tahsis yapmadan seri + HLT. Eski ISTISNA seri satırı aynen, ardından ekranla aynı yeni satır.

- C ölçümü: PIT mode3 ile 10 IRQ aralığı LAPIC’te ~3.12M sayım verdi; sonuç ~200 Hz ve kâhin kırmızı. QEMU 8.2.2 `i8254_common.c` geçiş zamanını tam sayı ns’ye yuvarlıyor; `ioapic.c` edge yolunda her yüksek bildirimi kabul ediyor (https://raw.githubusercontent.com/qemu/qemu/v8.2.2/hw/timer/i8254_common.c ve https://raw.githubusercontent.com/qemu/qemu/v8.2.2/hw/intc/ioapic.c). Kalibrasyonda kanal0 mode0 tek atım, IRQ’da 11932 sayımla yeniden kurma seçildi. PVH mode3 değişmedi. İlk ölçüm 10 IRQ/6,551,951 LAPIC sayımı, periyot 655195; 8 sn’de +767 LAPIC, +0 PIT; test beklentisi değiştirilmedi. Port0x80 döngüsü QEMU’da mikrosaniye ölçmez; yalnız sınırlı bekleme olarak kullanılır.
- VARSAYIM: gerçekzamanlı ISR içindeki opak port asm’i için statik çevrim bütçesi port başına 1024 (PIT iki OUT: 2048) seçildi; bu fiziksel donanım WCET kanıtı değildir. Self-host RT007 denetimi bu açıklamayı zorunlu kıldı; bootstrap `--llvm` tek başına RT007 uygulamıyor.
- C son durum: LAPIC bölücüsü16, periyodik vektör48, sahte vektör255; PIC0/1 imr=ff. IRQ0 GSI2 yalnız kalibrasyonda açık, sonra maskeli. IRQ1 GSI1/vektör33 ve IRQ12 GSI12/vektör44 D kurulana kadar maskeli. QMP `info lapic`/`info pic` kayıtları ve bağımsız sayfa yürüyüşü kapıda denetleniyor. PVH PIC/preemption yolu korunuyor.

## C11-001 — ham işaretçiye sabit atamada dar store

C ve self-host aşağıdaki programda `store volatile i32` üretir; beklenen tam64 yazımdır. Yerel clang koşumunda ikisi de rc=1 (doğru sonuç rc=0). Derleyici değiştirilmedi; halka başlıklarında `0 olarak tam64` kullanıldı.

```kem
çıplak işlev main() -> tam32 {
 değişken n: tam64 = 72623859790382856;
 değişken p: *tam64 = (&n) olarak *tam64;
 *p=0;
 eğer n != 0 { ver 1; }
 ver 0;
}
```

Tekrar: `python3 tools/codex/c11_001.py` iki derleyicide asıl/dolan biçimlerini derler, yerelde koşar ve store genişliğini gösterir. Başlangıç değeri Python `int('0102030405060708',16)=72623859790382856`; dar yazımdan kalan üst yarı 72623859706101760.

## SAHTE MODEL DÜZELTİLDİ

- Yeni kâhinin ilk `echo imlec` gönderiminde `i` qcode’u Türkçe düzende `ı` üretti. Beklenen `imlec` korunup gönderici mevcut Türkçe eşleme gibi `apostrophe` kullandı; ad QEMU `query-qmp-schema` sözlüğünden doğrulandı. Üretim klavye eşlemesi değişmedi.

- D ilk gerçek koşum: IRQ klavye yedi standart komutu, 8 saniye heap-sabit/HLT bekleyişini geçti. Tek QMP çağrısındaki 108 key olayı komutu tamamlayamadı. QEMU 8.2.2 `hw/input/ps2.c` aygıt kuyruğunu 16 baytla sınırlar (https://raw.githubusercontent.com/qemu/qemu/v8.2.2/hw/input/ps2.c). Yeni kâhinin gönderimi aynı 54 tuşu ardışık QMP çağrılarıyla 3 ms/tuş olarak verir; beklenen metin, 12×48 hızlı tuş ve 1296 baytlık halka sarımı korunur. Bu sürücü halkasında veri kaybını kabul etmek değildir; aygıtın giriş kuyruğunu tek host çağrısında aşmayan fiziksel giriş tarifidir. Eski kâhinlerin beklentileri değiştirilmedi.

- C11-001 dolanması: `*p=0 olarak tam64` ile C ve self yerel tekrar üretimleri rc=0; gerçek x86 IR’de üç halka başlığı da `store volatile i64`. ISR’de veri baytı `store volatile i8`, baş/tail/kayıp sayaçları hizalı `volatile i64`; x86 TSO yayımlama sırası korunuyor.
- D ilk tam gerçek ölçüm: PS/2 fare kimliği0, 3 bayt paket. Hareketler (17,-11,sol), (-9,4,boş), (4,-3,sağ), (-5,2,orta), (0,0,boş); araya AUX D3/00 kayık baytı verildi. IRQ1=1540, IRQ12=16, halka kaybı0, parite/zaman aşımı0. 12×48 hızlı tuş + 1296 bayt halka sarımı, 8 sn heap-sabit HLT ve 1.024.000 piksel save-under karşılaştırması yeşil. Saf fare çözümleyicisi gerçek aygıtla uyuştu; değişiklik gerekmedi.

- D tekrar: 20/20 normal açılışta IRQ1/12, hızlı tuşlar, 8 sn boşta bekleme ve save-under yeşil. Sabotaj: IRQ12 maskeli 10/10 olay yok; senkron guard kapalı 10/10, kayık AUX baytından sonra beklenen dx=-9/dy=4 yerine dx=24/dy=247. İki kaynak da geri alındı.

- D karma ilk gerçek ölçüm: USB klavye beklerken PS/2 fare IRQ12 ile işlendi; ardından USB klavye komutu ve aynı xHCI denetleyicisindeki FAT32 okuması geçti. IRQ1=0/IRQ12=16, kayıp0; tüm GOP pikselleri ve disk SHA256 aynı. Bu şekil bekleyen USB isteğinin korunmasını ayrıca ölçer.

- Sınırlar: tek işlemcili xAPIC yolu ölçüldü; firmware x2APIC açık bırakırsa açık hata döner. PS/2 fare biçimi kimlik0/üç bayttır; tekerlek, SMP ve MSI/MSI-X bu dalganın dışında. Donanım sonuçları QEMU q35/OVMF ölçümleridir.

- D karma tekrar: C10/10+self10/10 yeşil. USB bekleyişinin fareye yol vermesi kaldırılınca 10/10 gerçek açılış kırmızı (ilk FARE satırı gelmiyor). Geri alınan kaynakla hem karma hem PS/2 klavye biçimi yeniden yeşil. D regresyonu: check351/369, 18 sınıflandırılmış, 0 kırmızı; baremetal_diff23/23, 0 atlama; x86 noredzone480/480.
