# VirtIO-blk Sektör Okuma (`blk_oku`) — D9

D9, virtqueue üzerinden bir sektör okuyup caller'ın 512-byte tamponuna yazan
`blk_oku`'yu ekler. C9 typed-width MMIO primitifleri (`mmio_oku/yaz16/32/64`)
üzerine kurulur; descriptor zincirini ring belleğe yazar, host-mock'ta
tamamlanmayı (used ring) simüle eder, cihaz status baytını `çeşit IOHatasi`'ya
map'ler.

Dosyalar: [drivers/virtio/virtio_blk_oku.kem](../../drivers/virtio/virtio_blk_oku.kem),
test [tests/drivers/virtio/virtio_blk_oku_test.kem](../../tests/drivers/virtio/virtio_blk_oku_test.kem),
harness [tests/drivers/Makefile.drivers](../../tests/drivers/Makefile.drivers).

## İmza

```kemgu
blk_oku(y: yetki<MMIO>, vq: Virtqueue, aygit: BlkAygit, sektor: tam64, tampon: Tampon512)
    -> sonuç<boş, IOHatasi>
```

- **`yetki<MMIO>`** — D1/D3/D6 ile aynı capability; ödünç alınır, fonksiyon
  sonunda tek `geri_al(y)`.
- **`Tampon512 { adres: tam64 }`** — caller'ın 512B tamponunun mock-fiziksel
  adresi. Data descriptor doğrudan `tampon.adres`'i gösterir (zero-copy DMA);
  cihaz oraya yazar, caller o adresten okur. "Veri kopyalama" = bu indireksiyon.

### Handle modeli: BY-VALUE struct (Mehmet onayı)

D1/D3/D6 bileşik veriyi `&Struct` ile geçirir (örn. `virtqueue_bagla(..., vq: &Virtqueue)`).
**blk_oku struct'ları BY-VALUE alır.** Sebep: `&Struct` referans-parametreli +
`sonuç<>` dönüşlü fonksiyon mevcut `src/llvm.c` codegen'inde host-mock'ta
**segfault** ediyor (latent bug; minimal repro `&Vq` param → exit 139, by-value
→ exit 0). Aynı bug `virtqueue_bagla`'yı, dolayısıyla `blk_baslat` init testini
de düşürüyor (`virtio_blk_init_test` origin/main'de segfault). KEMGU'da "bileşik
tipler otomatik referansla aktarılır" (CLAUDE.md) olduğundan, read-only input
(`Virtqueue`/`BlkAygit`) için by-value semantik **aynıdır, kopya yoktur**; çıktı
`Tampon512` adres-indireksiyonuyla yazılır. Codegen bug'ı C-track'te düzeltilince
`&`-imzasına **mekanik** geri dönülebilir. (Karar: by-value, şimdi unblock.)

## çeşit IOHatasi

Aile stiline (payload-less PascalCase: `BagHatasi`, `YapilandirmaHatasi`,
`BaslatHatasi`) uyumlu:

| Varyant | Kaynak | virtio karşılığı | Tetik |
|---------|--------|------------------|-------|
| `AygitHatasi` | cihaz status baytı | `VIRTIO_BLK_S_IOERR` (1) | device-write status=1 |
| `Desteklenmiyor` | cihaz status baytı | `VIRTIO_BLK_S_UNSUPP` (2) | device-write status=2 |
| `GecersizSektor` | sürücü ön-kontrol | — | `sektor >= aygit.kapasite_sektor` |
| `DescriptorTukendi` | sürücü | — | `vq.boyut < 3` (header/data/status sığmaz) |
| `TamamlanmaYok` | sürücü poll | — | `used.idx` ilerlemedi (HW'de timeout) |

Başarı: `tamam(boş)`. Status `0` (`VIRTIO_BLK_S_OK`) → `tamam`.

## Descriptor-zincir tasarımı (VirtIO 1.2 §5.2.6)

3-descriptor okuma zinciri; her descriptor 16-byte (addr le64, len le32,
flags le16, next le16):

```
desc[0] header : addr=HDR,         len=16,  flags=NEXT,        next=1   (device-READ)
desc[1] data   : addr=tampon.adres, len=512, flags=NEXT|WRITE,  next=2   (device-WRITE)
desc[2] status : addr=STATUS,      len=1,   flags=WRITE,       next=0   (device-WRITE)
header @HDR    : type le32=VIRTIO_BLK_T_IN(0), reserved le32=0, sector le64=sektor
avail publish  : avail.ring[0]=0(head desc); avail.idx += 1                 (le16)
[bare-metal: dmb ishst — C10 TODO; host-mock no-op]
used poll      : used.idx > son_görülen ? tamamlandı : TamamlanmaYok        (le16)
map            : status 0→tamam(boş); 1→AygitHatasi; 2→Desteklenmiyor
```

**C9 primitif eşlemesi:** addr → `mmio_yaz64`, len → `mmio_yaz32`, flags/next →
`mmio_yaz16`, sector → `mmio_yaz64`, avail/used idx → `mmio_yaz16`/`mmio_oku16`,
used id/len → `mmio_yaz32`, status baytı → `mmio_yaz16`/`mmio_oku16` (1 byte;
üst bayt 0). C9 yoksa le16 ring alanları yazılamaz/okunamazdı.

**Bellek yerleşimi (host-mock 16 KiB pencere, `adres % 16384`):** register'lar
0–276, header `1024`, status `1280`, data (`tampon.adres`) test'te `2048`,
desc `4096`, avail `8192`, used `12288` — çakışma yok. Gerçek DMA tahsisi
(`tekkez` sahipliği) ileride; şimdilik sabit scratch adresleri.

## Host-mock tamamlanma (Faz 1)

D4/D5 deseni: `blk_oku_cekirdek(..., host_mock_status: tam16)` çekirdeği,
cihazın yapacağı işi mock tampona inline simüle eder — avail publish sonrası
veriyi (`OK` ise sektöre bağlı 512B desen), status baytını ve used ring'i yazar.
Public `blk_oku` bunu `VIRTIO_BLK_S_OK` ile çağırır (başarı simülasyonu). Hata
yolları test'te `host_mock_status` = `1`/`2`/`255` (IOERR/UNSUPP/tamamlanma-yok)
ile sınanır. **Gerçek HW (D10): bu simülasyon bloğu kalkar**, cihaz used ring'i
IRQ ile doldurur (`// ===== HOST-MOCK CİHAZ SİMÜLASYONU =====` ile işaretli).

## C10 bariyeri (ertelendi)

Avail yazımı ile cihaz okuması arasına bare-metal'de `dmb ishst` gerekir (DMA
ring ordering). `virtio_blk_oku.kem` içinde `// TODO(C10)` ile işaretli.
Host-mock'ta bariyer no-op olduğundan **D9 onsuz çalışır**; C10 yalnız bare-metal
ring ordering için şart (Faz 2).

## Doğrulama

```
make -f tests/drivers/Makefile.drivers calistir_blk_oku_test
# build/kemgu --llvm <test> | clang -x ir | run  → exit 0
```

`virtio_blk_oku_test` (exit 0) altı yolu ayrı ayrı assert eder: başarı + tampon
içerik doğrulaması (`word[0]==sektor`, `word[127]==sektor+127`), IOERR→AygitHatasi,
UNSUPP→Desteklenmiyor, GecersizSektor, DescriptorTukendi, TamamlanmaYok. `opt
-passes=verify` temiz, 0 undefined sembol. `make test_tumu` (çekirdek suite)
etkilenmez — D9 yalnız `drivers/` + `tests/drivers/` + `docs/drivers/` ekler.

## Kapsam dışı

- Gerçek DMA tahsisi / `tekkez` ring sahipliği (sabit scratch adresler kullanılıyor).
- IRQ/ISR tamamlanma (D10); şimdilik host-mock used-ring poll.
- Çoklu eşzamanlı istek / descriptor free-list (tek-shot, slot 0).
- Yazma (`blk_yaz`), discard, flush — sonraki D-görevleri.
- `&Struct` handle modeline dönüş — C-track codegen fix sonrası (mekanik).
