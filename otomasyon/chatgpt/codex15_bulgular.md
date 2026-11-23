# Dalga 15 ölçümleri

## Donanım beklemeleri, bölüm 2 anındaki satırlar

| Dosya ve satır | Beklenen durum | Sınır |
|---|---|---|
| `runtime/mimari/x86_64/kem_uart.kem:26` | 16550 LSR.THRE | 100.000 okuma, ardından tek çıkış |
| `runtime/mimari/x86_64/kem_ps2_hw.kem:5` | 8042 IBF boş | 10.000 × 50 gecikme adımı |
| `runtime/mimari/x86_64/kem_ps2_hw.kem:22` | 8042 OBF, klavye cevabı ve yoklamalı girdi dilimi | 10.000 × 50, ardından -1 |
| `runtime/mimari/x86_64/kem_ps2_hw.kem:36` | 8042 başlangıç FIFO drenajı | 256 bayt |
| `runtime/mimari/x86_64/kem_ps2_hw.kem:117` | AUX cevabında OBF ve kaynak biti | 10.000 × 50, ardından -1 |
| `runtime/mimari/x86_64/kem_rtc_hw.kem:14` | RTC UIP temiz ve iki örnek aynı | 2.000 × 10 |
| `runtime/mimari/x86_64/kem_apic_hw.kem:116` | PIT sayacı ve LAPIC kalibrasyonu | 2 milyon × 50 |
| `runtime/mimari/x86_64/kem_guc_hw.kem:49` | 8042 yeniden başlatma komutu için IBF | 10.000 × 50, ardından sıradaki reset yöntemi |
| `runtime/mimari/x86_64/kem_pci_hw.kem:77` | Port 0x80 gecikmesi | Çağıranın verdiği adım sayısı |
| `drivers/nvme/cekirdek.kem:79` | CSTS.RDY ve CQ fazı | CAP.TO, başlangıç değeri 500 |
| `drivers/ahci/cekirdek.kem:22` | HBA, port, görev dosyası ve PxCI | Faz başına 1.000 adım |
| `drivers/usb/xhci_cekirdek.kem:37` | xHCI durum biti ve port reset | 4.000 adım |
| `drivers/usb/xhci_cekirdek.kem:219` | xHCI olay halkasının döngü biti | 4.000 adım |
| `drivers/usb/depo.kem:92` | BOT aktarımı, kontrol ve toplu uç | Faz başına 1.000 adım |
| `test/ornekler/kem_os_x86_blok_ortak.kem:224` | AHCI portlarından birinde PxSSTS.DET=3 | 12 tarama × 32 port |
| `test/ornekler/kem_os_x86_blok_ortak.kem:107` | xHCI olay halkası boşaltma | Tek turda halka kapasitesi |
| `test/ornekler/kem_os_x86_blok_ortak.kem:151` | Blok durum makinesi | 2 milyon adım |
| `test/ornekler/kem_os_x86_blok_ortak.kem:218` | USB yapılandırma ve devir | 100.000 adım |
| `test/ornekler/kem_os_x86_kabuk.kem:62` | Başlangıç xHCI kontrol aktarımı | 100.000 adım |
| `runtime/kem_virtio_blk.kem:157` | VirtIO blok used.idx | 100 milyon tur |
| `runtime/kem_virtio_net.kem:132` | VirtIO TX used.idx | 100 milyon tur |
| `runtime/kem_virtio_net.kem:144` | VirtIO RX used.idx | Çağıranın verdiği tik sayısı |
| `test/ornekler/kem_os_x86_nvme.kem:18` | NVMe QEMU sürücü örneği | 2 milyon adım |
| `test/ornekler/kem_os_x86_ahci.kem:18` | AHCI QEMU sürücü örneği | 2 milyon adım |
| `test/ornekler/kem_os_x86_usb.kem:22` | xHCI ve USB depo QEMU sürücü örneği | İki yürütücü de 100.000 adım |
| `runtime/kdl_runtime_uart_16550.c:156` | C 16550 TX/RX | LSR.THRE ve DR beklemelerinde sınır yok. Dalganın izin vermediği C çalışma zamanı |
| `runtime/kdl_runtime_uart_pl011.c:153` | ARM C PL011 TX/RX | FR.TXFF ve RXFE beklemelerinde sınır yok. ARM çalışma zamanı bu dalganın dışında |

Sürücü durum makineleri çağrı başına bir adım yürütür. Tablo hem bu tekrarları hem onları süren dış sınırları gösterir. Klavye girdisi, çocuğun çıkışı ve boşta HLT bir kullanıcı olayını bekler. Panik ve kapanış sonrası HLT son durumlardır. Eski PVH kabuğu girdi gelene kadar sonlu PS/2 yoklama dilimlerini heap ayırmadan yineliyor.

QEMU’nun NVMe denetleyicisi komutlara yanıt verir. Namespace bağlamadan çalıştırılan gerçek aygıt, kullanılamayan NVMe hedefini temsil etti. Kabuk çalışan USB’yi bağlayıp komut kabul etti. Sökme deneyi gerçek `device_del` kullandı. PORTSC bağlantı ve değişim bitleri, önbellekte duran sektörlerin sökülmüş ortama ait olduklarını da saptadı. xHCI port olayını halkadan alan kod USB klavyenin sıradaki tamamlamasını işleyebildi. Boş SATA denetleyicisindeki süre sınırı kaldırıldığında kâhin istemi beklerken süreyi doldurdu.
