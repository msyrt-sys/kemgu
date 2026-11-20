# Dalga 13 bulguları

- Taban `b1b9470`, D-760. UEFI kabuk birleşik birimi 5358 satır, self-host `--llvm` 23,54 sn. Taban UEFI kabuk kâhini USB/256 MiB'de 3.935.760 bayt, AHCI/256 MiB'de 3.804.592 bayt heap üst sınırı ölçtü.
- VARSAYIM: ABI v2, eski RDI/RDX düzenine üçüncü argüman olarak RSI'yi ekler. Mevcut PVH ring0 bilinmeyen çağrı davranışı ve arm64 ABI'si korunur. SysV çağrısından önce yığın hizalanır ve DF temizlenir. `iretq` çağıranın bayraklarını yükler.

## C13-001

Self-host, negatif `tam64` sabiti doğrudan dönerken `ret i64` operandını i32 üretir. Küçük tekrar:

```kem
sabit HATA: tam64 = -38;
çıplak işlev sonuc() -> tam64 { ver HATA; }
```

Aynı kaynakta `build/kemgu --llvm --mimari x86_64-none C13-001.kem` ve `build/codegen --llvm --mimari x86_64-none C13-001.kem` çalıştırıldı. Her iki çıktıda beklenen sonuç, `clang -target x86_64-unknown-none -x ir -c` ile derlenmesiydi. C çıktı derlendi. Self çıktıda `sub i32 0, 38` ve `ret i64 %1` oluştu, clang tip uyuşmazlığı verdi. Pozitif sabit ve `ver 0 - HATA;` her iki derleyicide geçiyor.

## Kullanıcı ELF'i

- Mevcut `runtime/kem_heap.kem` değişikliksiz derleniyor. Kullanıcı `_start` linker'dan gelen `__heap_start` ve `__heap_end` ile `kem_heap_kur` çağırıyor. Mimari sınır ihlali kancası çıkış kodu 134 veriyor.
- İlk ELF her iki derleyicide 13.040 bayt. PT_LOAD yerleşimi R-X `0x54000000`, R `0x54001000`, RW BSS `0x54002000`. Bu programda ilk değerli RW veri yok. Böyle bir veri varsa linker betiği onu BSS'ten ayrı PT_LOAD'a yerleştirir.
- Kullanıcı biriminin ham IR'ında ortak heap `kdl_bolge_*`, `kdl_dizi_*`, `kdl_metin_*`, `malloc/free` ve `memcpy/memset` işlevlerini tanımlar. `merhaba` çıkışında LLD kullanılmayan işlevleri ayıkladı. Kullanıcı bağlantı girdisi yalnız kendi nesnesi, iki ELF'te de `llvm-nm -u` boş.

## C13-002

Self-host, küresel skalerin adresi ham işaretçiye dönüştürüldüğünde IR'da işaretçi operandını boş bırakıyor.

```kem
küresel değişken olcek: tam64 = 349;
çıplak işlev oku() -> tam64 {
    değişken p: *tam64 = (&olcek) olarak *tam64;
    ver *p;
}
```

`build/kemgu --llvm --mimari x86_64-none C13-002.kem` ve `build/codegen --llvm --mimari x86_64-none C13-002.kem` çıktıları `clang -target x86_64-unknown-none -x ir -c` ile denendi. C çıktı derlendi. Self `store ptr , ptr %0` üretti ve clang `expected value token` verdi. `merhaba` ilk değerli veri denetimi `.data` bölümünde kendi nöbetçisini tanımlıyor ve RIP-göreli erişiyor.

C1'in ilk denemesinde, QMP süreç durumu PID 1 ve çıkış 3 gösterirken self-host büyük sonuç yapısını çağıranda PID 1286144 ve çıkış 8 olarak gösterdi. Aynı yapı biçimiyle küçük hosted örnek iki derleyicide geçti. Sonuç alanlarını çağıranın ayırdığı yapıya yazan biçim gerçek QEMU'da iki derleyicide geçti.

## C1 ölçümleri ve seçimleri

- VARSAYIM: C1'de aynı anda bir kullanıcı süreci çalışır. PID 0 kabuğa ayrılmıştır. İki fiziksel yuva sırayla kullanılır. D-755 ayırıcısı UEFI haritasından toplam 1.032 sayfa aldı. Bir yuva 516 sayfa veya 2.113.536 bayt, dört tablo sayfası ve 512 kullanıcı sayfası içerir. QMP haritası tabanın 1.286.144 olduğunu ve her iki yuvanın firmware RAM'inde, korunan aralıklar dışında kaldığını gösterdi.
- VARSAYIM: Pencere 2 MiB. Kullanıcı heap'i 0x54100000–0x541ef000, koruma sayfası 0x541ef000, yığın 0x541f0000–0x54200000. Başlangıç RSP 0x541feff8. Argc ve 64 bit argv adresleri 0x541ff000'de. Kullanıcı FPU ve SIMD'ye erişemez. C1 PS/2 ve LAPIC olaylarında IRQ bekler, USB HID mevcut xHCI olay halkasını yoklar.
- VARSAYIM: Kabuk en son ELF'i yol, çalışma dizini ve ortak FAT32 içerik nesliyle saklar. Eski sürüm her tekrarda yaklaşık 852.640 bayt heap büyütüyordu ve 50 komutluk 28 MiB bütçesine sığmıyordu. Yazma planı veri yayımlamaya başladığında nesil değişir.
- İlk 50 tekrar ölçümü. C süreç heap'i +0 ve kabuk heap'i 4.657.328 → 16.003.360 bayt. Self-host süreç heap'i +0 ve kabuk heap'i 4.854.176 → 16.069.040 bayt. Her iki koşuda her çıkıştan sonra 1.032 çerçevenin tamamı sıfırdı. Kabuk heap'inin büyümesi E4k kapsamında kalır.
- C2, fare çağrısı, arka plan yönetimi ve spawn önerilen iş sırasına aittir. C1 çalışması senkron olduğu için `hata_dongu` senaryosu uygulanmadı.
