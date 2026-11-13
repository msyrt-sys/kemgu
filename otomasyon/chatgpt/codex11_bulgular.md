# Dalga 11 bulguları ve tasarım seçimleri

- IST1 #DF, IST2 NMI, IST3 #MC için ayrı 16 KiB yığın; alt sınır kanaryası. PVH/UEFI girişleri ve ring3/kurtarılabilir #PF yolları korunacak.
- PVH zamanlayıcı PIC olarak korunacak; UEFI yolu MADT/IOAPIC/LAPIC kullanacak.

- QEMU ölçümü: kanonik olmayan `0x8000000000000000` adresini okumak #GP(0) üretirken CR2’yi de bu adrese değiştirdi. Mimari #GP için CR2 tanımlamadığından fikstür bunun yerine GDT dışı seçici `7*8=0x38` yükler; beklenen hata kodu 0x38. Beklenti üretim çıktısına uydurulmadı, ayırt edici mimari ihlal seçildi.

- Taban gerçek QEMU/OVMF RAM dökümü: MADT 120 bayt, LAPIC 0xfee00000; IOAPIC kimlik 0, adres 0xfec00000, GSI tabanı 0. ISO: IRQ0→GSI2 bayrak 0; IRQ5/9/10/11→aynı GSI, bayrak 0x0d (yüksek etkin, seviye). IRQ1/12 için ISO yok: GSI1/GSI12. LAPIC NMI tüm işlemciler, LINT1.
- A/B: #DF CR2=0x60000ff8 (eşlenmemiş 0x60001000 RSP’ye ilk 8 baytlık itme), vektör8/hata0; QMP RSP IST1 aralığında. Her üç 16 KiB IST’nin alt kanaryası 0x4b454d4755495354; panik girişinde üretim denetimi ve QMP ham bellek karşılaştırması var. Koruma sayfası yerine kanarya seçildi; taşmayı erişim anında engellemez, panik girişinde ve kâhinde bildirir.
- E4n: etkin heap CAT listeleri 16 kez tarandı (Makefile 2, harness/sınıflandırma 14); ARM64 UART parçası sessiz kancayı, x86 MMU parçası ekran kancasını taşır. İstisna mesajı .bss 256 bayt; 255 bayt + NUL sınırı; iç içe giriş tahsis yapmadan seri + HLT. Eski ISTISNA seri satırı aynen, ardından ekranla aynı yeni satır.
