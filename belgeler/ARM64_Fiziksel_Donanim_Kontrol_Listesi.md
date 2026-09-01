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

## Adım 1 — Taban: test gerçek donanımda GEÇİYOR mu?

```bash
make calistir_smp_queue_test_arm
```

**Beklenen:** `SMP QUEUE OK`, `toplam=20540`, her iki çekirdeğin sayacı `> 0`.

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
