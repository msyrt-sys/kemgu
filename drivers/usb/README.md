# USB sans-IO çekirdek ve gerçek aygıt testleri

`xhci_cekirdek.kem` donanıma erişmez. `xhci_yeni(dma, bar_boy)` ile durum
oluşturulur; `xhci_adim(durum, önceki_yanıt)` bir `GcIstek` döndürür. Taşıyıcı
MMIO adreslerini BAR’a göre, bellek adreslerini fiziksel DMA adresi olarak yorumlar.
Her istek tamamlanıp yanıt verilmeden sonraki adıma geçilmez. `Hata` terminaldir;
`Bitti` enumerasyonun veya başlatılan aktarımın tamamlandığını gösterir.

DMA tabanı 64 KiB hizalıdır; taşıyıcı en az 8 MiB ayrılmış erişilebilir alan sağlamalıdır.
Kullanılabilir 4 KiB sayfa, en çok32 scratchpad, ilk16 slot desteklenir. Aygıt başına
tek IN ve tek OUT ucu vardır; hub, isochronous ve alt ayar değiştirme desteklenmez.
`aygitlar` çözülmüş aygıt/yapılandırma/uç tanımlayıcılarını içerir.

- `xhci_kontrol(durum, indeks, setup)`: boşta EP0 kontrol aktarımı; veri varsa
  yalnız IN ve en çok4096 bayt. Sonuç `durum.sonuc` içinde.
- `xhci_giris(durum, indeks, boy)`: interrupt IN; boy1..4096. Kısa paket CC13
  geçerlidir; yalnız alınan baytlar sonuç olarak döner. Aynı anda tek aktarım.
- `xhci_depo_hazir(durum, indeks)`: EP0 dequeue ve olay halkasını `depo.kem`
  için kurar. Bu **tek sahipli devirden sonra** aynı durumla kontrol/HID aktarımı
  başlatılmaz. BOT `UsbDepo` DMA tabanı seçilen aygıtın `dma` alanıdır; kapı zili
  `dboff+slot*4`, ERDP `rtsoff+56` kullanır.

`depo.kem`, ilk TUR için QEMU’nun power-on Unit Attention6/41/0 yanıtını bir kez
REQUEST SENSE sonrası yeniden dener. Diğer sense hataları ve tekrarlanan UA hata
kalır. Küçük aktarım halkalarında Link TRB önceki cycle bitini korur; olay halkası16
girdidir. HID klavye mevcut Türkçe Q dönüştürücüyü, fare mevcut yukarı-pozitif
`FareOlayi` biçimini kullanır.

`make calistir_qtest`: NVMe/AHCI’ye ek olarak `xhci`, `usb_depo`, `usb_kbd`,
`usb_fare` dalları. Her yeni dal iki derleyiciyi ayrı QEMU süreçlerinde çalıştırır,
toplam58 saniyelik çalışma sınırı uygular. HLT BIOS sayesinde firmware sürücüye
karışmaz; VM çalışır, HID girdileri QMP ile gönderilir. USB FAT32 çıktısı QEMU
kapandıktan sonra fsck.fat ve bağımsız Python okuyucuyla denetlenir.

`make calistir_chatgpt_kopru`: QEMU’dan alınan enumerasyon izleri, HID raporları,
BOT başlangıç/Link/olay davranışları ve önceki saf testler. Sahte modeller gerçek
QEMU’nun yerini tutmaz. İz yenilenirse önce gerçek qtest geçmeli; üretme komutları
`tests/drivers/usb/xhci_qemu_fikstur.kem` başındadır. Ayrıntılı ölçümler ve derleyici
dolanmaları `otomasyon/chatgpt/codex6_bulgular.md` içinde kayıtlıdır.
