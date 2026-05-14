# KEMGU Dil Rehberi

Bu belge KEMGU dilinin temel özelliklerini örnekle anlatır. Tutorial örnekler
[`test/ornekler/01_merhaba.kem`](../test/ornekler/) ile başlayan seride
sıralı olarak izlenebilir. Tam grammar için
[`KEMGU_Grammar_EBNF.md`](KEMGU_Grammar_EBNF.md).

> Burada anlatılan tüm özellikler `kemgu --check` ile doğrulanmıştır.

---

## İçindekiler

1. [Söz dizimi temelleri](#1-söz-dizimi-temelleri)
2. [Veri tipleri](#2-veri-tipleri)
3. [Değişkenler ve sabitler](#3-değişkenler-ve-sabitler)
4. [Kontrol akışı](#4-kontrol-akışı)
5. [İşlevler ve lambda](#5-i̇şlevler-ve-lambda)
6. [Yapılar ve generic](#6-yapılar-ve-generic)
7. [Seçimlik ve sonuç](#7-seçimlik-ve-sonuç)
8. [Bölge sistemi](#8-bölge-sistemi)
9. [Linear types — `tekkez<T>`](#9-linear-types--tekkezt)
10. [Özellik / uygula](#10-özellik--uygula)
11. [Modül sistemi](#11-modül-sistemi)
12. [Bit operatörleri](#12-bit-operatörleri)
13. [Güvensiz blok](#13-güvensiz-blok)

---

## 1. Söz dizimi temelleri

| Konsept                | KEMGU                              |
|------------------------|------------------------------------|
| Dosya uzantısı         | `.kem`                             |
| Kodlama                | Zorunlu UTF-8                      |
| İfade sonlandırıcı     | `;`                                |
| Satır yorumu           | `// ...`                           |
| Blok yorumu (iç içe ✓) | `/* ... */`                        |
| Sayı ayracı            | `1_000_000`                        |
| Ham metin              | `r#"...içinde " var..."#`          |

```kemgu
// Tek satır yorumu
/* Blok yorumu /* iç içe destekli */ */

işlev main() -> tam32 {
    değişken x = 1_000_000;       // sayı ayracı
    değişken s = r#"o "demiş""#;  // ham string
    ver 0;
}
```

33 anahtar kelime — sayı, kontrol akışı, tip, bölge, lineer:

```
eğer  değilse  için  iken  eşleş  ver  işlev  yapı  özellik  modül
değişken  sabit  doğru  yanlış  boş  ve  veya  değil  kullan  dışa
tamam  hata  bölge  uygula  kendin  seçimlik  sonuç  değer  hiç
güvensiz  tekkez  imha
```

---

## 2. Veri tipleri

### 2.1 İlkel tipler

| Kategori     | Tipler                                       |
|--------------|----------------------------------------------|
| İşaretli tam | `tam8`, `tam16`, `tam32`, `tam64`            |
| İşaretsiz tam| `dtam8`, `dtam16`, `dtam32`, `dtam64`        |
| Kesirli      | `kesirli32`, `kesirli64`                     |
| Mantıksal    | `mantıksal` (`doğru` / `yanlış`)             |
| Karakter     | `karakter` (Unicode kod noktası)             |
| Metin        | `metin` (UTF-8 dilim — null değil)           |
| Boş          | `boş` (`void` karşılığı)                     |

```kemgu
işlev main() -> tam32 {
    değişken kucuk: tam8 = 127;
    değişken buyuk: tam64 = 9_223_372_036_854_775_807;
    değişken dogru_mu: mantıksal = doğru;
    değişken c: karakter = 'Ö';
    değişken ad: metin = "Mehmet";
    ver 0;
}
```

> **Asla:** null pointer yok. `seçimlik<T>` kullan. Implicit conversion yok —
> `tam8` → `tam32` için açık `dönüştür` ifadesi (planlanmakta) veya
> bidirectional inference bağlamı gerek.

### 2.2 Bileşik tipler

| Tip                | Anlam                                  |
|--------------------|----------------------------------------|
| `&T`               | Değişmez referans (her yerde)          |
| `&değişken T`      | Değişebilir referans                   |
| `*T`               | Ham pointer (yalnız `güvensiz`)        |
| `Dizi<T>`          | Homojen dizi                           |
| `seçimlik<T>`      | Var / Yok (null değil)                 |
| `sonuç<T, H>`      | Başarı / Hata (istisna değil)          |
| `tekkez<T>`        | Lineer kaynak — bir kez tüketilir      |
| `işlev(T1) -> T2`  | İşlev tipi (parametre + dönüş)         |

```kemgu
yapı Nokta { x: tam32; y: tam32; }

işlev al(d: &Dizi<tam32>, i: tam32) -> seçimlik<tam32> {
    eğer i < 0 {
        ver hiç;
    }
    ver değer(d[i]);     // (henüz `değer(...)` ifade context'i kısıtlı)
}
```

### 2.3 Sabit-boyutlu vs platform-bağımsız

KEMGU **çıplak `int` yok**. Her sayı tipi bit genişliğini ister
(`tam8`, `tam16`, `tam32`, `tam64`). Bu C/C++'taki taşma sürprizlerini elimine eder.

---

## 3. Değişkenler ve sabitler

```kemgu
değişken x: tam32 = 42;     // mutable; tip annot opsiyonel
değişken y = 42;            // tip çıkarsanır (tam32)
sabit PI: kesirli64 = 3.14159; // immutable, modül seviyesi
```

**Bidirectional inference:**

```kemgu
işlev f(b: tam8) { /* ... */ }

işlev main() -> tam32 {
    f(5);                   // 5 burada tam8 olarak çıkarsanır (context-driven)
    değişken xs: Dizi<tam64> = [];   // boş dizi tipi context'ten geldi
    ver 0;
}
```

---

## 4. Kontrol akışı

### 4.1 `eğer` / `değilse`

```kemgu
işlev kategori(n: tam32) -> metin {
    eğer n < 0 {
        ver "negatif";
    } değilse eğer n == 0 {
        ver "sıfır";
    } değilse {
        ver "pozitif";
    }
}
```

### 4.2 `iken` (while)

```kemgu
işlev faktoryel(n: tam32) -> tam32 {
    değişken sonuc = 1;
    iken n > 1 {
        sonuc = sonuc * n;
        n = n - 1;
    }
    ver sonuc;
}
```

### 4.3 `için` (range/for)

```kemgu
işlev kare_toplam(xs: Dizi<tam32>) -> tam32 {
    değişken t = 0;
    için x: xs {        // dizi üzerinde iterasyon
        t = t + x * x;
    }
    ver t;
}
```

### 4.4 `eşleş` (pattern matching)

```kemgu
işlev ad_ver(o: seçimlik<metin>) -> metin {
    eşleş o {
        değer(s) => { ver s; }
        hiç      => { ver "isimsiz"; }
    }
    ver "ulaşılmaz";
}
```

Desen tipleri: `LITERAL`, `TANIMLAYICI` (bind), `JOKER` (`_`), `YAPICI` (yapı/enum).

---

## 5. İşlevler ve lambda

### 5.1 Temel işlev

```kemgu
işlev topla(a: tam32, b: tam32) -> tam32 {
    ver a + b;
}
```

Dönüş tipi yoksa boş (void) varsayılır:

```kemgu
işlev logla(s: metin) {
    /* boş dönüş */
}
```

### 5.2 Lambda

```kemgu
işlev main() -> tam32 {
    değişken iki_kat = |x: tam32| x * 2;
    değişken dort_kat = |x: tam32| { değişken y = x * 2; y * 2 };
    ver iki_kat(21);
}
```

### 5.3 Generic işlev (monomorphization)

```kemgu
işlev kimlik<T>(x: T) -> T {
    ver x;
}

işlev main() -> tam32 {
    ver kimlik(42);       // T = tam32 olarak instantiate
}
```

Generic işlevler çağrı sırasında her tip argümanı için ayrı bir fonksiyon
emit edilir — runtime dispatch maliyeti yok (Rust tarzı monomorphization).

### 5.4 Kısıtlı generic (bound)

```kemgu
özellik Sayilabilir {}
yapı Tam { x: tam32; }
uygula Sayilabilir için Tam {}

yapı Vektor<T: Sayilabilir> { ic: T; }
// Vektor<Tam> ✓     — Tam, Sayilabilir uyguluyor
// Vektor<metin> ✗   — T030: uygula bildirimi yok
```

---

## 6. Yapılar ve generic

### 6.1 Temel yapı

```kemgu
yapı Nokta {
    x: tam32;
    y: tam32;
}

işlev mesafe_kare(a: &Nokta, b: &Nokta) -> tam32 {
    değişken dx = a.x - b.x;
    değişken dy = a.y - b.y;
    ver dx * dx + dy * dy;
}

işlev main() -> tam32 {
    değişken n: Nokta = Nokta { x: 17, y: 25 };
    ver n.x + n.y;     // → 42
}
```

### 6.2 Generic yapı

```kemgu
yapı Kutu<T> {
    icerik: T;
    bos_mu: mantıksal;
}

işlev kutu_yarat<T>(deger: T) -> Kutu<T> {
    ver Kutu { icerik: deger, bos_mu: yanlış };
}
```

### 6.3 İç içe generic

```kemgu
yapı Cift<A, B> { ilk: A; ikinci: B; }

işlev demo() {
    değişken c = Cift { ilk: 1, ikinci: "ad" };
    /* c.ilk : tam32, c.ikinci : metin */
}
```

---

## 7. Seçimlik ve sonuç

### 7.1 `seçimlik<T>` — null'un yerini alır

```kemgu
işlev ilk_eleman(xs: &Dizi<tam32>) -> seçimlik<tam32> {
    eğer xs[0] == 0 { ver hiç; }     // örnek; gerçek "boş dizi" runtime bağımlı
    ver değer(xs[0]);
}

işlev main() -> tam32 {
    değişken d: Dizi<tam32> = [42, 7];
    eşleş ilk_eleman(&d) {
        değer(x) => { ver x; }
        hiç      => { ver 0; }
    }
    ver 0;
}
```

### 7.2 `sonuç<T, H>` — istisna'nın yerini alır

```kemgu
yapı KSonuc<T, H> {
    basarili: mantıksal;
    deger: T;
    hata_msg: H;
}

işlev bol(a: tam32, b: tam32) -> KSonuc<tam32, metin> {
    eğer b == 0 {
        ver KSonuc { basarili: yanlış, deger: 0, hata_msg: "böl sıfır" };
    }
    ver KSonuc { basarili: doğru, deger: a / b, hata_msg: "" };
}
```

> Built-in `sonuç<T, H>` pattern desteği parser fix bekliyor — bu yüzden
> şu an stdlib `KSonuc<T, H>` yapısını kullanır
> ([`stdlib/sonuc.kem`](../stdlib/sonuc.kem)).

---

## 8. Bölge sistemi

KEMGU GC kullanmaz. Bunun yerine **bölge tabanlı** (region-based) bellek
modeli vardır. Compiler her tahsisin hangi bölgeye ait olduğunu otomatik
çıkarsar — programcı annotation yazmaz.

### 8.1 Bölge ömrü hiyerarşisi (Katman 1)

```
ρ_iterasyon  <  ρ_yerel  <  ρ_çağıran  <  ρ_global
```

- `ρ_yerel(f)` — işlev gövdesinde tahsis, çağrı bittiğinde imha.
- `ρ_çağıran(f)` — `ver` ile dönen değer; çağıranın bölgesinde yaşar.
- `ρ_iterasyon(d)` — döngü gövdesi; bir tur biterken imha.
- `ρ_global` — program ömrü.

Compiler **escape analizi** (DFA + fixed-point) ile karar verir:

```kemgu
işlev iki_olustur() -> Kutu<tam32> {
    değişken k = Kutu { icerik: 42, bos_mu: yanlış };
    ver k;     // k escape ediyor → ρ_çağıran (otomatik)
}

işlev sadece_kullan() -> tam32 {
    değişken k = Kutu { icerik: 42, bos_mu: yanlış };
    ver k.icerik;     // k escape etmiyor → ρ_yerel
}
```

### 8.2 Concurrency (Katman 2 — iskelet hazır)

Her bölge **tek thread**'e aittir:

- `ρ_sahip(t)` — thread `t`'ye ait bölge.
- `ρ_kanal(k)` — kanal `k`'nın transfer tamponu.

Aksiyomlar:
- **R-GÖREV:** Görev yarattığında yeni bölge `ρ_sahip(yeni_thread)`.
- **R-BİRLEŞTİR:** Join sonrası transfer; sahip değişir.
- **R-KANAL:** Kanal üzerinden transfer; sahiplik el değiştirir.
- **R-PAYLAŞ:** İmmutable paylaşım (v1: yok).

> Lang syntax (`görev`, `kanal` anahtar kelimeleri) henüz parse edilmiyor;
> aksiyomlar `bolge.h` API'sinde mevcut.

---

## 9. Linear types — `tekkez<T>`

Linear Types Spec V1 — onaylı. Kaynak yönetimini *yapısal olarak* zorunlu kılar.

| Konsept            | Anlam                                                |
|--------------------|------------------------------------------------------|
| `tekkez<T>`        | Lineer tip — bir kez tüketilebilir                   |
| `tekkez_yarat<T>(e)` | Producer intrinsic — `T` → `tekkez<T>`             |
| `kullan(t)`        | Tüket + iç değeri al (`tekkez<T>` → `T`)             |
| `imha(t)`          | Tüket + at (`tekkez<T>` → `boş`)                     |

### 9.1 Temel kullanım

```kemgu
işlev tek_kez_kullan() -> tam32 {
    değişken k: tekkez<tam32> = tekkez_yarat(42);
    ver kullan(k);     // k tüketildi
    // ver kullan(k);  // ✗ L002: move sonrası kullanım
}

işlev imha_ornegi() {
    değişken k: tekkez<metin> = tekkez_yarat("merhaba");
    imha(k);           // değer atıldı
}
```

### 9.2 Move semantiği (ownership transfer)

```kemgu
işlev tuket(t: tekkez<tam32>) {
    imha(t);
}

işlev cagri_ornegi() {
    değişken k = tekkez_yarat(7);
    tuket(k);          // k → tuket'e devredildi
    // imha(k);        // ✗ L002
}
```

### 9.3 Sahiplik geri devri

```kemgu
işlev sahip_uret() -> tekkez<tam32> {
    ver tekkez_yarat(99);     // LR-4: ver ile çağırana devir
}
```

### 9.4 Hata sınıfları

| Kod   | Açıklama                                  |
|-------|-------------------------------------------|
| L001  | Lineer bağ tüketilmedi (bölge kapanışında)|
| L002  | Move sonrası kullanım                     |
| L004  | Lineer değer üzerinden referans yasak     |
| L007  | Consume operandı `tekkez<T>` değil        |
| L008  | `tekkez_yarat` parametre sayısı yanlış    |
| LR002 | Yapı/dizi alanı `tekkez<T>` içeremez (v1) |

Örnek hata sergileyen dosya: [`test/ornekler/lineer_hata.kem`](../test/ornekler/lineer_hata.kem).

### 9.5 Closure-itself-linear (LC-2)

Lambda lineer bir bağ yakaladığında, lambda'nın tipi otomatik
`tekkez<işlev(...)>` olur. Bu, "lineer kaynak yakalanmış closure tek kez
çağrılabilir" garantisini sağlar.

---

## 10. Özellik / uygula

KEMGU'da interface karşılığı **özellik** (trait), implementasyon
**uygula** ile yapılır.

```kemgu
özellik Sayilabilir {
    işlev say(kendin) -> tam32;   // (kendin parametresi v1'de kısıtlı)
}

yapı Liste { uzunluk: tam32; }

uygula Sayilabilir için Liste {
    işlev say(kendin) -> tam32 {
        ver kendin.uzunluk;
    }
}
```

### 10.1 Inherent impl (özellik yok)

```kemgu
yapı Nokta { x: tam32; y: tam32; }

uygula Nokta {
    işlev kare_toplam(kendin) -> tam32 {
        ver kendin.x * kendin.x + kendin.y * kendin.y;
    }
}
```

### 10.2 Generic uygula

```kemgu
yapı Kutu<T> { icerik: T; }

uygula<T> Kutu<T> {
    işlev ic(kendin) -> T { ver kendin.icerik; }
}
```

### 10.3 Bound enforcement

```kemgu
yapı Vektor<T: Sayilabilir> { ic: T; }

sabit V: Vektor<Liste> = ...;     // ✓
// sabit X: Vektor<metin> = ...;  // ✗ T030: metin Sayilabilir uygulamıyor
```

---

## 11. Modül sistemi

```kemgu
modül matematik {
    dışa işlev mutlak(x: tam32) -> tam32 {
        eğer x < 0 { ver 0 - x; }
        ver x;
    }
}

kullan matematik;     // top-level: import bildirimi
// matematik::mutlak(-5) çağrılabilir.
```

`dışa` ile işaretlenenler modül dışından erişilir. İşaretlenmeyen tanımlar
modül-içi kalır.

> **Not:** Çoklu dosya `kullan` import'u henüz tek dosya derleme aşamasında —
> stdlib şu an `cat lib test > combined.kem && kemgu --check combined.kem`
> şeklinde birleştirilerek doğrulanır ([`Makefile`](../Makefile) →
> `calistir_stdlib_check`).

---

## 12. Bit operatörleri

`tam8` … `tam64` ve `dtam8` … `dtam64` üzerinde:

| Op    | Anlam              |
|-------|--------------------|
| `&`   | Bit AND            |
| `\|`  | Bit OR             |
| `^`   | Bit XOR            |
| `~`   | Bit NOT (tekli)    |
| `<<`  | Sola kaydır        |
| `>>`  | Sağa kaydır        |

```kemgu
işlev sayfa_giris(adres: tam64, izinler: tam64) -> tam64 {
    ver (adres << 12) | (izinler & 4095);
}

işlev sayfa_izin(giris: tam64) -> tam64 {
    ver giris & 4095;
}
```

Tam çalışan örnek: [`test/ornekler/kernel.kem`](../test/ornekler/kernel.kem) —
ARM64 bare-metal cross-compile testi.

> Generic parser şu an `Dizi<seçimlik<T>>` gibi `>>` ile biten generic
> kapanışları `parser_buyuk_ayir` ile iki `>` token'ına böler. Yani
> sağa-kaydır `>>` ile generic kapanış çakışmaz.

---

## 13. Güvensiz blok

KEMGU'nun `güvensiz` bloğu — Rust'taki `unsafe` ile aynı role sahip ama
**asla normalleştirilmez**. Yalnız buradan:
- Ham `*T` pointer ile çalışılır,
- C ABI FFI çağrıları yapılır,
- Compiler'ın doğrulayamadığı invariant'lar elle taahhüt edilir.

```kemgu
işlev tehlikeli() {
    güvensiz [etiket: "C ABI köprüsü"] {
        /* *T dereferansı yalnızca burada legal */
    }
}
```

`[etiket: "..."]` opsiyonel açıklama; code review'da bu blokların *neden*
güvensiz olduğunu görmek için.

---

## 14. Kripto stdlib (constant-time)

KEMGU stdlib'de `kripto` modülü — yan kanal saldırılarına karşı sabit-süre
garantili kripto temel taşları. Sabitsüre Spec V1 ile sıkı tip uyumu.

### 14.1 Modül haritası

```
stdlib/
├── kripto.kem               — Base CT primitives
└── kripto/
    ├── karma.kem            — SHA-256 + BLAKE3 placeholder
    ├── sifre.kem            — ChaCha20 + Poly1305 + AEAD
    ├── rastgele.kem         — xorshift64 + xoshiro256 + OS RNG iskelet
    └── anahtar.kem          — tekkez<SimetrikAnahtar256> + HKDF + OTP
```

### 14.2 Base API (kripto.kem)

```kemgu
// Sabit-süre eşitlik — XOR-toplama + zeroness; OpenSSL CRYPTO_memcmp eşdeğeri
işlev sabit_süre_eşit_blok(
    a: Dizi<sabitsüre<dtam8>>,
    b: Dizi<sabitsüre<dtam8>>,
    uzunluk: tam32
) -> sabitsüre<mantıksal>

// Branchless seçim — LLVM `select` opcode; V1: bitmask
işlev sabit_süre_seç_u32(
    maske: sabitsüre<dtam32>,   // 0xFFFFFFFF (true) veya 0 (false)
    t: sabitsüre<dtam32>,
    f: sabitsüre<dtam32>
) -> sabitsüre<dtam32>

// XOR (CT-safe; ^ ile eşdeğer ama isim semantik)
işlev sabit_süre_xor_blok(
    a: Dizi<sabitsüre<dtam8>>,
    b: Dizi<sabitsüre<dtam8>>,
    sonuc: Dizi<sabitsüre<dtam8>>,
    n: tam32
) -> tam32
```

Tipik kullanım:

```kemgu
// HMAC tag karşılaştırma — `if tag != computed` deseni TIMING SALDIRISIDIR
değişken sonuc: sabitsüre<mantıksal> = sabit_süre_eşit_blok(
    hesaplanan_tag, beklenen_tag, 16
);
// Dallanmak için ifşa zorunlu:
eğer ifşa(sonuc) { /* tag eşleşti, decrypt'e devam */ }
```

### 14.3 Hash (karma.kem)

SHA-256 (FIPS 180-4) sabit-süre referans:

```kemgu
işlev sha256_blok_sikistir(
    state: Dizi<sabitsüre<dtam32>>,    // h0..h7
    blok_w: Dizi<sabitsüre<dtam32>>    // 16 x dtam32 (512-bit message block)
) -> tam32
```

Σ0, Σ1, σ0, σ1, Ch, Maj fonksiyonları branchless. Round constants K[64]
ve initial values H0[8] PUBLIC (RFC standartı).

### 14.4 ChaCha20-Poly1305 AEAD (sifre.kem)

RFC 8439 — Quarter Round + 20 round + Poly1305 MAC + AEAD construction:

```kemgu
işlev aead_chacha20_poly1305_sifrele(
    key: Dizi<sabitsüre<dtam32>>,
    nonce: Dizi<sabitsüre<dtam32>>,
    aad: Dizi<sabitsüre<dtam32>>, aad_uzunluk: tam32,
    plaintext: Dizi<sabitsüre<dtam32>>, plaintext_uzunluk: tam32,
    ciphertext: Dizi<sabitsüre<dtam32>>,
    tag: Dizi<sabitsüre<dtam32>>
) -> tam32

işlev aead_chacha20_poly1305_dogrula(
    ...
) -> sabitsüre<mantıksal>     // tag mismatch sabitsüre — programcı ifşa edilmesin
```

Niye AES değil? AES T-table cache-timing (Bernstein 2005); ChaCha20 sadece
ADD/XOR/ROTL, her arşitektürde uniform.

### 14.5 Anahtar yönetimi (anahtar.kem)

`tekkez<SimetrikAnahtar256>` linear types + sabitsüre iç içe:

```kemgu
// Anahtar yaratma — tekkez sarmalama
değişken a: tekkez<SimetrikAnahtar256> = anahtar_yarat(govde);

// Tek-kullanım: kullan veya imha
otp_sifrele(metin, a, sifreli, n);   // `kullan(a)` içeride — sonrası L002
// a'ya buradan erişim derleme hatası
```

HKDF (RFC 5869) iskeleti:

```kemgu
işlev anahtar_turet(
    ikm: Dizi<sabitsüre<dtam8>>, ikm_uzunluk: tam32,
    salt: Dizi<sabitsüre<dtam8>>, salt_uzunluk: tam32,
    info: Dizi<sabitsüre<dtam8>>, info_uzunluk: tam32,
    sonuc_govde: Dizi<sabitsüre<dtam8>>  // 32-byte
) -> tam32
```

### 14.6 Rastgele sayı (rastgele.kem)

Platform RNG iskeleti (V2'de syscall):
- Linux: getrandom(2)
- Windows: BCryptGenRandom
- macOS: getentropy(3)

V1: xorshift64 + xoshiro256 fallback (kripto için **kullanma**, sadece test):

```kemgu
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32
```

Hardware RNG (RDRAND, ARMv8.5 RNDR) için placeholder yorum — yeni
built-in opcode V2'de.

### 14.7 Örnek: ChaCha20-Poly1305 ile dosya şifreleme

[`test/ornekler/sifrele_dosya.kem`](../test/ornekler/sifrele_dosya.kem)

Akış:
1. Parola → HKDF → 32-byte anahtar
2. Rastgele nonce (12 bayt)
3. AEAD encrypt (ChaCha20 + Poly1305)
4. Authenticated decrypt — tag mismatch'te `sabitsüre<mantıksal>` yanlış

### 14.8 Test

Toplam **67 test** (3 dosya, hepsi `--check` ile geçer):

| Dosya | Test# | Konu |
|-------|-------|------|
| `test_kripto.kem`        | 42 | K1-K6: base + her submodül + bundle entegrasyon |
| `test_kripto_vektor.kem` | 8  | NIST SHA-256, RFC 8439 ChaCha20, RFC 5869 HKDF |
| `test_kripto_timing.kem` | 17 | CT compile-time pozitif (taint, mask, generic, linear) |

```bash
make calistir_kripto_check       # yalnız kripto bundle
make calistir_stdlib_check       # tüm stdlib (kripto dahil)
```

### 14.9 Sınırlamalar (V1)

- HKDF/HMAC tam implementasyon V2 (karma.kem ile birleşik tek pipeline)
- Poly1305 reduction stub — V2: Barrett/Montgomery 130-bit mul
- Dosya I/O yok — `kemgu --llvm` ile derleme yapılır ama gerçek I/O syscall
- Hardware RNG placeholder — yeni built-in opcode V2'de
- Modül import yok — şu an bundle yaklaşımı (Makefile concat)

---

## Devam

- Tutorial: [`test/ornekler/01_merhaba.kem`](../test/ornekler/) → `09_arm64.kem`.
- Mimari: [`MIMARI.md`](MIMARI.md).
- Onboarding: [`BASLAMAK.md`](BASLAMAK.md).
- Stdlib: [`../stdlib/README.md`](../stdlib/README.md).
