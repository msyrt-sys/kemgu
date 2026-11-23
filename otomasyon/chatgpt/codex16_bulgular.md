# Dalga 16 ölçümleri

D-766 tabanında `runtime/mimari/x86_64/kem_uart.kem` içindeki `KGUN_BOY` 8192 bayttır. ABI çağrı listesi 70 ile biter.

## Klavye

QEMU 8.2.2 üzerinde PS/2 klavyenin 8042 öz-testi 85, birinci ve ikinci port testleri 0 döndürdü. ABD düzeninde fiziksel I ve Shift+2 tuşları kabukta `i@` üretti.

PS/2’de Caps, Num, Scroll, Caps, Scroll, Num basışları için QEMU `ps2_set_ledstate` izi sırasıyla 4, 6, 7, 3, 2, 0 verdi. USB klavyenin USBmon PCAP kaydındaki SET_REPORT veri baytları aynı girişte 2, 3, 7, 5, 1, 0 oldu. `q35,i8042=off` ile FADT boot bayrağı 0, kabuk kaydı “8042 yok” ve USB klavyeyle girilen `echo usb` çıktısı “usb” oldu.

Taşma ölçümünde QMP, PS/2 aygıtına 1.600 adet tek baytlık A basışı gönderdi. Her sekiz basışta IRQ1 sayacıyla aygıt FIFO’sunun boşalması beklendi. Konuk 1.022 basış, 0 bırakma ve 578 kayıp okudu. Halkadaki bir başka bayt, ölçümü başlatan Enter bırakmasıydı. Sonuç seri hattaki `PS2_KAYIP 578 0 0` ile eşleşti.

| Gerçekte uygulanan değişiklik | Gözlem | QEMU kâhini |
|---|---|---:|
| Pause dizisinin dördüncü baytını yut | 581 basışı kayboldu. Ayrı 29 ve 69 bırakmaları geldi. Host testi de 1 döndü. | 1 |
| PS/2 Caps ve Scroll maskelerini değiştir | Konuk 1, 3, 7, 6, 2, 0 gönderdi. | 1 |
| Ctrl+C ön plan süreci yerine kabuk işlevinden çıksın | Seri kayıtta `KABUK SONLANDI` görüldü. Yeni istem gelmedi. | 1 |
| Dolu halkada kayıp sayacını artırma | 1.600 basışın 1.022’si okundu, sayaç 0 kaldı. | 1 |

Bölüm kapılarını ayrı `make` süreçleriyle paralel başlatmak, `selfhost/konak.kem` hedefinin geçici dosyasında yarışa neden oldu. Derleyici yeniden üretildi, teslim kaynağındaki regresyonlar tek `make -k -j6 --output-sync=target` çağrısında başlatıldı.
