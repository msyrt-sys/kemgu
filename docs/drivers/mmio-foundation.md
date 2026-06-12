# MMIO Foundation — Capability-Parametreli Register Erişimi

Bu doküman, KEMGU'da bellek-eşlemeli G/Ç (MMIO) **typed-width (16/32/64-bit)**
register erişimi için compiler + runtime tarafında kurulan **foundation**'ı
tanımlar. Amaç: Codex'in `drivers/virtio/virtio_mmio.kem` gibi **gerçek**
sürücüleri bu primitive'lerin üstüne yazabilmesi. Bu çalışma sürücü yazmaz;
yalnızca dil/araç altyapısını kurar.

> **C9 güncellemesi:** İlk foundation yalnız `mmio_*32` (32-bit) sağlıyordu.
> D9 (virtio-blk ring) descriptor/avail/used yapıları `le16` (idx, flags, next)
> ve `le64` (descriptor addr) alanları gerektirir. C9 bunun için **16/64-bit
> typed varyantları** (`mmio_oku16`/`yaz16`, `mmio_oku64`/`yaz64`) ekler ve host
> mock'u **byte-adreslenebilir** yapar (eski `(adres>>2)` kelime-collapse
> kaldırıldı). Ayrıntı: [§10](#10-c9--typed-width-1664-bit--byte-adreslenebilir-mock).

İlgili kaynaklar:
- `src/tip_kontrol.c` — `yetki<MMIO>` kaynağı + `mmio_oku*`/`mmio_yaz*` tip kontrolü
- `src/llvm.c` — codegen (runtime çağrısı + üst düzey `sabit` çözümü)
- `runtime/kdl_mmio.h`, `runtime/kdl_runtime_mmio.c` — host mock / bare-metal volatile
- `test/ornekler/mmio_smoke.kem` — referans örnek (32-bit handshake)
- `test/snapshots/mmio_genis.kem` — typed-width (le16 komşu + le64) verify fikstürü
- `test/test_mmio.c` — tip kontrol testleri (23/23: 32-bit M1-M15 + typed-width M16-M23)
- `test/test_llvm.c` — round-trip yürütme testleri (32-bit + C9 16/64-bit + komşu ayrışma)

---

## 1. API

```kemgu
mmio_oku16(y: yetki<MMIO>, adres: tam64) -> tam16     // le16
mmio_yaz16(y: yetki<MMIO>, adres: tam64, deger: tam16)
mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32     // le32
mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32)
mmio_oku64(y: yetki<MMIO>, adres: tam64) -> tam64     // le64
mmio_yaz64(y: yetki<MMIO>, adres: tam64, deger: tam64)
```

- **`mmio_okuN`** — `adres`'teki N-bit register'ı okur, değeri döner (`tamN`).
- **`mmio_yazN`** — `deger`'i `adres`'teki N-bit register'a yazar (`bos` döner).

Üçü de aynı kapı/semantik: ilk argüman derleme-zamanı `yetki<MMIO>` ispatı
(runtime'a geçmez), `adres` `tam64`, dönüş düz `tamN` (Karar 4 — `sonuç<>` yok).
Genişlik seçimi alanın VirtIO tipine göre: `le16` → 16-bit, `le32` → 32-bit,
`le64` → 64-bit. Endianness: host mock **açık little-endian**, bare-metal native
volatile (target zaten LE) — bkz. [§10](#10-c9--typed-width-1664-bit--byte-adreslenebilir-mock).

Producer (yetki üretimi) ve consumer (tüketim) — Capability Spec V1'den:

```kemgu
yetki_olustur(6, izin) -> yetki<MMIO>   // 6 = MMIO kaynak id; izin: 1=OKU 2=YAZ (OKU|YAZ = 3)
geri_al(y)                              // linear tüketim (capability iade)
delege(y, alt_izin) -> yetki<MMIO>      // alt-yetki (izin daraltma; y tüketilmez)
```

---

## 2. Dört Karar (Mehmet onaylı)

| # | Karar | Gerekçe |
|---|-------|---------|
| **1** | API adı `mmio_oku32` / `mmio_yaz32` | `donanim_*` çok geniş (port I/O, DMA da donanım). `mmio_` tam olarak ne yaptığını söyler; `VIRTIO_MMIO_*` offset sabitleriyle terminolojik uyumlu. |
| **2** | Capability-parametreli (Model 1) | İlk parametre `yetki<MMIO>`. Kernel-only intrinsic (Model 2) hızlı ama güvenlik anlatısını çöpe atar. Object-capability izolasyonu vitrin özelliği (Hedef 1). |
| **3** | `yetki<MMIO>` tek kaynak, katmanlı | Tek top-level resource kind. `MMIO_VirtIO` AYRI kaynak değil — `MMIO`'nun adres bölgesiyle daraltılmış formu. Geniş `Donanim` (Seçenek A) izolasyonu zayıflatır → red; cihaz-başına resource typechecker'ı şişirir → red. |
| **4** | Dönüş düz `tam32` (`sonuç<>` değil) | MMIO load donanım seviyesinde başarısız olmaz. `sonuç<tam32,Hata>` her register erişimine unwrap yükü bindirir, `gerçekzamanlı` ISR'da WCET'i bozar. Geçersiz adres = **programlama hatası** (init/compile-time), runtime hatası değil. |

---

## 3. Linear (object-capability) semantiği — ÖNEMLİ

`yetki<MMIO>` **linear**'dir (Capability Spec V1): kopyalanamaz, alias alınamaz,
scope sonunda **tüketilmeli** (`geri_al` / `ver` / çağrı argümanı). İki kuralı
karıştırmayın:

### 3.1. `mmio_oku32` / `mmio_yaz32` yetkiyi ÖDÜNÇ alır (tüketmez)

`delege` ve `kanal_al`/`kanal_gönder` gibi, MMIO erişim intrinsic'leri yetkiyi
**tüketmez**. Böylece **tek yetki ile birçok register erişimi** mümkündür:

```kemgu
değişken y: yetki<MMIO> = yetki_olustur(6, 3);
değişken a: tam32 = mmio_oku32(y, 0x0A000000);   // ödünç — y canlı kalır
mmio_yaz32(y, 0x0A000070, 1);                    // ödünç — y canlı kalır
değişken b: tam32 = mmio_oku32(y, 0x0A000004);   // ödünç
geri_al(y);                                       // TÜKETİM (tek noktada)
```

Erişimden sonra `geri_al` **şarttır**; yoksa scope sonunda `CP005` (yetki leak).
Bu, "yetki var = erişim hakkı var; ama hakkı sonunda iade et" modelidir.

### 3.2. Fonksiyona geçmek = MOVE (tüketim)

Düzenli bir fonksiyona `yetki<MMIO>` geçmek onu **çağrı yerinde tüketir** (move).
Callee yetkinin sahibidir; ya `geri_al` etmeli ya da `ver y` ile geri döndürmeli.
Çok-fonksiyonlu sürücüde yetkiyi **thread** edin (geri döndürün) ve **her adımda
yeni bir bağlama** kullanın (linear'da aynı isme reatama "canlanmaz" → `CP005`):

```kemgu
işlev mmio_status_yaz(y: yetki<MMIO>, deger: tam32) -> yetki<MMIO> {
    mmio_yaz32(y, 0x0A000070, deger);
    ver y;                                  // yetkiyi caller'a iade et
}

işlev init(y: yetki<MMIO>) -> tam32 {
    değişken magic: tam32 = mmio_oku32(y, 0x0A000000);   // ödünç
    değişken y1: yetki<MMIO> = mmio_status_yaz(y, 1);     // YENİ bağlama
    değişken y2: yetki<MMIO> = mmio_status_yaz(y1, 3);    // YENİ bağlama
    geri_al(y2);
    ver magic;
}
```

> **V1 sınırı (Mehmet'e iletildi):** Bir register OKUMASINI ayrı bir yardımcı
> fonksiyona taşımak (değer + yetki ikisini birden döndürememe nedeniyle) move
> semantiği altında zordur; okumalar yetkiyi tutan scope'ta yapılmalı ya da
> yazma-tarzı thread edilmeli. Çağrı-sınırında **ödünç** (borrow) parametresi
> (`yetki<MMIO>`'yu tüketmeden geçirme) tip/teorem değişikliği olduğundan V2'ye
> ve Mehmet onayına bırakıldı.

---

## 4. Resource modeli ve bölge daraltma (Karar 3)

`yetki<MMIO>` tip seviyesinde **tek** kaynaktır (nominal: `yetki<MMIO>`). "Bölge
daraltma", yetkinin hangi adres aralığını yetkilendirdiğinin **mint (üretim)
anında** belirlenmesidir. V1'de:

- Tip her zaman düz `yetki<MMIO>` (cihaz-başına tip patlaması yok → Karar 3).
- Daraltma = üretim-yeri sözleşmesi: yetki `[taban, taban+uzunluk)` penceresi
  için verilir. `adres` argümanı bu pencere içinde olmalıdır.
- **Geçersiz adres yakalaması init/compile-time'dır** (Karar 4): yetkiyi üreten
  kod (kernel/init) pencereyi sabitler. V1, per-erişim aralık denetimini tip
  seviyesinde henüz zorlamaz (V2: `KdlYetki`'ye `taban`/`uzunluk` alanları +
  derleme/init-time kontrol). Model bunu destekler (yetki değeri bölgeyi taşır).

`yetki<MMIO_VirtIO>` (Codex'in önerdiği biçim) bu modele şöyle eşlenir:
**ayrı bir kaynak değil**, `yetki<MMIO>` + VirtIO MMIO penceresi (örn. QEMU
`virt` slot 0: `0x0A000000`, uzunluk `0x200`). Sürücü `yetki<MMIO>` alır;
adresler `VIRTIO_MMIO_TABAN + offset` ile o pencereye kalır.

---

## 5. Host/mock vs. bare-metal davranışı

Codegen (`src/llvm.c`) her zaman runtime çağrısı emit eder (UART PL011 deseniyle
aynı). Davranış, hangi runtime nesnesinin **link edildiğiyle** seçilir:

```
mmio_oku16(y, adres)  →  call i16  @kdl_mmio_oku16(i64 adres)
mmio_yaz16(y,a,d)     →  call void @kdl_mmio_yaz16(i64 a, i16 d)
mmio_oku32(y, adres)  →  call i32  @kdl_mmio_oku32(i64 adres)
mmio_yaz32(y,a,d)     →  call void @kdl_mmio_yaz32(i64 a, i32 d)
mmio_oku64(y, adres)  →  call i64  @kdl_mmio_oku64(i64 adres)
mmio_yaz64(y,a,d)     →  call void @kdl_mmio_yaz64(i64 a, i64 d)
```

`runtime/kdl_runtime_mmio.c` iki modda derlenir (`kdl_runtime_uart_pl011.c` deseni):

| Mod | Derleme | Davranış |
|-----|---------|----------|
| **Host / mock** (varsayılan) | `-Iruntime` | **Byte-adreslenebilir** global tampon (16 KiB). Adres byte ofsetine eşlenir (`adres % 16384`); eski `(adres>>2)` kelime-collapse **kaldırıldı** → komşu byte adresleri (örn. `taban+64`, `taban+66`) AYRI slotlara düşer (`le16` ring alanları faithful). 16/32/64-bit erişim **açık little-endian** byte birleştirmesi yapar (host byte sırasından bağımsız). Keyfi cihaz adresi host'ta segfault-sız; deterministik **round-trip**. ASan temiz. |
| **Bare-metal** | `-DKEMGU_BARE_METAL` | `*(volatile uintN_t *)(uintptr_t)adres` — gerçek N-bit volatile load/store (PL011 idiomu). Derleyici erişimi optimize edemez/yeniden sıralayamaz. Native erişim, target byte sırasında (ARM64/x86_64 = LE). |

**Capability runtime'a gitmez:** `yetki<MMIO>` derleme-zamanı yetki ispatıdır;
codegen MMIO erişiminde yetkiyi argüman olarak runtime'a **geçirmez** (WCET için
sıfır ek yük). Tip sistemi `yetki<MMIO>` olduğunu zaten kanıtlar. Bu, object-
capability dilinde meşru ve `gerçekzamanlı` ISR için optimaldir.

---

## 6. Güvenlik varsayımları

1. **Authority = static proof.** `yetki<MMIO>` tutmak = MMIO erişim hakkı. Tip +
   linear denetim derleme-zamanında zorlanır; çalışma-zamanı ek kontrol yok (WCET).
2. **No ambient authority.** `yetki_olustur` çağrısı olmadan MMIO erişimi imkânsız.
   Bir fonksiyon yetki almıyorsa register'a dokunamaz.
3. **No alias / no copy.** `&yetki<MMIO>` yasak (`L004`); move sonrası erişim `CP005`.
4. **Kaynak izolasyonu (Karar 3).** `yetki<Dosya>`, `yetki<Donanim>` vb. MMIO
   erişiminde reddedilir (`MM002`). MMIO, geniş `Donanim`'dan ayrı ve dardır.
5. **Geçersiz adres = programlama hatası.** Runtime hata yolu yok (Karar 4); bölge
   üretim/init-time sabitlenir.
6. **Bare-metal'de runtime güveni:** Host mock güvenlidir; gerçek donanımda
   `adres`'in geçerli/eşlenmiş olması sürücü+kernel sorumluluğudur.

Hata kodları: `MM001` (argüman sayısı), `MM002` (ilk argüman `yetki<MMIO>` değil),
`MM003` (adres/değer tamsayı değil), `CP005` (linear ihlal: leak/çift tüketim).

---

## 7. Cross-file `sabit` codegen — DÜZELTİLDİ (0.4)

**Önceki durum:** Üst düzey `sabit X: T = literal;` bir fonksiyon gövdesinde
referans edilince LLVM codegen `; HATA: tanimsiz tanimlayici` üretiyordu
(aynı dosya dahil). `status.kem` bunu `128` literaliyle workaround'ladı;
`mock_transport.kem` sabit yerine fonksiyon (`... -> tam32 { ver 4096; }`) kullandı.

**Kök neden:** `src/llvm.c::tanimlayici_yukle` yalnızca yerel/parametre (`isim_bul`)
ve işlev adlarını çözüyordu; üst düzey `sabit` tanımlarını hiç tanımıyordu.

**Düzeltme (`src/llvm.c`):**
- `SabitKayit` tablosu + `llvm_ir_uret` içinde üst düzey `sabit` ön-geçişi.
- `tanimlayici_yukle`: `isim_bul`/`islev_bul` başarısızsa `sabit_bul` ile sabiti
  bulur ve **değer ifadesini referans yerinde inline eder** (sabitin beklenen IR
  tipini taşıyarak; sabit→sabit zincirleri özyinelemeli çözülür).
- Mevcut `kullan` ön-geçişi sabitleri program üyelerine zaten eklediğinden, bu
  düzeltme hem **aynı dosya** hem **`kullan` ile yüklenen (cross-file)** sabitleri
  kapsar.

**Sonuç:** Sürücüler register offset'lerini `sabit` olarak (kendi dosyalarında)
tanımlayıp kullanabilir. Doğrulama: `test/test_llvm.c` test 107 (`sabit` adresli
MMIO round-trip → exit 42) + `mmio_smoke.kem` `--llvm` temiz.

**Parke kalan:** Gerçek çok-dosya/modül linkleme (ayrı derleme birimleri arası
sembol çözümü) hâlâ modül sistemi gerektirir — bu foundation kapsamı dışı. Pratik
öneri: sürücüye ait sabitleri **o sürücünün dosyasında** tutun (zaten doğal desen).

---

## 8. Codex bundan sonra neye dokunabilir?

| Alan | İzin |
|------|------|
| `drivers/virtio/virtio_mmio.kem` (yeni) | ✅ Codex yazar |
| `tests/drivers/virtio/virtio_mmio_mock_test.kem` (yeni) | ✅ Codex yazar |
| `docs/drivers/virtio-mmio.md` (yeni) | ✅ Codex yazar |
| `src/`, `runtime/`, `tip_kontrol`, `llvm/`, `proofs/` | ⛔ Yasak (foundation tarafı) |

Codex yalnızca `§1`'deki imzaları kullanır; primitive'leri değiştirmez.

---

## 9. Codex'e devir contract'ı

```
Kullanılacak fonksiyonlar:
    mmio_oku16(y: yetki<MMIO>, adres: tam64) -> tam16        // le16 (D9 ring idx/flags/next)
    mmio_yaz16(y: yetki<MMIO>, adres: tam64, deger: tam16)   // -> bos
    mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32        // le32
    mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32)   // -> bos
    mmio_oku64(y: yetki<MMIO>, adres: tam64) -> tam64        // le64 (D9 descriptor addr)
    mmio_yaz64(y: yetki<MMIO>, adres: tam64, deger: tam64)   // -> bos
    // le16 değeri aritmetik/tam32 bağlamda kullanırken: 'mmio_oku16(...) olarak tam32'
    // (KEMGU'da implicit int genişletme YOK; açık 'olarak' cast zorunlu).

yetki<MMIO> nasıl elde edilir:
    değişken y: yetki<MMIO> = yetki_olustur(6, 3);   // 6 = MMIO, izin OKU|YAZ
    ...erişim (mmio_oku32/yaz32 ödünç alır, tüketmez)...
    geri_al(y);                                       // scope sonunda ZORUNLU tüketim
    // (Gerçek sürücüde yetki init/main'den parametre olarak da gelebilir.)

MMIO_VirtIO eşlemesi:
    AYRI resource YOK. yetki<MMIO_VirtIO> yerine yetki<MMIO> kullan;
    "bölge daraltma" = adresleri VirtIO penceresine sınırla:
        sabit VIRTIO_MMIO_TABAN: tam64 = 167772160;   // 0x0A000000 (QEMU virt slot 0)
        mmio_oku32(y, VIRTIO_MMIO_TABAN + offset);
    Pencere/uzunluk yetkiyi üreten kod (kernel/init) tarafından sabitlenir.

Çok-fonksiyonlu desen (linear):
    - Okuma: yetkiyi tutan scope'ta yap (ödünç).
    - Yazma yardımcısı: işlev f(y: yetki<MMIO>, ...) -> yetki<MMIO> { ...; ver y; }
      caller'da YENİ bağlamaya ata: değişken y1 = f(y, ...);
    - Aynı isme reatama linear'da çalışmaz (CP005) — her adımda yeni bağlama.

Import/dosya sırası:
    - V1 tek-dosya derleme. Sürücünün register offset SABİTLERİNİ kendi
      .kem dosyasında tanımla (üst düzey `sabit ... : tam64 = ...;`).
      Codegen aynı dosya sabitlerini çözer (cross-file sabit bug DÜZELTİLDİ).
    - `kullan modul::dosya` ile yüklenen sabitler de codegen'de çözülür;
      ama tip kontrol (--check) çok-dosya import'u henüz tam değil — güvenli
      yol: sürücüyü + sabitlerini tek dosyada tut.

sabit uyarısı:
    SERBEST (kök neden düzeltildi). Register offset'lerini named `sabit` yap;
    literal'e indirmeye gerek yok. (Sınır: ayrı derleme birimi linkleme = V2.)

Yasak alanlar:
    src/ runtime/ tip_kontrol llvm/ proofs/ — sadece drivers/ + tests/drivers/ + docs/

Beklenen örnek kullanım:
    test/ornekler/mmio_smoke.kem  (parse/check/llvm temiz; VirtIO handshake iskeleti)

Doğrulama (Codex kendi PR'ında):
    .\build\kemgu.exe --check drivers/virtio/virtio_mmio.kem   → OK
    .\build\kemgu.exe --llvm  drivers/virtio/virtio_mmio.kem   → "HATA"/"tanimsiz" YOK
    Mevcut 4 saf model (status/virtqueue/features/mock) bozulmamalı.
```

---

## 10. C9 — typed-width (16/64-bit) + byte-adreslenebilir mock

D9'un (virtio-blk request handling) ön-koşulu olan ring-bellek erişim katmanı.
32-bit foundation'ı **birebir genişletir** — yeni keyword/sözdizim/capability
modeli YOK; yalnız mevcut `mmio_*32` deseninin genişlik varyantları.

### 10.1. Neden gerekli (D9 ring yapısı)

VirtIO virtqueue yapıları sabit-genişlikli little-endian alanlardan oluşur:

| Yapı | Alan | Genişlik | Erişim |
|------|------|----------|--------|
| Descriptor | `addr` | `le64` | `mmio_oku64`/`yaz64` |
| Descriptor | `len` | `le32` | `mmio_oku32`/`yaz32` |
| Descriptor | `flags`, `next` | `le16` | `mmio_oku16`/`yaz16` |
| Avail / Used ring | `idx`, `ring[i]` | `le16` | `mmio_oku16`/`yaz16` |

Yalnız `mmio_*32` ile `le16` alanı okunamaz (komşu 16-bit alanı da kapsar) ve
`le64` adres tek erişimde alınamaz. C9 16/64-bit varyantları bunu çözer.

### 10.2. Byte-adreslenebilir host mock (KRİTİK düzeltme)

**Eski (collapse):** mock `uint32_t[4096]`'ydı, adres `(adres>>2) % 4096` kelime
indeksine eşleniyordu. Sonuç: `taban+64` ve `taban+66` **aynı kelimeye** düşüyordu
(`64>>2 == 66>>2 == 16`) → komşu `le16` ring alanları host'ta test EDİLEMİYORDU
(ikinci yazma birinciyi eziyor; eski repro exit 5).

**Yeni (byte-addressable):** mock `uint8_t[16384 + 8]`, adres `adres % 16384` byte
ofsetine eşlenir. `+8` dolgu: en yüksek ofsette 64-bit (8 byte) erişimin taşmasını
önler (ASan temiz). 16/32/64-bit erişim **açık little-endian** byte birleştirmesi
yapar (`kdl_mmio_le_oku`/`kdl_mmio_le_yaz`) — host endianness'inden bağımsız,
bare-metal LE target ile birebir aynı sonuç. Artık `taban+64` ve `taban+66` ayrı
slotlar; `le16` alanları faithful test edilir. 16 KiB pencere boyu değişmedi
(yalnız adresleme kelime→byte oldu); 32-bit erişim regresyonsuz.

### 10.3. Endianness sözleşmesi

VirtIO alanları little-endian'dır. KEMGU `tamN` tipleri ve LLVM `iN` doğal olarak
target byte sırasındadır; hedef platformlar (x86_64, ARM64) zaten LE. Bu yüzden:

- **Bare-metal:** native `*(volatile uintN_t *)` — target LE, VirtIO LE ile uyumlu.
- **Host mock:** açık LE byte birleştirme — host LE olsa da olmasa da deterministik
  LE; bare-metal davranışıyla bit-bit aynı. Big-endian host'ta dahi faithful kalır.

### 10.4. Testler

- `test/test_mmio.c` M16-M23 (tip kontrol): oku16/yaz16/oku64/yaz64 pozitif +
  arg sayısı (MM001) + yanlış kaynak (MM002) + değer tamsayı değil (MM003).
- `test/test_llvm.c` (round-trip yürütme): 16-bit round-trip → 42, 64-bit
  round-trip → 42, **komşu 16-bit ayrışma** (4096/4098 → 10+32 = 42; eski mock'ta
  64'e çakışırdı), `mmio_genis.kem` fikstürü `opt -passes=verify` PASS + exit 42.

### 10.5. Sınırlar / kapsam dışı

- Ring veri modeli / descriptor / free-list → **D9 (Codex)** işi; C9 yalnız erişim
  primitifini verir.
- DMA bellek bariyeri (`dmb ishst` / `mfence`) → **C10** (ayrı iş). Host mock'ta
  no-op olacağı için D9 host-testini bloklamaz; yalnız bare-metal ring ordering
  için gerekir. C9'a dahil DEĞİL.
- Per-erişim adres aralık denetimi hâlâ V2 (Karar 3/§4 ile aynı; genişlik bunu
  değiştirmez).
