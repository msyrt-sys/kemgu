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
- İlk 50 tekrar ölçümü. C süreç heap'i +0 ve kabuk heap'i 4.657.328 → 16.003.360 bayt. Self-host süreç heap'i +0 ve kabuk heap'i 4.854.176 → 16.069.040 bayt. Önceki uygulamada her çıkıştan sonra 1.032 çerçevenin tamamı sıfırdı; aşağıdaki güncel tahsis ölçümü bunun yerini alır. Kabuk heap'inin büyümesi E4k kapsamında kalır.
- C2, fare çağrısı, arka plan yönetimi ve spawn önerilen iş sırasına aittir. Bu dalga yalnız C1 işlevsel kapsamını tamamlar.


## Güncel işlevsel kapsam

- Önceki oturumdan kalan hata yalıtımı programları (`hata_*`, `sinir`) bu dalganın tesliminden çıkarıldı. Başlangıçtaki commit edilmemiş değişiklikler `/tmp/kemgu-codex13-once-uwn28jit` altında korundu. Bu testlerin geliştirilmesi H9j/E3f kapsamında Claude'a aittir.
- Altı program: `merhaba`, `kedi`, `say`, `yazdene`, `tus`, `bolme`. `bolme` normal KEMGU bölme işleminin D-502 denetimiyle `PANIK: sifira bolme` yazar ve 134 ile çıkar; kabuk ardından `echo yasiyor` komutunu çalıştırır.
- VARSAYIM: süreç tablosunda bir canlı süreç; iki fiziksel yuva sırayla kullanılır. UEFI ayırıcısından alınmış 1.032 sayfalık havuz çekirdek ömrünce ayrılmıştır. Süreç çıkışı yuva bitini temizler ve dört sayfa tablosunu kaldırır. Kullanıcı belleği sonraki tahsis sırasında tamamen sıfırlanır. Bu, genel ayırıcıya iade değil, süreç havuzuna geri vermedir.
- QMP kâhini boş çerçeve sayısını artık sıfır bayt sayısından türetmez: gerçek tahsis bitlerini ve kaldırılmış sayfa tablolarını kontrol eder. `merhaba` BSS nöbetçisini her girişte sıfır bekler, ardından değiştirir; 50 tekrar aynı çerçevelerin yeniden kullanılmasını ölçer.
- `boot/start_x86_64.S` değişikliği yalnız `kdl_syscall_stub` içindedir. Sürece giriş koçanı `runtime/mimari/x86_64/kem_surec_hw.kem` içindeki satıriçi asm'dedir. Mevcut ring3, IST ve istisna dönüş yolları değişmez.
- Yeni parça dosyalar muafiyet listesine eklenmedi. `tools/codex/surec_check.py` gerçek harness kaynak listelerinden birleşik birimi oluşturup `--check` çalıştırır.
- Eski satır okuma çağrısında sıfır/negatif kapasite, UART yardımcısına ulaşmadan reddedilir.
- Çıkış eşitleme kapısı süreç döner dönmez VM'yi durdurup diski `fsck.fat -n` ile denetler. Böylece kabuğun kapanış `sync` işlemi eksik süreç temizliğini gizleyemez. Disk SHA256 denetimi seri kâhini başarısız olduğunda da çalışır. Yazma sonrası tüm program ELF'lerinin ve önceki dosyaların içeriği bağımsız FAT okuyucusuyla karşılaştırılır.
- Sabotaj aracı `python3 tools/codex/surec_sabotaj.py`: her kusur ayrı kaynak kopyasında, taze ELF'lerle ve taze diskle çalışır. ELF sembolü ve ABI numarası denetimleri QEMU öncesi bağlantı/belge kapılarıdır; bunlara QEMU açılışı sonucu yazılmaz.

## Taban ve kapsam kontrolü

Dal `b1b9470` D-760 tabanından devam etti. Görev gereği main birleştirilmedi. `origin/main` sonradan yalnız görev metnini ve LOOP kaydını güncelledi; doğrudan `git diff origin/main` bu iki dosyanın taban farkını da gösterir. Bu dalın kendi PR farkı `git diff origin/main...HEAD` ile ölçülür. `LOOP.md` ve `CODEX_GOREV_13.md` bu çalışmada değiştirilmedi.

## Son işlevsel ölçümler

- C ve self-host ayrı derlemede aynı kullanıcı ELF boyutlarını verdi: `merhaba` 13.200, `kedi` 8.992, `say` 13.200, `yazdene` 13.232, `tus` 13.240, `bolme` 13.232 bayt. Bütün ELF'lerin çözümlenmemiş sembol listesi boş; kendi çalışma zamanı dışında çekirdek sembolü yok.
- PVH kanıt birimi 6.242 → 6.637 satır; self-host `--llvm` taban 8,84 → son 9,62 sn (8,45 sn kullanıcı / 1,16 sn sistem).
- Birleşik UEFI kabuğu 5.358 → 6.360 satır. Self-host `--llvm`: taban 23,54 sn; son ölçüm 27,58 sn duvar / 24,54 sn kullanıcı / 3,00 sn sistem. Son ölçüm yerel QEMU testleri sürerken alındı. Kullanıcı programları birleşik çekirdeğe eklenmedi.
- İlk `merhaba` yüklemesi (ELF okuma/ayrıştırma ve kabuk komutu dahil) C ile +1.115.168, self-host ile +1.180.752 bayt çekirdek heap tahsis etti. Önbellekteki aynı ELF yeniden kullanıldığında süreç kurma/temizleme ölçümü +0 bayttır; kabuk komut ayrıştırma büyümesi ayrı toplamda izlenir.
- `SUREC_TEKRAR=1 SUREC_SENARYO='bolme yazma tekrar' make calistir_bm_x86_surec` iki derleyicide geçti. C: kabuk heap 4.657.328 → 16.003.360; self-host 4.788.592 → 16.003.456 bayt. Süreç kurma/temizleme bölümünde iki derleyicide de +0 bayt ve 50 tekrar sonunda 1.032 kullanılabilir havuz çerçevesi.

## Güncel sabotaj sonuçları

Her satır bağımsız kaynak kopyası ve taze imaj kullanır. Gerçek QEMU kusurları C derleyicisinde 1/1 kırmızı; olumlu matris iki derleyicide çalışır.

| Kusur | Ölçülen kırmızı |
| --- | --- |
| Argc sıfır yazıldı | `merhaba` beklenen 3 yerine 0 ile çıktı |
| ELF/BSS bölgesinin tahsis sıfırlaması atlandı | Yeniden kullanılan yuvada `merhaba` BSS nöbetçisi 124 ile çıktı |
| fd sınırında 16 geçirildi | Çekirdek dizi sınır ihlali; seri kâhini kırmızı |
| Salt okunur yazma izni atlandı | Gerçek NVMe test diski SHA256 değişti |
| Yuva iadesi atlandı | 50 tekrar kâhini ikinci süreçte boş çerçeve sayısını yanlış buldu |
| Kod sayfasına yazma izni verildi | Canlı QMP PTE yürüyüşü `0x54000000` iznini reddetti |
| Konumla sonucuna 1 eklendi | `yazdene` 12 ile çıktı |
| Süreç çıkışı eşitlemesi atlandı | Çıkış anındaki `fsck.fat -n`, kirli bayrak nedeniyle rc=1 |
| ELF'e `kdl_syscall_isle` sembolü eklendi | ELF kapısı QEMU öncesinde reddetti |
| Belgede tuş çağrısının 55 numarası 58 yapıldı | ABI kapısı QEMU öncesinde iki uyuşmazlığı bildirdi |

Sabotaj kayıtları: `build/surec-sabotaj-azbrg8mz` (ilk dört), `build/surec-sabotaj-qjuy8fcm` (çerçeve, izin, konum), `build/surec-sabotaj-eq00cr8r` (eşitleme, ABI, ELF). İlk denemelerde QMP soket yolunun uzunluğu ve raporlayıcıdaki kanıt metni düzeltilmiştir; bunlar ürün başarısı olarak sayılmamıştır.

## Regresyon ve atlama dökümü

- `make calistir_kem_os_x86 calistir_kem_os_arm`: rc=0; kayıt `build/codex13_cekirdek_son.log`.
- `calistir_check_kapisi`: 356/377 geçti, 21 mevcut muafiyet, 0 RED. İki yeni parça muaf tutulmadan birleşik kaynaklarıyla denetlendi.
- `calistir_baremetal_diff`: 23/23, 0 atlama. `calistir_sifir_uyari`: 38 dosya, 0 uyarı.
- Self-host bootstrap: 100.505 satır IR sabit noktası. Sürücü checker karşılaştırmaları iki derleme yolunda 232/232.
- ASan E2E: PASS=133, FAIL=0, SKIP=45, mevcut ALLOW=4, SIZINTI-MUAF=2. ASan/UBSan matrisi: 12/12.
- ChatGPT köprüsü: dil kartı + 141 test geçti.

| Mevcut atlama | Koşum sayısı | Neden |
| --- | ---: | --- |
| Codegen semantik karşılaştırmaları | 6 | Üç karşılaştırmada ikişer yabancı mimari girdisi |
| Geniş codegen | 8 | Host'ta bağlanamayan bare-metal MMIO sembolleri |
| Yapısal IR | 2 | Korpusun mevcut atlamaları |
| Sürücü paritesi | 1 | Korpusun mevcut atlaması |
| Sürücü koşum | 5 | C oracle `--check` reddi: virtio feature/mock/status ve virtqueue bind/layout |
| Üst bellek alt senaryosu | 1 | QEMU RAM ≤1 GiB |
| ASan E2E | 45 | Kapının mevcut SKIP sınıfı |
| Toplam atlama olayı | 68 | Tekil dosya sayısı değildir; tekrarlanan derleyici koşuları ayrı sayılır |

Yapısal IR kapısındaki 24, sürücü paritesindeki 1 ve tip kapısındaki 21 mevcut muafiyet ayrı kategorilerdir; yeni muafiyet eklenmedi. HASHCRACK çıktısındaki `7 atlandi`, bulunan adaydan sonraki arama adımlarıdır; test atlaması toplamına katılmadı.

Son kabul: `make test_tumu` rc=0 (`PIPESTATUS[0]`), `Tum testler gecti!`; 20/20 yeni PVH ABI ve 20/20 yeni OVMF süreç açılışı. Tam kayıt `build/codex13_test_tumu_son.log`. Tarih: 2026-11-21 (Europe/Istanbul).
