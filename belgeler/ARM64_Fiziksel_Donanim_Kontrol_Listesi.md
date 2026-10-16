# ARM64 Fiziksel Donanım Kontrol Listesi

> **[D-530]** Bu belge D-490'ın açık bıraktığı tek şeyi kapatır: *"gerçek
> doğrulama yalnız fiziksel ARM64 donanımında yapılabilir"* denmişti ama
> **nasıl** yapılacağı hiçbir yerde yazılı değildi. Kaynak dosyanın başlığında
> bir uyarı vardı; **çalıştırılabilir adım yoktu.**

## Neden bu belge var

D-490'da **ölçüldü** (tahmin değil): `test/bare_metal/smp_queue_arm.c` içindeki
`dc civac` + `dc ivac` + eşlik eden `dsb sy` komutlarının **dördü de `nop`a
çevrildi** ve test **birebir aynı sonuçla GEÇTİ** (`toplam=20540`).

Bundan üç sonuç çıkar ve üçü de bu belgenin gerekçesidir:

1. O bariyerler **doğru ve gereklidir** — gerçek donanımda onlarsız bu kod
   bozulur — ama **hiçbir kapı bunu zorlamıyor.** Sessizce silinseler QEMU
   kapısı yeşil kalır.
2. QEMU üzerine kurulacak bir "zayıf bellek" kapısı **hiçbir şey kanıtlamaz**
   → böyle bir kapı bilerek **eklenmedi** (D-425: yanlışın gözlenebilir olduğu
   şekli ölçemeyen kapı, kapı değildir).
3. Doğrulama **ertelenmiştir, iptal edilmemiştir.** Bu liste onu ertelenmiş
   bir borç olmaktan çıkarıp **koşulabilir bir prosedüre** çevirir.

## Ön koşullar

| Gereksinim | Doğrulama |
|---|---|
| Fiziksel ARM64 (DGX Spark vb.), **≥2 çekirdek** | `nproc` ≥ 2 |
| `clang` (aarch64 hedefi) | `clang -target aarch64-unknown-none --version` |
| `ld.lld` | `ld.lld --version` |
| Depo kurulu, host takımı yeşil | `make test_tumu` → `rc=0` |

⚠ **QEMU'nun varlığı bir ön koşul DEĞİLDİR** — bu prosedürün amacı tam olarak
QEMU'nun ölçemediğini ölçmektir.

> **[2026-09-28] Yazılım ön koşulları DGX Spark'ta SAĞLANDI (ölçüldü).**
> `make test_tumu` → `rc=0`, 70 kapı, 0 kırmızı, eksik araçtan kaynaklanan
> atlama yok. Bu satır bir iddia değil: ilk yerli koşumda 9 kapı kırmızıydı
> (test tarafı arch-tag varsayımları + self-host'un sabit x86_64 üçlüsü);
> ikisi de onarıldı, ayrıntısı `CLAUDE.md` "Geliştirme Ortamı (DGX Spark)".
>
> **Buna rağmen Adım 1 BU MAKİNEDE KOŞULAMADI** — sebebi aşağıda, "Adım 1"in
> altındaki ölçüm notunda. Engel yazılım ön koşullarında değil, imajın
> QEMU'ya bağlı olmasında ve fiziksel konsol erişiminde.

## Adım 1 — Taban: test gerçek donanımda GEÇİYOR mu?

```bash
make calistir_smp_queue_test_arm
```

**Beklenen:** `SMP QUEUE OK`, `toplam=20540`, her iki çekirdeğin sayacı `> 0`.

> **[2026-09-28 ÖLÇÜLDÜ — DGX Spark] BU ADIM BU MAKİNEDE KOŞULAMADI.**
>
> Yukarıdaki `make` hedefi Spark'ta **rc=0** verdi ve seri çıktıda
> `toplam=20540`, `SMP QUEUE OK`, her iki çekirdek `islenen=20` göründü —
> **ama bu QEMU sonucudur, D-490'ın zaten bildiği taban.** Fiziksel ölçüm
> DEĞİLDİR.
>
> Aşağıdaki *"fiziksel donanımda ELF'i doğrudan çalıştırın"* talimatı
> **izlenemiyor** ve sebebi ölçüldü, tahmin edilmedi:
> - ELF `-ffreestanding -nostdlib`, `ENTRY(_start)` — Linux kullanıcı
>   alanında süreç olarak çalışamaz. "Doğrudan çalıştırmak" diye bir şey yok;
>   imajı **boot etmek** gerekir.
> - Giriş adresi `0x40000000` — QEMU `virt` makinesinin RAM tabanı
>   (`linker/bare-metal-aarch64.ld:33`).
> - UART `0x09000000`, kaynaktaki yorumu aynen: `/* QEMU virt UART0 */`
>   (`runtime/kdl_runtime_uart_pl011.c:39`).
>
> Yani imaj *"ARM64 için"* değil, **QEMU `virt` için** kuruludur. D-490'ın
> gerekçesi *"QEMU zayıf bellek sıralamasını modellemez"* olduğuna göre,
> QEMU'nun bellek haritasına çakılı bir imajla fiziksel doğrulama **tanım
> gereği** yapılamaz. Bu, belgenin yazıldığında fark edilmemiş bir
> bağımlılığıdır.
>
> **Yazılım tarafı aslında hazır (ölçüldü) — eksik olan fiziksel kurulum:**
> - Yükleme tabanı zaten parametre: `DEFINED(__kem_yukleme_tabani)`
>   (`linker/bare-metal-aarch64.ld:33`; Makefile `kem_os`ta `--defsym` ile
>   kullanıyor).
> - UART tabanı zaten parametre: `-DKDL_PL011_BASE`, `-DKDL_16550_BASE`.
> - Spark'ın konsolu PL011 değil **16550 sınıfı** (`/proc/cmdline`:
>   `console=ttyS0,921600`) ve deponun 16550 sürücüsü VAR, kapısı yeşil
>   (`calistir_uart_16550_test`). `kexec` de kurulu.
>
> **Kalan gerçek engeller fizikseldir:**
> 1. Spark'ın UART taban adresi bilinmiyor (DT/ACPI'den okunmalı).
> 2. Seri çıktıyı **okumak için karşı uçta ikinci bir makine** gerekir.
> 3. Bare-metal imaja `kexec` Linux'u düşürür → geri dönüş power-cycle.
>    Bu makine birincil geliştirme ortamıdır; konsol elde değilken denenmez.
>
> Sonuç: D-490 **kapatılmadı ve daraltılmadı**; "ertelenmiş borç" olmaktan
> çıkıp **"tek eksiği fiziksel kurulum"** hâline geldi.

⚠ Bu hedef QEMU çağırır. Fiziksel donanımda **ELF'i doğrudan çalıştırmak**
gerekir; QEMU dalı atlanmalıdır. ELF üretimi hedefin ilk üç satırıdır:

```bash
clang -target aarch64-unknown-none -ffreestanding -nostdlib \
      -Wall -Wextra -Wpedantic -std=c11 -O2 -DKEMGU_BARE_METAL -Iruntime \
      -ffunction-sections -fdata-sections \
      -c test/bare_metal/smp_queue_arm.c -o build/smp_queue_arm.o
ld.lld -m aarch64linux -T linker/bare-metal-aarch64.ld \
       -o build/smp_queue_arm.elf build/smp_queue_arm.o $(BM_A64_OBJS)
```

**Taban geçmiyorsa DURUN.** Adım 2 anlamsızdır: neyin bozulduğunu ölçemezsiniz.

## Adım 2 — SABOTAJ: bariyerleri `nop` yapın

`test/bare_metal/smp_queue_arm.c` içinde **dört satır** vardır (D-490'da
ölçülen konumlar; satır numaraları kayabilir, deseni arayın):

```
dc civac, %0      (yazma sonrası boşaltma)
dsb sy            (aynı bloktaki bariyer)
dc ivac, %0       (okuma öncesi geçersiz kılma)
dsb sy            (aynı bloktaki bariyer)
```

Dördünü de `nop\n` ile değiştirin. **Yamanın indiğini SAYIN** — bu depoda
sabotajın sessizce uygulanmaması defalarca yaşandı (D-402, D-490):

```bash
# ⚠ SAYIM KOD SATIRLARINDA yapılmalı: düz `grep -c "dc civac"` YORUMLARI DA
#   sayar (bu dosyada 9 eşleşme çıkar, oysa kodda 2 tane var). Tırnak öneki
#   yalnız satıriçi-asm dizgilerini yakalar.
grep -cE '"dc (civac|ivac)' test/bare_metal/smp_queue_arm.c   # 2 -> 0 OLMALI
grep -cE '"dsb sy'          test/bare_metal/smp_queue_arm.c   # 4 -> 2 OLMALI
```

⚠ `dsb sy` **4 kez** geçer; ikisi önbellek bakımına eşlik eder, ikisi
spinlock yolundadır. Sabotaj yalnız **önbellek bakımına eşlik edenleri**
(satır ~131 ve ~142 civarı) hedefler; spinlock bariyerlerini kaldırmak
**farklı bir şeyi** ölçer ve sonucu yorumlanamaz hale getirir.

Yeniden derleyip çalıştırın.

## Adım 3 — Sonucu okuyun

| Gözlem | Anlamı | Eylem |
|---|---|---|
| **KIRMIZI** (toplam ≠ 20540, ya da bir çekirdek 0 iş çekti, ya da asıldı) | **BEKLENEN.** Bariyerler gerçekten gerekliymiş; QEMU'nun göremediği şey budur. | Sabotajı geri al, D-490'ı *"fiziksel donanımda doğrulandı"* diye kapat. |
| **YEŞİL** (yine `toplam=20540`) | **Test yeterince zorlamıyor.** Donanım coherency'si burada devreye giriyor olabilir ya da yarış penceresi çok dar. | Aşağıya bak — testi güçlendir, sonucu *"bariyerler gereksiz"* diye YORUMLAMA. |

⚠⚠ **YEŞİL SONUÇ "BARİYERLER GEREKSİZ" DEMEK DEĞİLDİR.** Bu, tam olarak
QEMU'da yaşanan durumdur ve orada yanıltıcı olduğu ölçülmüştür. Yeşilse test
zorlamayı artırmalı: öğe sayısını (`N_IS`, bugün **40**) büyütün, iş başına hesabı
küçültün (yarış penceresini genişletir), koşumu 100 kez tekrarlayın.
**Aralıklı bir kırmızı bile kesin kanıttır** — zayıf bellek hataları
belirlenimci değildir.

## Adım 4 — Kaydı güncelleyin

Sonuç ne olursa olsun:

1. `test/bare_metal/smp_queue_arm.c` başlığındaki D-490 uyarısını **ölçülen
   gerçekle** değiştirin (uyarı bugün *"QEMU'da ölçülmüyor"* diyor; fiziksel
   ölçümden sonra artık ne bilindiğini yazın).
2. `CLAUDE.md`'de D-490'ı kapatın ya da daraltın.
3. Sabotajın **kaynakta kalmadığını** doğrulayın:
   `grep -cE '"dc (civac|ivac)' test/bare_metal/smp_queue_arm.c` → **2**,
   `grep -cE '"dsb sy' ...` → **4** olmalı.

## Aynı turda koşulması gereken diğer hedefler

Fiziksel ARM64'e ilk taşımada, D-469'un uyardığı şey geçerlidir: *"derleyici
taşınır, kapılar taşınmaz."* Bu üçü sırayla koşulmalı:

```bash
make calistir_qemu_cekirdek      # 5 temsilci (QEMU varsa)
make calistir_baremetal_diff     # ARM64 yapı paritesi, 5/5 birim
make calistir_arm64_test
```

⚠ **D-469'un dürüst sınırı hâlâ geçerli:** Windows/WSL'de yapılan hazırlık
*"doğru yolu arıyor"* ve *"doğru üçlü seçilecek"* demektir; **ARM64'te
gerçekten çalıştığı ancak orada kanıtlanır.** Orada farklı çıkan hiçbir şey
*"platform farkı"* diye geçiştirilmemelidir — bu depoda parite sapmalarının
sessiz kalma eğilimi defalarca ölçülmüştür.

## Kapsam dışı (bilinçli)

- **`görev`/`kanal` bare-metal koşumu:** D-490'da ölçüldü ki `kem_os` QEMU'da
  **`-smp` olmadan** koşar (tek çekirdek) → orada DRF hakkında hiçbir şey
  kanıtlanamaz. ABI paritesi ayrıca kapılıdır (D-527, `baremetal_diff`), ama
  **koşum** fiziksel donanımın işidir.
- **Zayıf-bellek kapısı:** eklenmedi ve eklenmemeli (yukarıdaki 2. sonuç).

- **Linux KULLANICI ALANINDA "aynı testin" koşulması: REDDEDİLDİ (2026-09-28).**
  Adım 1 fiziksel donanımda koşmayınca akla gelen ilk kısayol budur —
  "madem imaj QEMU'ya bağlı, aynı mantığı iki `pthread` ile Spark'ın gerçek
  çekirdeklerinde ölçelim". **Yapmayın.** Gerekçe, testin kendi başlığında
  yazılı (`test/bare_metal/smp_queue_arm.c`, satır 28-33): çekirdek 1 PSCI
  `CPU_ON` ile **MMU KAPALI** başlar (non-cacheable, doğrudan RAM), çekirdek 0
  ise **MMU AÇIK**tır (Normal-WB cacheable). `dc civac`/`dc ivac` tam olarak bu
  **cacheability uyuşmazlığı** için vardır.

  Linux kullanıcı alanında iki iş parçacığı da MMU açık, Normal-WB ve aynı
  inner-shareable alandadır → **donanım coherency'si devreye girer** ve o `dc`
  komutları GERÇEKTEN gereksiz olur. Sabotaj neredeyse kesin YEŞİL çıkar; o
  yeşil D-490'ın sorusunu yanıtlamaz ama yanıtlamış gibi durur. Adım 3'teki
  uyarının (*"YEŞİL SONUÇ 'BARİYERLER GEREKSİZ' DEMEK DEĞİLDİR"*) tam olarak
  öngördüğü tuzak budur — üstelik bu sefer testin zayıf olmasından değil,
  **ölçülen koşulun hiç kurulamamasından**.

  Uyuşmazlığı Linux'ta taklit etmek non-cacheable eşleme ister → çekirdek
  modülü → yukarıdaki "gerçek boot" maliyetine çıkarsınız, onun sadakati
  olmadan. Bu madde buraya, aynı kısayol tekrar önerilmesin diye yazıldı.
