# MMIO Foundation — Capability-Parametreli Register Erişimi

Bu doküman, KEMGU'da bellek-eşlemeli G/Ç (MMIO) 32-bit register erişimi için
compiler + runtime tarafında kurulan **foundation**'ı tanımlar. Amaç: Codex'in
`drivers/virtio/virtio_mmio.kem` gibi **gerçek** sürücüleri bu primitive'lerin
üstüne yazabilmesi. Bu çalışma sürücü yazmaz; yalnızca dil/araç altyapısını kurar.

İlgili kaynaklar:
- `src/tip_kontrol.c` — `yetki<MMIO>` kaynağı + `mmio_oku32`/`mmio_yaz32` tip kontrolü
- `src/llvm.c` — codegen (runtime çağrısı + üst düzey `sabit` çözümü)
- `runtime/kdl_mmio.h`, `runtime/kdl_runtime_mmio.c` — host mock / bare-metal volatile
- `test/ornekler/mmio_smoke.kem` — referans örnek
- `test/test_mmio.c` — tip kontrol testleri (15/15)
- `test/test_llvm.c` — round-trip yürütme testleri (106, 107)

---

## 1. API

```kemgu
mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32
mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32)
```

- **`mmio_oku32`** — `adres`'teki 32-bit register'ı okur, değeri döner.
- **`mmio_yaz32`** — `deger`'i `adres`'teki 32-bit register'a yazar (`bos` döner).

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
mmio_oku32(y, adres)  →  call i32  @kdl_mmio_oku32(i64 adres)
mmio_yaz32(y,a,d)     →  call void @kdl_mmio_yaz32(i64 a, i32 d)
```

`runtime/kdl_runtime_mmio.c` iki modda derlenir (`kdl_runtime_uart_pl011.c` deseni):

| Mod | Derleme | Davranış |
|-----|---------|----------|
| **Host / mock** (varsayılan) | `-Iruntime` | Global tampon (16 KiB). Adres 4-byte kelime indeksine eşlenir (mod tampon). Keyfi cihaz adresi (örn. `0x0A000000`) host'ta segfault-sız; yaz-sonra-oku deterministik **round-trip**. ASan temiz. `--llvm` çıktısı host'ta derlenip çalıştırılabilir. |
| **Bare-metal** | `-DKEMGU_BARE_METAL` | `*(volatile uint32_t *)(uintptr_t)adres` — gerçek volatile load/store (PL011 idiomu). Derleyici erişimi optimize edemez/yeniden sıralayamaz. |

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
    mmio_oku32(y: yetki<MMIO>, adres: tam64) -> tam32
    mmio_yaz32(y: yetki<MMIO>, adres: tam64, deger: tam32)   // -> bos

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
