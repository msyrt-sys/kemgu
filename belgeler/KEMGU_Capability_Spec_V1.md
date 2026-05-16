# KEMGU Capability (Object-Capability) Spec V1

**Durum:** TASLAK (Direktif Hedef 1 — Kırılamaz Güvenlik altında).
**Spec içi alt-adımlar otomatik onaylı; Direktif Ek v1.2 ile MERGE bekler.**

---

## CP.0 — Motivasyon ve Üç Stratejik Hedef Bağlantısı

KEMGU `yetki<R>` tipi, bir kaynağa (Dosya, Soket, Bellek, Donanım, OTP_Anahtar
vb.) erişim için **taşınabilir bir yetki belirteci**dir (capability /
object-capability token). Erişim kontrolü, Unix `rwx` permission flag'leri
veya capability-list (Saltzer & Schroeder 1975, "The Protection of Information
in Computer Systems") gibi **dış tabloya** değil, kaynağa erişen kodun **elinde
tuttuğu nesneye** dayanır. seL4 (Klein et al. 2009, SOSP) ve Genode
(Feske 2011) bunu mikro-kernel düzeyinde gerçekleştirir.

### CP.0.1 Unix Permission Modelinin Yetersizlikleri

Klasik Unix discretionary access control (DAC) modeli iki klasik açıkla maluldür:

1. **Ambient authority problemi (Miller 2006, PhD thesis):** Bir Unix
   process'i, kullanıcısının **tüm** yetkilerine sahiptir. `cat file.txt`
   çalıştırıldığında, `cat` programı sadece `file.txt`'yi okuma yetkisine
   değil, kullanıcının tüm dosyalarını okuma + ağ açma + child process
   spawn etme + signal gönderme yetkilerine sahiptir. Programcı *hangi
   yetkilerin gerektiğini* açıkça beyan etmez; runtime sistem **çevre
   yetkisinden (ambient authority)** verir.

2. **Confused deputy attacks (Hardy 1988, ACM OSR):** Klasik örnek IBM
   System/38 compiler'ı: derleyici hem **kullanıcının** kaynak kodunu okur,
   hem de **debug log'una** yazar. Saldırgan kaynak dosya adı olarak
   `/etc/passwd` verir; derleyici kendi yüksek-yetkisiyle açıp debug log'a
   yazar; saldırgan debug log'undan `passwd` içeriğini okur. Derleyici
   "şaşkın vekil" rolünde — kendi yetkisi ile başkasının niyetini yerine
   getirir. Modern muadiller: Cross-Site Request Forgery (CSRF), TOCTOU
   sembolik link saldırıları, sudo arg injection.

3. **Path-based capability eksikliği:** `open(path)` sistem çağrısı
   `path` üzerinden çözüm yapar; başkası `path`'i değiştirebilir veya
   sembolik link koyabilir (TOCTOU). Capability modeli `open` çağrısının
   bir handle (file descriptor benzeri ama unforgeable) döndürmesini
   gerektirir.

### CP.0.2 Object-Capability Modelinin Çözümü

Capability = **unforgeable** (taklit edilemez), **delegable** (devredilebilir,
ama tam yetki ile değil, alt-yetki olarak), **revocable** (geri alınabilir)
token. Programın yaptığı her I/O çağrısı, **ilgili capability** parametresini
gerektirir. Kaynak yoksa erişim derleme zamanında reddedilir.

Klasik literatür:
- **Dennis & Van Horn 1966 (Comm. ACM):** İlk capability tabanlı OS önerisi.
- **Hardy 1985 (KeyKOS):** Tüm OS capability tabanlı, persistent.
- **Miller 2006 (PhD thesis, "Robust Composition"):** Object-capability calculus,
  E dili, capability-secure JavaScript (Caja).
- **Klein et al. 2009 (seL4, SOSP):** Capability'leri seçilebilen mikro-kernel'de,
  her sistem çağrısı capability gerektirir; formal verification (Coq + Isabelle).
- **Feske 2011 (Genode OS Framework):** seL4 ilkesini desktop-ish OS'a taşıdı.

### CP.0.3 Üç Stratejik Hedef Bağlantısı

- **HEDEF 1 (Kırılamaz Güvenlik):** Confused deputy + ambient authority +
  TOCTOU sınıflarının **derleme zamanında engellenmesi**. OTP anahtarı,
  TLS session key, kernel page table, IPC handle — hepsi `yetki<R>` ile.
  `dosya_oku(yol)` yerine `dosya_oku(y: yetki<Dosya>)` çağrı zorunlu.
- **HEDEF 2 (Maksimum Performans):** Runtime check minimum — capability
  yapısı 16 byte (`id: tam64`, `tip: tam16`, `izin: tam16`, `iptal: mantıksal`,
  rezerv); revoke kontrolü tek bit comparison; ambient authority araması yok.
- **HEDEF 3 (Evrensel OS):** Kernel + userspace **aynı dilde**, **aynı capability
  semantiği**. ARM64 DGX Spark / desktop / mobile aynı disiplin. Driver
  yazımı kapsam-ayrılmış capability'lerle (DMA capability, MMIO capability).

### CP.0.4 ASLA Listesi Hatırlatması

- ASLA implicit conversion: `R` → `yetki<R>` **asla** otomatik değil. Yetki
  yalnız üretici (`yetki_olustur`) ile elde edilir.
- ASLA exception: `yetki_kullan(y, ...)` revoked ise `sonuc<T, HataKodu>`
  döner; runtime panic yok.
- ASLA null: `yetki<R>` her zaman var olur; revoke = "iptal flag'i set" değil
  "ele geçirme = invalidate", yine tip içinde takip edilir.
- ASLA GC: `yetki<R>` linear olarak takip edilir; sızıntı = compile error.

---

## CP.1 — Tip Tanımı

```
yetki<R> : tip      (R : kaynak tipi)
```

`yetki` bir tip kurucusudur. Runtime temsili sabit (16-byte struct):

```c
struct kdl_yetki {
    uint64_t id;          /* unforgeable token id (PRNG) */
    uint16_t kaynak_tipi; /* enum: 1=Dosya, 2=Soket, 3=Bellek, 4=Donanim,
                                  5=OTP_Anahtar, ... */
    uint16_t izin;        /* bit field: bkz. CP.5 */
    uint8_t  iptal;       /* 0 = aktif, 1 = revoked */
    uint8_t  rezerv[3];   /* hizalama */
};
```

`R` aşağıdaki **kaynak tiplerinden** biri olmalıdır (CP.4 CP004 zorunluluğu):

| Kaynak `R`     | Anlamı                                | Tipik üretici            |
|----------------|----------------------------------------|--------------------------|
| `Dosya`        | Disk dosyası / FIFO                    | `dosya_ac → yetki<Dosya>`|
| `Soket`        | TCP/UDP/Unix socket                    | `soket_ac → yetki<Soket>`|
| `Bellek`       | Page-aligned memory region             | `bellek_haritala`        |
| `Donanim`      | Driver MMIO/DMA region                 | `mmio_haritala`          |
| `OTP_Anahtar`  | One-Time-Pad anahtarı (linear ile)     | `otp_uret`               |

V1'de bu beş kaynak. V2'de stdlib genişler (Kanal, Saat, Rasgele, vs.).

### CP.1.1 Linear Integration

`yetki<R>` aynı zamanda **linear olarak takip edilir**. Yani:
- Kopyalanamaz — atama / argüman geçirme = transfer (Linear Spec V1 L-NO-COPY).
- Sessizce atılamaz — scope kapanırken `yetki_iptal(y)` veya `delege(y,...)`
  veya bir I/O çağrısına geçirme zorunludur (CP.4 CP005).
- Referans alınamaz (Linear Spec V1 L-NO-ALIAS, hata kodu CP005 + L004).

Bu, **tekkez<yetki<Dosya>>** yazmayı zorunlu kılmaz; `yetki<R>` *zaten linear*.
İçe sarma izinli: `tekkez<yetki<R>>` da geçerli (lineer × lineer = lineer).
Ama redundant — V1'de gereksiz nesting CP004 üretmez (Linear/Linear OK).

---

## CP.2 — Sözdizim

Üç yeni anahtar kelime (35-37. AK — toplam 37):

- `yetki` — tip kurucusu (`yetki<Dosya>`) **ve** producer intrinsic adı
  (`yetki_olustur(...)`)
- `delege` — yetki bölüştürme operatörü (`delege(y, izin)`)
- `geri_al` — yetki iptal operatörü (`geri_al(y)`)

Operasyonlar **built-in çağrı sözdizimi** kullanır; özel `kullan`/`imha` gibi
parser-level form değil — basit `cagri` AST'sine çözülür ve **özel-case**
tip kontrolü ile doğrulanır. Bu, lexer/parser tablosunu minimal tutar.

### CP.2.1 Built-in İntrinsiklerin Tam Listesi

| Çağrı                          | İmza                                                          | Anlamı                |
|--------------------------------|---------------------------------------------------------------|------------------------|
| `yetki_olustur(kaynak_tipi, izin)` | `(tam16, tam16) -> yetki<R>`                          | Producer (compiler-only) |
| `delege(y, yeni_izin)`         | `(yetki<R>, tam16) -> yetki<R>`                              | Alt-yetki üretir; `y` korunur |
| `geri_al(y)`                   | `(yetki<R>) -> boş`                                           | `y` invalidate; tüketim |
| `yetki_kontrol(y, gerekli)`    | `(yetki<R>, tam16) -> sonuc<boş, HataKodu>`                   | Runtime check (CP.6) |
| `yetki_izin(y, istenen)`       | `(yetki<R>, tam16) -> mantıksal`                              | İzin biti boolean sorgu — y tüketilmez (KIRMIZI G.3 eklendi) |
| `yetki_id(y)`                  | `(yetki<R>) -> tam64`                                         | Diagnostic (ifşa benzeri) |

`yetki_olustur` SADECE kernel/runtime kodu içinde çağrılabilir — userspace
KEMGU programları için bu işlem `dosya_ac`, `soket_ac` vb. **yüksek-seviyeli
producer'lar üzerinden** yapılır. Compiler şu an `yetki_olustur`'u global
olarak kabul eder; V2'de capability-private modülü gelir.

### CP.2.2 Sözdizimsel Örnekler

```kemgu
// Üretim (sadece runtime/kernel bağlamında — userspace dosya_ac kullanır)
değişken y: yetki<Dosya> = yetki_olustur(1, 0b011);  // Dosya, oku+yaz

// Delege (alt-yetki kop — y orijinal *kalır*, y2 yeni token)
değişken y2: yetki<Dosya> = delege(y, 0b001);        // sadece oku

// I/O çağrısı — yetki zorunlu (CP001 olmazsa)
değişken icerik: metin = dosya_oku_yetkili(y2, "kayit.txt");

// Geri al — y2 invalidate
geri_al(y2);

// y2 kullanılırsa CP002
// değişken x = dosya_oku_yetkili(y2, "...");   // CP002

// y hala aktif — scope sonunda tüketim zorunlu
geri_al(y);
```

```kemgu
// Confused deputy senaryosu — KEMGU'da derlemez!
işlev derleyici(kaynak_yol: metin, debug_y: yetki<Dosya>) {
    // Eskiden: open(kaynak_yol) -> ambient authority
    // Şimdi: kaynak için ayrı yetki gerekir!
    // dosya_oku_yetkili(debug_y, kaynak_yol);   // CP003: debug_y yazma değil
}
```

---

## CP.3 — Tip Kuralları

Notasyon: `Γ ⊢ e : τ`. `R` bir kaynak tipi (CP.1 listesinde).

### CP-FORM (Tip Formasyonu)

```
R : kaynak-tipi  (Dosya, Soket, Bellek, Donanim, OTP_Anahtar, ...)
─────────────────────────────────────────────────────────────────
yetki<R> : tip
```

Bilinmeyen `R` = CP004 (CAPABILITY_RESOURCE_INVALID).

### CP-PROD (Producer Intrinsic)

```
Γ ⊢ kt : tam16     Γ ⊢ izin : tam16     kt ∈ {kaynak_tipi_id_seti}
──────────────────────────────────────────────────────────────────
Γ ⊨ yetki_olustur(kt, izin) : yetki<R>    (R = lookup(kt))
```

Kaynak tipi `kt` integer literal olmalı (V1) — `1`=Dosya, `2`=Soket,
`3`=Bellek, `4`=Donanim, `5`=OTP_Anahtar. Dinamik `kt` (variable) v2'de.

### CP-DELEGE (Delegate)

```
Γ ⊢ y : yetki<R>     Γ ⊢ izin : tam16     izin ⊆ y.izin (compile)
──────────────────────────────────────────────────────────────────
Γ ⊨ delege(y, izin) : yetki<R>   ⇒   Γ   (y tüketilmez, *üretilen* y2 linear)
```

Delege **alt-yetki** üretir. Anlamsal kural: `y2.izin ⊆ y.izin` (bit-subset).
Compile zamanında çoğu durumda izin literal olduğundan kontrol edilir;
runtime'da da `kdl_yetki_delege` aynı kontrolü yapar.

Önemli: `delege(y, ...)` çağrıldığında `y` linear **tüketilmez** — çünkü
yeni bir token üretir, eskisi de geçerli kalır (alt-yetki kavramı). Yine de
delege'nin dönüşü `yetki<R>` linear, takibe alınır.

### CP-GERI_AL (Revoke / Dispose)

```
Γ ⊢ y : yetki<R>
─────────────────────────────────────
Γ ⊨ geri_al(y) : boş   ⇒   Γ \ {y}
```

`y`'yi runtime'da iptal eder (iptal flag = 1) ve linear takipte tüketir.

### CP-QUERY (Permission Inspection) — KIRMIZI G.3

```
Γ ⊢ y : yetki<R>     Γ ⊢ istenen : tam16
─────────────────────────────────────────
Γ ⊨ yetki_izin(y, istenen) : mantıksal   ⇒   Γ   (y tüketilmez)
```

Runtime karşılığı: `kdl_yetki_izin_var_mi(y, istenen) -> i1`. Semantik:

```
yetki_izin(y, istenen) = ¬y.iptal ∧ y.id ≠ 0 ∧ (y.izin ∧ istenen) = istenen
```

Confused deputy senaryolarında **dinamik izin denetimi** için kullanılır
— compile-time izin literal değilse (örneğin runtime-input'tan gelen
istenen), kod yine de güvenli kalır.

`y` tüketilmez (delege'ye benzer ama yeni token üretmez — pure query).

### CP-IO (I/O Çağrısı)

```
Γ ⊢ y : yetki<R>     gerekli_izin ⊆ y.izin     y.iptal = 0 (runtime)
────────────────────────────────────────────────────────────────────
Γ ⊨ kaynak_i_o(y, ...) : sonuc<T, HataKodu>
```

Her I/O işlevi `yetki<R>` parametresi alır. Compile zamanında kaynak tipi
eşleşmesi kontrolü; runtime'da iptal flag + izin bit-mask kontrolü.

### CP-NO-COPY / CP-NO-ALIAS

`yetki<R>` Linear Spec V1 ile aynı kurallar (L-NO-COPY, L-NO-ALIAS).
İhlal: CP005 (CAPABILITY_LINEAR_VIOLATION).

### CP-LIFETIME

`yetki<R>` linear olarak takip edilir → her scope kapanmadan önce
**tam bir kez** tüketilmelidir. Tüketim:
- `geri_al(y)` — revoke + dispose
- I/O çağrısı argümanı (`dosya_oku_yetkili(y, ...)` vs.)
- `ver y` — çağırana devir
- `delege(y, ...)` **tüketmez** (alt-yetki üretir, ana yetki kalır)

Tüketilmemiş = CP005 (linear leak).

---

## CP.4 — Hata Kodları

| Kod   | Adı                              | Anlamı                                          |
|-------|----------------------------------|-------------------------------------------------|
| CP001 | CAPABILITY_MISSING               | Kaynak erişiminde `yetki<R>` parametresi yok    |
| CP002 | CAPABILITY_REVOKED               | İptal edilmiş yetki kullanıldı (runtime)        |
| CP003 | CAPABILITY_PERMISSION_INSUFFICIENT | İzin seviyesi yetersiz (oku istenip yaz yetkisi gibi) |
| CP004 | CAPABILITY_TYPE_MISMATCH         | `yetki<Dosya>` Soket için kullanıldı            |
| CP005 | CAPABILITY_LINEAR_VIOLATION      | Linear semantik ihlali (kopya/alias/sızıntı)    |

### CP.4.1 CP001 Detay — Eksik Yetki

```kemgu
işlev oku_yetkisiz() -> metin {
    ver dosya_oku("kayit.txt");   // CP001 — dosya_oku artık y: yetki<Dosya> alır
}
```

V1'de yapı: `dosya_oku_yetkili`, `soket_yaz_yetkili` vs. **yeni** kapasiteli
varyantlar. Eski `dosya_oku(path: metin)` deprecated; v1'de hala mevcut
(geriye uyumluluk) ama capability-yokluk uyarısı verir. V2'de tamamen
kaldırılır (eski API CP001).

### CP.4.2 CP002 Detay — Revoked

```kemgu
değişken y: yetki<Dosya> = ...;
geri_al(y);
dosya_oku_yetkili(y, "k.txt");   // CP005 linear (y tüketildi)
                                  // Tüketilmemiş olsa CP002 runtime
```

CP002 normalde runtime'da yakalanır (`yetki_kontrol → sonuc<..., HataKodu>`).
Compile-time CP005 daha kapsamlı (y zaten tüketildi).

### CP.4.3 CP003 Detay — İzin Yetersiz

```kemgu
değişken y_oku: yetki<Dosya> = ...;       // sadece OKU
dosya_yaz_yetkili(y_oku, "k.txt", "v");   // CP003 — yaz izni gerekir
```

Compile-time, yetki izni literal/bilinen ise kontrol edilir; runtime'da
da `kdl_yetki_kontrol(y, KDL_IZIN_YAZ)` çalışır.

### CP.4.4 CP004 Detay — Tip Yanlış

```kemgu
değişken y: yetki<Soket> = ...;
dosya_oku_yetkili(y, "k.txt");   // CP004 — Soket ≠ Dosya
```

Pure compile-time; nominal tip eşitliği.

### CP.4.5 CP005 Detay — Linear İhlali

```kemgu
değişken y1: yetki<Dosya> = ...;
değişken y2: yetki<Dosya> = y1;          // CP005 — y1 tüketildi
geri_al(y1);                              // CP005 — y1 zaten move'di
&y1;                                       // CP005 — referans alma yasak
// Scope sonunda y2 tüketilmedi -> CP005
```

CP005 Linear Spec V1'nin L001/L002/L003/L004 koşullarını **yetki-bağlamında**
kapsar.

---

## CP.5 — İzin Seviyeleri (Bit Field)

`tam16` bit-mask:

| Bit   | Sabit              | Anlam                                    |
|-------|--------------------|------------------------------------------|
| 0     | `YETKI_OKU` (0x01) | Okuma izni                               |
| 1     | `YETKI_YAZ` (0x02) | Yazma izni                               |
| 2     | `YETKI_CALISTIR` (0x04) | Çalıştırma izni (executable)        |
| 3     | `YETKI_SIL` (0x08) | Silme izni                               |
| 4     | `YETKI_DEVRET` (0x10) | Delege ile alt-yetki üretebilir        |
| 5-14  | (rezerv)           | V2/V3'te genişler                        |
| 15    | `YETKI_HEPSI` (0x8000) | Tam yetki sentineli (root benzeri) |

Sabitler `stdlib::yetki::*` modülünde tanımlanır (V1 hardcoded):
```kemgu
sabit YETKI_OKU: tam16 = 1;
sabit YETKI_YAZ: tam16 = 2;
sabit YETKI_CALISTIR: tam16 = 4;
sabit YETKI_SIL: tam16 = 8;
sabit YETKI_DEVRET: tam16 = 16;
sabit YETKI_HEPSI: tam16 = 32768;
```

**Subset kuralı (CP-DELEGE):** `y2.izin & ~y.izin == 0` (y2'nin izinleri
y'nin izinlerinin alt-kümesi olmalı).

---

## CP.6 — LLVM Codegen ve Runtime

### CP.6.1 Yetki Struct LLVM Tipi

```llvm
%kdl_yetki = type { i64, i16, i16, i8, [3 x i8] }
```

`yetki<R>` IR tipi: `%kdl_yetki`. By-value veya by-pointer geçirim
mevcut struct-by-value (LLVM v3) ile aynı.

### CP.6.2 Producer / Delege / Revoke / Query

```llvm
declare %kdl_yetki @kdl_yetki_olustur(i16 noundef, i16 noundef)
declare %kdl_yetki @kdl_yetki_delege(ptr noundef, i16 noundef)    ; C1
declare void @kdl_yetki_geri_al(ptr noundef)                       ; mutate: iptal=1
declare i32 @kdl_yetki_kontrol(ptr noundef, i16 noundef)           ; C1
declare i32 @kdl_yetki_kontrol_tipi(ptr, i16, i16)                 ; C1
declare i64 @kdl_yetki_id(ptr noundef)                             ; C1
declare i16 @kdl_yetki_tipi(ptr)                                   ; C1
declare i16 @kdl_yetki_izin(ptr)                                   ; C1 — internal
declare i8 @kdl_yetki_iptal_mi(ptr)                                ; C1
declare i8 @kdl_yetki_izin_var_mi(ptr, i16)                        ; G.3 + C1
```

`kdl_yetki_geri_al` mutate eder; tüm query/inspector çağrıları `y`'nin
**alloca**'sına pointer geçirir (KEMGU LLVM emit otomatik). Read-only
arg'lar `const KdlYetki *` C imzasıyla.

**ABI gerekçesi (C1, G.3 dersinden):** Win64 ABI'sinde 16-byte struct
by-value arg geçişi `byval` attribute eklenmediği sürece LLVM IR'de
güvensiz (segfault). KEMGU LLVM emit'i şu an `byval` üretmiyor,
dolayısıyla **tüm `yetki<R>` arg'ları by-pointer**. Return değeri
`%kdl_yetki` by-value çalışıyor (clang `sret` attribute'unu otomatik
ekliyor 16-byte için).

### CP.6.3 Runtime Kontroller

`kdl_yetki_kontrol`:
- `y.iptal == 1` → CP002 (return -2)
- `(y.izin & gerekli) != gerekli` → CP003 (return -3)
- OK → return 0

`kdl_yetki_delege`:
- `(yeni_izin & ~y.izin) != 0` → CP003 (return invalid yetki, id=0)
- OK → return new yetki, id = `kdl_yetki_id_uret()` (PRNG)

### CP.6.4 PRNG Token Üretimi

`id` field unforgeability için PRNG ile üretilir. V1: xorshift64 (mevcut
`kdl_prng_next64`). V2: CSPRNG (/dev/urandom + ChaCha20). Kernel modunda
hardware RNG (RDRAND / DGX Spark TRNG).

`id == 0` reserved (geçersiz / null yetki sentineli, ama API'de döndürülmez —
hata için `kdl_yetki_kontrol` kullanılır).

---

## CP.7 — İlk Kullanıcı: dosya_ac_yetkili + OTP

### CP.7.1 Dosya I/O Yeniden Yazımı

```kemgu
// Yetkili (yeni) API:
işlev dosya_ac_yetkili(yol: metin, izin: tam16) -> sonuc<yetki<Dosya>, tam32> {
    // Native runtime sarmalayıcı:
    // 1. fopen(yol, mod_from(izin))
    // 2. başarılı -> yetki_olustur(KAYNAK_DOSYA, izin)
    // 3. başarısız -> hata(errno)
    güvensiz [etiket: "FFI: dosya open"] {
        // ...
    }
}

işlev dosya_oku_yetkili(y: yetki<Dosya>, n: tam32) -> sonuc<metin, tam32> { ... }
işlev dosya_yaz_yetkili(y: yetki<Dosya>, m: metin) -> sonuc<tam32, tam32> { ... }
işlev dosya_kapat_yetkili(y: yetki<Dosya>) -> boş { /* geri_al(y) içerir */ }
```

Eski API (`dosya_oku(yol: metin)`) deprecated; v1'de hala derlenir, v2'de
silinir.

### CP.7.2 OTP Integration

```kemgu
işlev otp_uret_yetkili(boyut: tam32) -> tekkez<yetki<OTP_Anahtar>> {
    // Lineer × Capability — anahtar tek-kullanımlık + capability'li
    ...
}

işlev otp_xor_yetkili(y: tekkez<yetki<OTP_Anahtar>>,
                      msg: Dizi<dtam8>) -> Dizi<dtam8> {
    // y burada tüketilir (linear); aynı anahtar tekrar kullanılamaz.
    ...
}
```

Bu, **iki güvenlik invaryantı**nı birleştirir:
1. OTP anahtarı asla tekrar kullanılamaz (linear).
2. Capability olmadan OTP işlevi çağrılamaz (object-capability).

---

## CP.8 — Test Minimum Sayısı: **35**

Test dağılımı (`test/test_capability.c`):

| Grup | Test #  | Konu                                          |
|------|---------|-----------------------------------------------|
| C1   | 1-4     | Tip ifadesi + producer (pozitif)              |
| C2   | 5-8     | delege alt-yetki + izin alt-kümesi (pozitif)  |
| C3   | 9-12    | geri_al + linear tüketim (pozitif)            |
| C4   | 13-16   | I/O çağrısı yetki ile (pozitif)               |
| C5   | 17-19   | CP001 eksik yetki (negatif)                   |
| C6   | 20-22   | CP002 revoked use (compile-CP005, runtime CP002) |
| C7   | 23-25   | CP003 izin yetersiz (negatif)                 |
| C8   | 26-28   | CP004 tip yanlış (negatif)                    |
| C9   | 29-32   | CP005 linear ihlali (kopya/alias/sızıntı)     |
| C10  | 33-37+  | Confused deputy + path TOCTOU senaryoları (CCS/USENIX) |

35+ test eşiği checkpoint tetikleyicidir.

### CP.8.1 Confused Deputy Senaryoları

Test 33-37 gerçek-dünya saldırılarının modellenmesidir:

- **Test 33 (Hardy 1988 PL/I derleyici):** Derleyici fonksiyonu hem kaynak
  hem debug log yetkisi alır; argüman olarak kaynak yolu verilir. Compile
  zamanında her iki capability ayrı geçirilir → confused deputy imkansız.

- **Test 34 (TOCTOU symlink saldırısı):** `dosya_ac_yetkili` capability
  döndürür; sonraki I/O o capability ile (handle-based); path tabanlı
  TOCTOU yok.

- **Test 35 (CSRF benzeri):** Network handler iki ayrı yetki alır (request
  capability, response capability). Saldırgan response capability'i ele
  geçirse de request'i okuyamaz.

- **Test 36 (sudo arg injection):** Sub-process spawn capability (`yetki<SubProc>`)
  ayrı; mevcut process capability'leri devredilmez (no ambient transfer).
  V1 placeholder — Process capability tipi v2'de.

- **Test 37 (OpenSSL Heartbleed muadili):** Memory read capability,
  byte sayısı argümanına bağlanmaz → out-of-bound okuma yetkisi yoksa
  derlemez.

---

## CP.9 — Uygulama Sırası

1. **Lexer**: `yetki`, `delege`, `geri_al` keyword (toplam 37).
2. **AST**: `DUGUM_TIP_YETKI` (+ `.veri.tip_yetki.kaynak_tipi`).
   Operasyonlar normal `DUGUM_CAGRI` olarak parse edilir (özel-case tip
   kontrolünde).
3. **Parser**: `parse_tip` → `yetki<R>` (R = TipBilgisi).
4. **Tip sistemi**: `TIP_YETKI` kategori + `tip_olustur_yetki` +
   `tip_yetki_mi`. Recursive nominal eşitlik, yazdırma.
5. **Tip kontrol** (kritik kısım):
   - `yetki_olustur(...)` producer intrinsic (CP-PROD)
   - `delege(...)` ve `geri_al(...)` özel-case (CP-DELEGE, CP-GERI_AL)
   - `dosya_oku_yetkili(y: yetki<Dosya>, ...)` parametre tipi eşleşmesi
   - CP001 / CP002 / CP003 / CP004 / CP005 enforcement
   - Linear semantik: `tip_lineer_mi` artık `TIP_YETKI` için de 1 döner
6. **LLVM**: `%kdl_yetki = type { i64, i16, i16, i8, [3 x i8] }` global tip;
   intrinsik çağrılar `declare`. Pointer geçişi `geri_al` için.
7. **Runtime** (`runtime/kdl_runtime.c` eklemeleri):
   - `kdl_yetki_olustur`, `kdl_yetki_delege`, `kdl_yetki_geri_al`,
     `kdl_yetki_kontrol`, `kdl_yetki_id`
   - Hardcoded kaynak tipi ID'leri: `KDL_KAYNAK_DOSYA=1`, vs.
   - İzin bit sabitleri: `KDL_IZIN_OKU=1`, vs.
8. **`test/test_capability.c`**: 35+ test, ASan temiz.
9. **Örnek**: `test/ornekler/yetki_temel.kem`, `yetki_confused_deputy.kem`.

---

## CP.10 — Spec Sınırları (V2'ye Bırakılanlar)

- **Persistent capabilities** (KeyKOS tarzı disk üzerinde yetki state).
- **Capability transfer over IPC** (R-KANAL + yetki kombinasyonu).
- **Hierarchical capability domain** (Genode tarzı parent-child sandbox).
- **Hardware-enforced** (Intel CET, ARM Pointer Authentication).
- **Capability garbage collection** (transitif unreachability —
  manual `geri_al` artık zorunlu, V1).
- **Audit log** (her yetki kullanımı kayda alma — ECC compliance için).
- **Dynamic resource type** (V1 hardcoded 5 tip, V2 plugin-style genişler).

---

## CP.11 — Tasarım Gerekçeleri (Niye Bu Yol?)

**Niye Unix POSIX capability (`CAP_SYS_*`) yetersiz?**
Linux `CAP_*` capability'leri ambient — bir process'in tüm thread'lerine
yayılır; başka process'e devredilemez (sadece exec ile inherit, ki SUID
benzeri); revocation yok. Object-capability'nin **token-bazlı** olması
gerekir.

**Niye Rust ownership yetersiz?**
Rust ownership Linear Types disipliniyle aynı — sızıntıyı engeller,
ama **ambient authority sorununu çözmez**. `File::open(path)` Rust'ta
da OS'tan ambient authority alır. Capability discipline ek bir disiplindir.

**Niye seL4 capability ABI'si direkt kullanılmadı?**
seL4 capability'leri kernel-internal cap-space tabanlı (cspace pointer +
slot). KEMGU userspace dilinde de kullanışlı olmalı (kernel ile aynı
syntax), bu yüzden token-bazlı (id + tip + izin) seçildi. Kernel-modunda
seL4-cspace ile bridge ileride.

**Niye `yetki<R>` ve `tekkez<yetki<R>>` ayrı katmanlar?**
Capability *delegate edilebilir* — bu, **non-linear** bir özellik. Linear
ise tek-kez. İkisini orthogonal tutmak gerek. `yetki<R>` linear (move-only)
ama delege ile çoğaltılabilir (alt-yetkilerle). `tekkez<yetki<R>>`
delege bile edilemez — tam tek-kullanımlık (OTP key gibi).

**Niye `geri_al` keyword yerine built-in çağrı?**
Sembolojik bütünlük: `kullan(t)`/`imha(t)` linear semantiğine paralel
`geri_al(y)`. Parser tablosu minimal. Gelecekte `geri_al` overload edilirse
(closure ile birlikte) sözdizimi değişmez.

**Niye `delege` keyword (özellikle Türkçe)?**
İngilizce literatür "delegate" — Türkçe "devret" daha doğal ama "devret"
zaten `YETKI_DEVRET` izin biti adı; AK olarak `delege` (Türkçe'ye
yerleşmiş hukuk terimi).

**Niye `yetki` keyword, `kapsam` veya `nesne` değil?**
"Kapsam" = scope (program scope); "nesne" = object (OOP). "yetki" =
authority/permission — capability'nin Türkçe doğal karşılığı.

---

## CP.12 — Hata Mesaj Şablonları (KEMGU Türkçe)

```
hata: CP001: kaynak erişimi için yetki<Dosya> parametresi eksik
  → işlev imzasına 'y: yetki<Dosya>' ekleyin

hata: CP002: yetki iptal edilmiş, kullanılamaz
  → 'geri_al(y)' çağrısından sonra y kullanıldı

hata: CP003: yetki izni yetersiz — gereken 'yaz', mevcut 'oku'
  → yetkiyi 'oku|yaz' izniyle yeniden oluşturun veya başka yetki kullanın

hata: CP004: yetki tipi uyumsuz — beklenen yetki<Dosya>, verilen yetki<Soket>

hata: CP005: linear yetki tüketilmedi (scope sonunda kullanılmış olmalı)
  → 'geri_al(y)' veya I/O çağrısı ile tüketin
```

---

## CP.13 — Direktif Hedef 1 Uyum Tablosu

| Direktif Hedef 1 Maddesi               | Capability Spec V1 Karşılığı       |
|----------------------------------------|-------------------------------------|
| Confused deputy önlenmesi              | CP-IO + capability parametre zorunlu |
| Ambient authority kaldırılması         | `yetki_olustur` sınırlı; userspace producer-only |
| Path TOCTOU önlenmesi                  | `dosya_ac_yetkili → yetki<Dosya>` handle |
| Privilege escalation engeli            | `delege` izin alt-küme zorunlu      |
| Side-channel mitigation                | (Sabitsüre Spec V1 ile combine)     |
| Compile-time enforcement               | CP001-CP005 hepsi compile-time      |
| Runtime check minimal                  | `yetki_kontrol` 16-byte + 2 comparison |

---

**END SPEC CP V1**
