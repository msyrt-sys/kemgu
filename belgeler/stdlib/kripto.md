# Kripto Modülü (`stdlib/kripto`)

KEMGU kripto modülü, yan kanal (side-channel) saldırılarına karşı **sabit-süre (constant-time)** garantili kriptografik temel taşları sağlar. Tüm gizli veriler `sabitsüre<T>` qualifier'ı ile işaretlenir; bu sayede derleyici (Sabitsüre Spec V1 — `belgeler/KEMGU_Sabitsure_Spec_V1.md`) gizli veriye dallanma veya gizli-indeksli erişim gibi zamanlama sızıntılarını **derleme zamanında** reddeder (CT001/CT002). Modül beş dosyaya bölünmüştür: temel CT primitifleri (`kripto.kem`), anahtar yönetimi (`anahtar.kem` — linear `tekkez<T>` ile), karma/hash fonksiyonları (`karma.kem` — SHA-256), güvenli rastgele üretim (`rastgele.kem`) ve simetrik şifreleme (`sifre.kem` — ChaCha20-Poly1305).

Tasarım, gerçek kripto-yazılım CVE deneyiminden çıkarılmıştır: OpenSSL `CRYPTO_memcmp` (tipte zorlama yok), Go `crypto/subtle` (opt-in, CVE-2023-39532), Rust `subtle` (trait, sessiz compile) yerine KEMGU sabit-süre eşitliği **mandatory** kılar. AES'in S-box / T-table cache-timing sorununu (Bernstein 2005) tamamen elemek için simetrik şifre olarak ADD/XOR/ROTL tabanlı ChaCha20 seçilmiştir.

**Genel v1 sınırlamaları:**
- Tüm dosyalar **standalone** derlenir; import sistemi henüz yok (CLAUDE.md tek-dosya derleme), bu yüzden `rotr`/`rotl` gibi CT yardımcıları her dosyada yeniden tanımlanır (`_karma_`, `_sifre_`, `_rastgele_` önekleriyle).
- HKDF, HMAC-SHA256, BLAKE3, Poly1305 tam-modüler-azaltma ve AEAD framing **placeholder/iskelet** (V1). Üretim için kullanılmamalıdır.
- Güvenli rastgele üretim syscall (getrandom / BCryptGenRandom) altyapısı henüz yok; V1 yalnız `xorshift64` fallback sunar — **kripto güvensiz, sadece test**.
- Bellek zeroize (`imha` sonrası gerçek sıfırlama) V2'ye ertelenmiştir.
- `sabitsüre_olustur(x)` özel bir built-in producer'dır: düz bir değeri `sabitsüre<T>` sarmalına alır.

---

## `kripto.kem` — Sabit-Süre Temel Primitifleri

Branchless eşitlik, seçim (select), XOR ve döngüsel kaydırma temel taşları. Tüm gizli operasyonlar dallanmasızdır; gizli `sabitsüre<mantıksal>` sonucundan dallanmak için programcının açıkça `ifşa(...)` çağırması gerekir (CT001).

### sabit_süre_eşit_blok

```kemgu
işlev sabit_süre_eşit_blok(
    a: Dizi<sabitsüre<dtam8>>,
    b: Dizi<sabitsüre<dtam8>>,
    uzunluk: tam32
) -> sabitsüre<mantıksal>
```

**Ne yapar:** İki bayt dizisini sabit-sürede karşılaştırır. `a[i] ^ b[i]` farklarını OR-toplar; birikim 0 ise diziler eşittir. OpenSSL `CRYPTO_memcmp` / libsodium `sodium_memcmp` ile eşdeğer; HMAC tag veya parola hash karşılaştırması için zorunludur.

**Örnek:**
```kemgu
// İki MAC etiketini sabit-sürede karşılaştır
değişken esit: sabitsüre<mantıksal> = sabit_süre_eşit_blok(tag_a, tag_b, 16);
// esit gizli; dallanmak için: eğer ifşa(esit) { ... }
```

**Kenar durum:** `uzunluk = 0` ise döngü hiç çalışmaz, birikim 0 kalır ve sonuç `doğru` (boş diziler eşit kabul edilir). Karşılaştırma süresi yalnız `uzunluk`'a bağlıdır (public), içeriğe değil — erken çıkış yoktur.

### sabit_süre_eşit_bayt

```kemgu
işlev sabit_süre_eşit_bayt(a: sabitsüre<dtam8>, b: sabitsüre<dtam8>) -> sabitsüre<mantıksal>
```

**Ne yapar:** Tek bayt eşitliği. `sabitsüre` üzerindeki `==` operatörü zaten CT garantili olduğu için doğrudan `a == b` döner.

**Örnek:**
```kemgu
değişken e: sabitsüre<mantıksal> = sabit_süre_eşit_bayt(x, y);  // CT karşılaştırma
```

**Kenar durum:** Yok — tek instruction; süre operand değerinden bağımsız.

### sabit_süre_eşit_u32

```kemgu
işlev sabit_süre_eşit_u32(a: sabitsüre<dtam32>, b: sabitsüre<dtam32>) -> sabitsüre<mantıksal>
```

**Ne yapar:** 32-bit kelime sabit-süre eşitliği (`a == b`).

**Örnek:**
```kemgu
değişken e: sabitsüre<mantıksal> = sabit_süre_eşit_u32(word_a, word_b);
```

**Kenar durum:** Yok — sabit-süre `==`.

### sabit_süre_eşit_u64

```kemgu
işlev sabit_süre_eşit_u64(a: sabitsüre<dtam64>, b: sabitsüre<dtam64>) -> sabitsüre<mantıksal>
```

**Ne yapar:** 64-bit kelime sabit-süre eşitliği (`a == b`).

**Örnek:**
```kemgu
değişken e: sabitsüre<mantıksal> = sabit_süre_eşit_u64(a64, b64);
```

**Kenar durum:** Yok — sabit-süre `==`.

### sabit_süre_seç_u8

```kemgu
işlev sabit_süre_seç_u8(
    maske: sabitsüre<dtam8>,
    t: sabitsüre<dtam8>,
    f: sabitsüre<dtam8>
) -> sabitsüre<dtam8>
```

**Ne yapar:** Branchless seçim. `maske` tüm-bir (0xFF) ise `t`, sıfır (0x00) ise `f` döner — `(t & maske) | (f & ~maske)`. Dallanmasız `if-then-else` yerine geçer.

**Örnek:**
```kemgu
// maske 0xFF → secilen = t; maske 0x00 → secilen = f
değişken secilen: sabitsüre<dtam8> = sabit_süre_seç_u8(maske, dogru_deger, yanlis_deger);
```

**Kenar durum:** Maske yalnız 0x00 veya 0xFF olmalıdır; ara değerler t ve f'in bit karışımını verir (tanımsız davranış değil ama anlamsız sonuç). Maske `sabit_süre_diff_mask_u8` ile üretilebilir. V2'de `sabitsüre_seç` built-in olacak ve maske kullanıcı tarafından hesaplanmayacak (Spec CT.9).

### sabit_süre_seç_u32

```kemgu
işlev sabit_süre_seç_u32(
    maske: sabitsüre<dtam32>,
    t: sabitsüre<dtam32>,
    f: sabitsüre<dtam32>
) -> sabitsüre<dtam32>
```

**Ne yapar:** 32-bit branchless seçim. Maske 0xFFFFFFFF → `t`, 0x00000000 → `f`.

**Örnek:**
```kemgu
değişken secilen: sabitsüre<dtam32> = sabit_süre_seç_u32(maske32, t32, f32);
```

**Kenar durum:** Maske all-ones veya all-zero olmalı; ara maskeler bit karışımı verir.

### sabit_süre_seç_u64

```kemgu
işlev sabit_süre_seç_u64(
    maske: sabitsüre<dtam64>,
    t: sabitsüre<dtam64>,
    f: sabitsüre<dtam64>
) -> sabitsüre<dtam64>
```

**Ne yapar:** 64-bit branchless seçim. Maske 0xFFFFFFFFFFFFFFFF → `t`, 0 → `f`.

**Örnek:**
```kemgu
değişken secilen: sabitsüre<dtam64> = sabit_süre_seç_u64(maske64, t64, f64);
```

**Kenar durum:** Maske all-ones veya all-zero olmalı; ara maskeler bit karışımı verir.

### sabit_süre_xor_u8

```kemgu
işlev sabit_süre_xor_u8(a: sabitsüre<dtam8>, b: sabitsüre<dtam8>) -> sabitsüre<dtam8>
```

**Ne yapar:** Sabit-süre XOR (`a ^ b`). Built-in `^` ile eşdeğerdir; ad, kripto kodunda semantik niyet (CT-safe XOR) taşımak için sağlanır.

**Örnek:**
```kemgu
değişken keystream_xor: sabitsüre<dtam8> = sabit_süre_xor_u8(byte, key_byte);
```

**Kenar durum:** Yok — saf XOR; süre operandlardan bağımsız.

### sabit_süre_xor_u32

```kemgu
işlev sabit_süre_xor_u32(a: sabitsüre<dtam32>, b: sabitsüre<dtam32>) -> sabitsüre<dtam32>
```

**Ne yapar:** 32-bit sabit-süre XOR (`a ^ b`).

**Örnek:**
```kemgu
değişken w: sabitsüre<dtam32> = sabit_süre_xor_u32(a32, b32);
```

**Kenar durum:** Yok.

### sabit_süre_xor_u64

```kemgu
işlev sabit_süre_xor_u64(a: sabitsüre<dtam64>, b: sabitsüre<dtam64>) -> sabitsüre<dtam64>
```

**Ne yapar:** 64-bit sabit-süre XOR (`a ^ b`).

**Örnek:**
```kemgu
değişken w: sabitsüre<dtam64> = sabit_süre_xor_u64(a64, b64);
```

**Kenar durum:** Yok.

### sabit_süre_xor_blok

```kemgu
işlev sabit_süre_xor_blok(
    a: Dizi<sabitsüre<dtam8>>,
    b: Dizi<sabitsüre<dtam8>>,
    sonuc: Dizi<sabitsüre<dtam8>>,
    n: tam32
) -> tam32
```

**Ne yapar:** Blok XOR — `sonuc[i] = a[i] ^ b[i]` (i = 0..n). CTR/CFB modlarının ve ChaCha20 keystream XOR'unun temel operasyonu. Her zaman `0` döner.

**Örnek:**
```kemgu
// metni keystream ile XOR'la (sifrele)
sabit_süre_xor_blok(metin, keystream, sifreli, 32);
```

**Kenar durum:** `n = 0` ise hiçbir bayt yazılmaz, `sonuc` değişmez. `sonuc` dizisinin en az `n` elemana sahip olduğu varsayılır (sınır kontrolü runtime'da heap-uniform dizi erişiminde yapılır).

### sabit_süre_diff_mask_u8

```kemgu
işlev sabit_süre_diff_mask_u8(a: sabitsüre<dtam8>, b: sabitsüre<dtam8>) -> sabitsüre<dtam8>
```

**Ne yapar:** `sabitsüre<mantıksal>` → `sabitsüre<dtam8>` mask dönüşümü manuel olamadığı için (mantıksal sayısal değil — E002), bit hilesiyle fark maskesi üretir. `a` ile `b` **farklıysa** 0xFF, **eşitse** 0x00 döner. `a ^ b` farkı OR-reduce edilip LSB'ye indirilir, ardından `0 - (d & 1)` ile wrap-around yapılır. `sabit_süre_seç_*` ile doğrudan birleştirilebilir.

**Örnek:**
```kemgu
değişken maske: sabitsüre<dtam8> = sabit_süre_diff_mask_u8(x, y);
// x != y → maske = 0xFF; x == y → maske = 0x00
değişken r: sabitsüre<dtam8> = sabit_süre_seç_u8(maske, deger_farkli, deger_esit);
```

**Kenar durum:** `a == b` → çıktı 0; `a != b` → çıktı 0xFF (ara değer yok). Sonuç maskenin tersi `sabit_süre_eşit_*` semantiğine karşılık gelir (bu fonksiyon "diff" yani fark maskesi üretir).

### sabit_süre_diff_mask_u32

```kemgu
işlev sabit_süre_diff_mask_u32(a: sabitsüre<dtam32>, b: sabitsüre<dtam32>) -> sabitsüre<dtam32>
```

**Ne yapar:** 32-bit fark maskesi. `a != b` → 0xFFFFFFFF, `a == b` → 0. OR-reduce 16/8/4/2/1 kaydırma adımlarıyla tüm bitleri LSB'ye toplar.

**Örnek:**
```kemgu
değişken maske: sabitsüre<dtam32> = sabit_süre_diff_mask_u32(w1, w2);
```

**Kenar durum:** `a == b` → 0; `a != b` → all-ones. Ara değer yok.

### rotr_u32

```kemgu
işlev rotr_u32(x: sabitsüre<dtam32>, n: tam32) -> sabitsüre<dtam32>
```

**Ne yapar:** 32-bit sağa döngüsel kaydırma (rotate-right): `(x >> n) | (x << (32 - n))`. SHA-256 ve ChaCha20 quarter-round için temel ilkel.

**Örnek:**
```kemgu
değişken r: sabitsüre<dtam32> = rotr_u32(x, 7);  // 7 bit sağa döndür
```

**Kenar durum:** V1'de kaydırma miktarı `n` **public sabit** olmalıdır (gizli `n` ile kaydırma CT008 ihlali). `n` 0 ile 31 aralığında olmalıdır; `n = 0` ise `32 - n = 32` kaydırması tanımsızdır (kaçınılmalı), `n = 32` benzer şekilde sorunludur.

### rotl_u32

```kemgu
işlev rotl_u32(x: sabitsüre<dtam32>, n: tam32) -> sabitsüre<dtam32>
```

**Ne yapar:** 32-bit sola döngüsel kaydırma (rotate-left): `(x << n) | (x >> (32 - n))`.

**Örnek:**
```kemgu
değişken r: sabitsüre<dtam32> = rotl_u32(d, 16);  // ChaCha20 QR adımı
```

**Kenar durum:** `n` public sabit, 1..31 aralığında olmalı (`n = 0` veya `n = 32` tanımsız kaydırma yaratır).

### rotr_u64

```kemgu
işlev rotr_u64(x: sabitsüre<dtam64>, n: tam32) -> sabitsüre<dtam64>
```

**Ne yapar:** 64-bit sağa döngüsel kaydırma: `(x >> n) | (x << (64 - n))`.

**Örnek:**
```kemgu
değişken r: sabitsüre<dtam64> = rotr_u64(x, 28);
```

**Kenar durum:** `n` public sabit, 1..63 aralığında olmalı (`n = 0` veya `n = 64` tanımsız).

### KRIPTO_VERSIYON

```kemgu
sabit KRIPTO_VERSIYON: tam32 = 1;
```

Modül sürüm sabiti. Şu an `1`.

---

## `karma.kem` — Karma/Hash Fonksiyonları (SHA-256)

FIPS 180-4 SHA-256 sabit-süre referans implementasyonu (tek-blok). Round sabitleri `K` ve başlangıç değerleri `H0` **public** (standardize, RFC 6234); working state ve message schedule `sabitsüre<dtam32>`. ROTR ve mantıksal operasyonlar dallanmasız. BLAKE3, HMAC-SHA256 ve HKDF tam implementasyonu V2'ye ertelenmiştir. Bu modül standalone olduğu için CT yardımcılarını `_karma_` önekiyle yeniden tanımlar.

### _karma_rotr_u32 (yardımcı)

```kemgu
işlev _karma_rotr_u32(x: sabitsüre<dtam32>, n: tam32) -> sabitsüre<dtam32>
```

**Ne yapar:** Modül-içi 32-bit rotate-right (`kripto.kem`'deki `rotr_u32` ile aynı; import yokluğu için kopyalanmış). SHA-256 sigma fonksiyonlarının temeli.

**Örnek:**
```kemgu
değişken r: sabitsüre<dtam32> = _karma_rotr_u32(x, 13);
```

**Kenar durum:** `n` public sabit, 1..31 aralığında olmalı.

### _karma_sigma0 (yardımcı)

```kemgu
işlev _karma_sigma0(x: sabitsüre<dtam32>) -> sabitsüre<dtam32>
```

**Ne yapar:** SHA-256 büyük Σ0 fonksiyonu: `(x ROTR 2) ^ (x ROTR 13) ^ (x ROTR 22)`. Round içinde `T2` hesabında kullanılır.

**Örnek:**
```kemgu
değişken s0: sabitsüre<dtam32> = _karma_sigma0(a);
```

**Kenar durum:** Yok — sabit kaydırma miktarları gömülü.

### _karma_sigma1 (yardımcı)

```kemgu
işlev _karma_sigma1(x: sabitsüre<dtam32>) -> sabitsüre<dtam32>
```

**Ne yapar:** SHA-256 büyük Σ1 fonksiyonu: `(x ROTR 6) ^ (x ROTR 11) ^ (x ROTR 25)`. Round içinde `T1` hesabında kullanılır.

**Örnek:**
```kemgu
değişken s1: sabitsüre<dtam32> = _karma_sigma1(e);
```

**Kenar durum:** Yok.

### _karma_sigma0_low (yardımcı)

```kemgu
işlev _karma_sigma0_low(x: sabitsüre<dtam32>) -> sabitsüre<dtam32>
```

**Ne yapar:** SHA-256 küçük σ0 fonksiyonu: `(x ROTR 7) ^ (x ROTR 18) ^ (x SHR 3)`. Message schedule (W[16..63]) genişletmesinde kullanılır. Son terim mantıksal sağa kaydırma (SHR), rotate değil.

**Örnek:**
```kemgu
değişken s0: sabitsüre<dtam32> = _karma_sigma0_low(W_im15);
```

**Kenar durum:** Yok.

### _karma_sigma1_low (yardımcı)

```kemgu
işlev _karma_sigma1_low(x: sabitsüre<dtam32>) -> sabitsüre<dtam32>
```

**Ne yapar:** SHA-256 küçük σ1 fonksiyonu: `(x ROTR 17) ^ (x ROTR 19) ^ (x SHR 10)`. Message schedule genişletmesinde kullanılır.

**Örnek:**
```kemgu
değişken s1: sabitsüre<dtam32> = _karma_sigma1_low(W_im2);
```

**Kenar durum:** Yok.

### _karma_ch (yardımcı)

```kemgu
işlev _karma_ch(x: sabitsüre<dtam32>, y: sabitsüre<dtam32>, z: sabitsüre<dtam32>) -> sabitsüre<dtam32>
```

**Ne yapar:** SHA-256 Ch (choose) fonksiyonu — branchless multiplexer: `(x & y) ^ (~x & z)`. `sabitsüre` üzerinde bitwise NOT operatörü olmadığından `~x` yerine `x ^ 0xFFFFFFFF` kullanılır.

**Örnek:**
```kemgu
değişken ch: sabitsüre<dtam32> = _karma_ch(e, f, g);
```

**Kenar durum:** Yok — tamamen aritmetik/bitwise, dallanmasız.

### _karma_maj (yardımcı)

```kemgu
işlev _karma_maj(x: sabitsüre<dtam32>, y: sabitsüre<dtam32>, z: sabitsüre<dtam32>) -> sabitsüre<dtam32>
```

**Ne yapar:** SHA-256 Maj (majority) fonksiyonu — branchless: `(x & y) ^ (x & z) ^ (y & z)`.

**Örnek:**
```kemgu
değişken maj: sabitsüre<dtam32> = _karma_maj(a, b, c);
```

**Kenar durum:** Yok.

### sha256_K

```kemgu
işlev sha256_K() -> Dizi<dtam32>
```

**Ne yapar:** SHA-256 round sabitleri `K[0..63]` (FIPS 180-4 §4.2.2) dizisini döner. Bu değerler **public**; `sabitsüre` değil (`dtam32`).

**Örnek:**
```kemgu
değişken K: Dizi<dtam32> = sha256_K();
değişken k0: dtam32 = K[0];  // 0x428A2F98
```

**Kenar durum:** Her zaman 64 elemanlı sabit dizi döner.

### sha256_H0

```kemgu
işlev sha256_H0() -> Dizi<dtam32>
```

**Ne yapar:** SHA-256 başlangıç hash değerleri `H0[0..7]` (FIPS 180-4 §5.3.3) dizisini döner. Public.

**Örnek:**
```kemgu
değişken H0: Dizi<dtam32> = sha256_H0();  // 0x6A09E667, ... (8 değer)
```

**Kenar durum:** Her zaman 8 elemanlı sabit dizi döner.

### sha256_blok_sikistir

```kemgu
işlev sha256_blok_sikistir(
    state: Dizi<sabitsüre<dtam32>>,
    blok_w: Dizi<sabitsüre<dtam32>>
) -> tam32
```

**Ne yapar:** Tek 512-bit blok (16 dtam32 = 64 bayt) için SHA-256 sıkıştırma fonksiyonu (FIPS 180-4 §6.2.2). Message schedule W[64]'ü inşa eder, 64 round çalıştırır ve `state` dizisini (8 working variable a..h) yerinde günceller: `state[i] += a..h`. Tüm aritmetik sabit-süre, dallanma yok (round sayacı public). Her zaman `0` döner.

**Örnek:**
```kemgu
// state H0 ile başlatılmış 8 dtam32; blok mesajın 16 kelimesi
sha256_blok_sikistir(state, blok_w);
// state artık tek-blok hash sonucunu içerir
```

**Kenar durum:** `state` tam 8 eleman, `blok_w` tam 16 eleman içermelidir. Yalnız **tek blok** işler; çok bloklu streaming API V1'de yok (V2). K[i] erişimi public indeks `i` ile (CT002 ihlali değil).

### blake3_init

```kemgu
işlev blake3_init() -> Dizi<sabitsüre<dtam32>>
```

**Ne yapar:** **Placeholder.** BLAKE3 başlangıç state'i (8 IV; SHA-256 H0 ile aynı değerler) döner. Tam BLAKE3 (compression function, tree mode, XOF) V2'ye ertelenmiştir.

**Örnek:**
```kemgu
değişken state: Dizi<sabitsüre<dtam32>> = blake3_init();  // 8 sabitsüre<dtam32> IV
```

**Kenar durum:** Yalnız API yüzeyi (8 elemanlı IV dizisi); gerçek BLAKE3 sıkıştırması yapmaz.

### hmac_pad_uret

```kemgu
işlev hmac_pad_uret(
    anahtar: Dizi<sabitsüre<dtam8>>,
    anahtar_uzunluk: tam32,
    inner: Dizi<sabitsüre<dtam8>>,
    outer: Dizi<sabitsüre<dtam8>>
) -> tam32
```

**Ne yapar:** HMAC (RFC 2104) iç/dış pad'lerini üretir: `inner[i] = anahtar[i] ^ 0x36` (ipad), `outer[i] = anahtar[i] ^ 0x5C` (opad). Tam HMAC pipeline (`SHA256(K_outer || SHA256(K_inner || message))`) V2'dedir; bu fonksiyon yalnız XOR-pad kısmını yapar. Her zaman `0` döner.

**Örnek:**
```kemgu
hmac_pad_uret(anahtar, 32, inner_pad, outer_pad);
// inner_pad, outer_pad artık K⊕ipad ve K⊕opad içerir
```

**Kenar durum:** Yalnız `anahtar_uzunluk` baytı işlenir; anahtar block-size'a (64 bayt SHA-256) padding/hash V2'de yapılır. `inner` ve `outer` en az `anahtar_uzunluk` elemana sahip olmalıdır.

---

## `rastgele.kem` — Güvenli Rastgele Üretim

Kriptografik güvenli rastgele üretim hedefi; platform OS RNG'sini (Linux getrandom, Windows BCryptGenRandom, macOS getentropy) kullanmayı amaçlar. **V1'de syscall/FFI altyapısı henüz yok**, bu yüzden CSPRNG fonksiyonları `xorshift64` fallback ile stub'lanmıştır — **kripto güvensiz, sadece test/compile için**. Donanım RNG (RDRAND, ARMv8.5 RNG) V2 intrinsic olarak planlanmıştır.

### xorshift64_adim

```kemgu
işlev xorshift64_adim(state: dtam64) -> dtam64
```

**Ne yapar:** Marsaglia 2003 xorshift64 PRNG'nin tek adımı: `x ^= x<<13; x ^= x>>7; x ^= x<<17`. Period 2^64−1, uniform dağılım. State **public** (`sabitsüre` değil) — deterministik PRNG. **Kripto için ASLA kullanılmamalıdır** (öngörülebilir).

**Örnek:**
```kemgu
değişken sonraki: dtam64 = xorshift64_adim(state);
```

**Kenar durum:** `state = 0` ise zincir çöker (0'da takılır) — state sıfırdan farklı olmalıdır.

### xorshift64_pool

```kemgu
işlev xorshift64_pool(state_pool: Dizi<dtam64>, sayi: tam32, sonuc: Dizi<dtam64>) -> tam32
```

**Ne yapar:** `state_pool[0]`'dan başlayarak `sayi` adet xorshift64 değeri üretir, `sonuc` dizisine yazar ve güncel state'i `state_pool[0]`'a geri yazar (stateful). Her zaman `0` döner.

**Örnek:**
```kemgu
değişken havuz: Dizi<dtam64> = [12345];  // başlangıç state
xorshift64_pool(havuz, 4, cikti);  // cikti'ya 4 değer; havuz[0] güncellenir
```

**Kenar durum:** `state_pool[0] = 0` ise dejenere (sabit 0 üretir). `sayi = 0` ise hiçbir değer yazılmaz ama state yine de `state_pool[0]`'a geri yazılır (değişmeden). Test/üretim ayrımı: yalnız test için.

### _rastgele_rotl_u64 (yardımcı)

```kemgu
işlev _rastgele_rotl_u64(x: dtam64, n: tam32) -> dtam64
```

**Ne yapar:** 64-bit sola döngüsel kaydırma (`(x << n) | (x >> (64 - n))`). xoshiro256** içinde kullanılır. Burada `dtam64` (sabitsüre değil) — PRNG public state.

**Örnek:**
```kemgu
değişken r: dtam64 = _rastgele_rotl_u64(x, 7);
```

**Kenar durum:** `n` 1..63 aralığında olmalı (`n = 0` veya `n = 64` tanımsız kaydırma).

### xoshiro256_adim

```kemgu
işlev xoshiro256_adim(s: Dizi<dtam64>) -> dtam64
```

**Ne yapar:** Blackman & Vigna 2018 xoshiro256** PRNG'nin tek adımı. 256-bit state (4× dtam64), period 2^256−1. BigCrush/TestU01 dağılım kalitesi xorshift64'ten çok daha iyi. State dizisi `s` yerinde güncellenir; üretilen değer döner. Yine de **DRBG değil** — kripto için yalnız bir DRBG/HKDF zinciri ile birlikte.

**Örnek:**
```kemgu
değişken s: Dizi<dtam64> = [a, b, c, d];  // 4 elemanlı state
değişken deger: dtam64 = xoshiro256_adim(s);  // s güncellenir
```

**Kenar durum:** `s` tam 4 eleman içermelidir. Tüm state 0 ise dejenere. Kripto-güvensiz (test/simülasyon).

### secure_rastgele_doldur

```kemgu
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32
```

**Ne yapar:** `sonuc` dizisini `gereksinim` adet güvenli rastgele bayt ile doldurmayı **amaçlar**. V2'de gerçek OS RNG (getrandom / BCryptGenRandom / getentropy) çağrısı yapacaktır. Dönüş: dolan bayt sayısı (`gereksinim`'e eşit; hata durumunda daha az). **V1'de PLACEHOLDER**: sabit bir Fibonacci-türevsel seed (`0x9E3779B97F4A7C15`) ile xorshift64 üretir — **KRİPTO GÜVENSİZ, sadece compile/test**.

**Örnek:**
```kemgu
değişken n: tam32 = secure_rastgele_doldur(buf, 32);  // V1: deterministik (güvensiz)
```

**Kenar durum:** V1 her çağrıda **aynı** (sabit seed) deterministik diziyi üretir — tahmin edilebilir. `gereksinim = 0` ise hiçbir bayt yazılmaz, `0` döner. dtam64→dtam8 daraltma iki adımda yapılır (E004'ten kaçınmak için önce dtam32, sonra dtam8).

### hw_rastgele_aktif_mi

```kemgu
işlev hw_rastgele_aktif_mi() -> mantıksal
```

**Ne yapar:** Donanım RNG (RDRAND / ARMv8.5 RNG) mevcudiyetini sorgular. **V1 stub**: her zaman `yanlış` döner. V2'de CPUID / `__builtin_cpu_supports` kontrolü yapacaktır.

**Örnek:**
```kemgu
eğer hw_rastgele_aktif_mi() {
    // V2: donanım RNG yolu
}
```

**Kenar durum:** V1'de daima `yanlış` (donanım yolu hiç aktif değil).

### entropy_karistir

```kemgu
işlev entropy_karistir(
    kaynak1: Dizi<sabitsüre<dtam8>>,
    kaynak2: Dizi<sabitsüre<dtam8>>,
    sonuc: Dizi<sabitsüre<dtam8>>,
    n: tam32
) -> tam32
```

**Ne yapar:** Birden çok entropi kaynağını karıştırır. **V1**: basit bayt-bazlı XOR (`sonuc[i] = kaynak1[i] ^ kaynak2[i]`). V2'de SHA-based extract+expand (HKDF benzeri) olacaktır. Her zaman `0` döner.

**Örnek:**
```kemgu
entropy_karistir(hw_entropi, os_entropi, karisik, 32);
```

**Kenar durum:** `n = 0` ise hiçbir bayt yazılmaz. V1 yalnız iki kaynağı XOR'lar — gerçek entropi extraction değil (saf XOR güvenli karıştırma garantisi vermez).

### test_rastgele_seq (yardımcı)

```kemgu
işlev test_rastgele_seq(seed: dtam64, uzunluk: tam32, sonuc: Dizi<dtam64>) -> tam32
```

**Ne yapar:** Verilen `seed`'den başlayarak `uzunluk` adet xorshift64 değeri üretir (`sonuc`'a yazar). Reproducible (tekrarlanabilir) test vektörü üretimi için — **yalnız UNIT TEST**. Her zaman `0` döner.

**Örnek:**
```kemgu
test_rastgele_seq(42, 8, test_vektoru);  // seed=42 ile 8 deterministik değer
```

**Kenar durum:** `seed = 0` ise zincir çöker (xorshift64 0'da takılır). Kripto için kullanılmamalıdır.

---

## `anahtar.kem` — Anahtar Yönetimi (Linear + CT)

Anahtarlar `tekkez<Anahtar>` linear tipi ile yönetilir: kopyalanamaz, `kullan(a)` ile tüketilir (sonrasında erişim L002 hatası), `imha(a)` ile bertaraf edilir (V2'de zeroize-on-drop). Linear tipler sayesinde derleyici anahtarın ileri scope'a sızmayacağını **garanti eder** ve scope sonunda tüketilmemiş anahtar L001 hatası verir (Rust Zeroize trait'inin opt-in zaafının aksine mandatory). HKDF (RFC 5869) iskeleti içerir; içerik tipleri `sabitsüre` ile yan kanala kapalıdır.

### Yapı tipleri

```kemgu
yapı SimetrikAnahtar256 {  // 256-bit (32 bayt) — ChaCha20 / AES-256 uyumlu
    bayt0..bayt7: sabitsüre<dtam8>;       // ilk 8 bayt (özet alanlar)
    govde: Dizi<sabitsüre<dtam8>>;        // 32-bayt tam içerik
}
yapı Nonce128 { govde: Dizi<sabitsüre<dtam8>>; }    // 16 bayt IV
yapı HKDF_Salt { govde: Dizi<sabitsüre<dtam8>>; }   // 32 bayt salt
```

### anahtar_olustur

```kemgu
işlev anahtar_olustur(govde: Dizi<sabitsüre<dtam8>>) -> tekkez<SimetrikAnahtar256>
```

**Ne yapar:** 32-baytlık `govde` dizisinden bir `SimetrikAnahtar256` yapısı kurar ve `tekkez_olustur` ile linear sarmala alarak döner. Sonuç tek-kullanımlıktır: `kullan` veya `imha` ile tüketilmelidir.

**Örnek:**
```kemgu
değişken anahtar: tekkez<SimetrikAnahtar256> = anahtar_olustur(rastgele_baytlar);
// anahtar artık kopyalanamaz; kullan(anahtar) veya imha(anahtar) ile tüketilmeli
```

**Kenar durum:** `govde` en az 8 eleman içermelidir (bayt0..bayt7 doğrudan indekslenir). Dönen `tekkez` scope sonuna kadar tüketilmezse L001 hatası. `govde` referansı yapıya de kopyalanır (govde alanı).

### anahtar_imha

```kemgu
işlev anahtar_imha(a: tekkez<SimetrikAnahtar256>) -> tam32
```

**Ne yapar:** Anahtarı `imha(a)` ile tek noktadan tüketir (bertaraf). Sonrasında `a`'ya erişim L002 hatası. V2'de derleyici bu çağrı sonrası belleği zeroize'lemek için LLVM intrinsic emit edecektir. Her zaman `0` döner.

**Örnek:**
```kemgu
anahtar_imha(anahtar);  // anahtar tüketildi; sonraki erişim L002
```

**Kenar durum:** Çağrıdan sonra `a` kullanılamaz (linear move). V1'de gerçek bellek sıfırlaması **yok** (yalnız tip-seviyesi tüketim); fiziksel zeroize V2'de gelecektir.

### hkdf_extract

```kemgu
işlev hkdf_extract(
    salt: Dizi<sabitsüre<dtam8>>,
    salt_uzunluk: tam32,
    ikm: Dizi<sabitsüre<dtam8>>,
    ikm_uzunluk: tam32,
    prk: Dizi<sabitsüre<dtam8>>
) -> tam32
```

**Ne yapar:** HKDF (RFC 5869) Extract aşaması — IKM'yi salt ile uniformize ederek pseudo-random key (PRK) üretir. **V1 PLACEHOLDER**: gerçek HMAC-SHA256 yerine `prk`'yı sıfırlayıp salt ve IKM'yi bayt-bayt XOR'lar. **ÜRETİM İÇİN ASLA KULLANMA.** Her zaman `0` döner.

**Örnek:**
```kemgu
hkdf_extract(salt, 32, ikm, 32, prk);  // prk[0..31] dolar (V1: XOR stub)
```

**Kenar durum:** `prk` ilk 32 bayt sıfırlanır (sabit 32-loop). `salt_uzunluk` ve `ikm_uzunluk` 32'yi aşarsa `prk` taşması olabilir (dizinin en az 32 + max(uzunluk) kapasitesi varsayılır). Gerçek HMAC-SHA256 V2'de `karma.kem` ile birleştirilecektir.

### hkdf_expand

```kemgu
işlev hkdf_expand(
    prk: Dizi<sabitsüre<dtam8>>,
    info: Dizi<sabitsüre<dtam8>>,
    info_uzunluk: tam32,
    okm: Dizi<sabitsüre<dtam8>>,
    okm_uzunluk: tam32
) -> tam32
```

**Ne yapar:** HKDF Expand aşaması — PRK'dan istenen uzunlukta output key material (OKM) türetmeyi amaçlar. **V1 PLACEHOLDER**: gerçek HMAC + counter döngüsü yerine `prk[i mod 32] ^ info[i mod 16]` rotasyonel-genişletme stub'ı. `info_uzunluk` PUBLIC olduğu için dallanma CT-OK. Her zaman `0` döner.

**Örnek:**
```kemgu
hkdf_expand(prk, info, 16, okm, 32);  // okm[0..31] dolar (V1: stub)
```

**Kenar durum:** `info_uzunluk = 0` ise info karıştırması atlanır (yalnız `prk[i mod 32]` yazılır). V1'de info indeksi sabit 16 baytla maskelenir (`i & 15`), değişken `info_uzunluk` modülosu değil — bu bir stub kısıtıdır. `okm_uzunluk` kadar bayt yazılır.

### anahtar_turet

```kemgu
işlev anahtar_turet(
    ikm: Dizi<sabitsüre<dtam8>>,
    ikm_uzunluk: tam32,
    salt: Dizi<sabitsüre<dtam8>>,
    salt_uzunluk: tam32,
    info: Dizi<sabitsüre<dtam8>>,
    info_uzunluk: tam32,
    sonuc_govde: Dizi<sabitsüre<dtam8>>
) -> tam32
```

**Ne yapar:** Tek-adım HKDF wrapper: IKM + salt + info → türetilmiş 32-baytlık anahtar gövdesi. İçeride 32-baytlık geçici `prk` oluşturur, `hkdf_extract` + `hkdf_expand` (32 bayt OKM) çağırır, sonucu `sonuc_govde`'ye yazar. Her zaman `0` döner. Not: imza V1'de `tekkez<SimetrikAnahtar256>` değil `tam32` döner — sonucu `sonuc_govde` üzerinden verir; tekkez sarmalama için ayrıca `anahtar_olustur` çağrılabilir.

**Örnek:**
```kemgu
anahtar_turet(ikm, 32, salt, 32, info, 16, turetilen_govde);
değişken anahtar: tekkez<SimetrikAnahtar256> = anahtar_olustur(turetilen_govde);
```

**Kenar durum:** Underlying HKDF V1 stub olduğundan **türetilen anahtar kriptografik değildir** — üretim için kullanılmamalıdır. `sonuc_govde` en az 32 eleman içermelidir.

### anahtar_govdesi_esit

```kemgu
işlev anahtar_govdesi_esit(
    a: Dizi<sabitsüre<dtam8>>,
    b: Dizi<sabitsüre<dtam8>>,
    uzunluk: tam32
) -> sabitsüre<mantıksal>
```

**Ne yapar:** İki anahtar gövdesini sabit-sürede karşılaştırır (OR-toplama + zeroness; `sabit_süre_eşit_blok` ile aynı algoritma). HMAC tag veya parola hash karşılaştırması için ZORUNLU (CRYPTO-1 timing-attack önlemesi).

**Örnek:**
```kemgu
değişken esit: sabitsüre<mantıksal> = anahtar_govdesi_esit(beklenen, hesaplanan, 32);
```

**Kenar durum:** `uzunluk = 0` → birikim 0 kalır → sonuç `doğru` (boş diziler eşit). Süre içeriğe değil yalnız `uzunluk`'a bağlı; erken çıkış yok.

### otp_sifrele

```kemgu
işlev otp_sifrele(
    metin: Dizi<sabitsüre<dtam8>>,
    anahtar_l: tekkez<SimetrikAnahtar256>,
    sifreli: Dizi<sabitsüre<dtam8>>,
    n: tam32
) -> tam32
```

**Ne yapar:** One-Time Pad şifreleme. `kullan(anahtar_l)` ile linear anahtarı tüketir (sonrasında L002 — bir kez kullanım garantisi), ardından `sifreli[i] = metin[i] ^ anahtar.govde[i]` ile XOR'lar. Linear types + CT + OTP birleşimi (stratejik güvenlik hedefi). Her zaman `0` döner.

**Örnek:**
```kemgu
değişken anahtar: tekkez<SimetrikAnahtar256> = anahtar_olustur(rastgele32);
otp_sifrele(duz_metin, anahtar, sifreli_metin, 32);
// anahtar artık tüketildi; tekrar kullanılamaz (L002)
```

**Kenar durum:** `n`, anahtar gövdesinin uzunluğunu (32) aşmamalıdır (`anahtar.govde[i]` indeks taşması). `n = 0` ise hiçbir bayt yazılmaz ama anahtar yine tüketilir. OTP güvenliği için anahtar **gerçekten rastgele** ve **bir kez** kullanılmalı (linear tip ikincisini zaten garanti eder).

---

## `sifre.kem` — Simetrik Şifreleme (ChaCha20 + Poly1305 AEAD)

RFC 8439 IETF ChaCha20 ve Poly1305. Yalnız ADD/XOR/ROTL operasyonları kullanır — S-box yok, dolayısıyla AES T-table cache-timing kanalına (Bernstein 2005) bağışık; her mimaride uniform süre. Wireguard, TLS 1.3, OpenSSH gibi modern protokollerin tercihi. AEAD construction tam framing ve Poly1305 modüler azaltma V2'de; V1 iskelet + clamp sunar. Modül standalone (`_sifre_rotl_u32` içerden tanımlı).

### _sifre_rotl_u32 (yardımcı)

```kemgu
işlev _sifre_rotl_u32(x: sabitsüre<dtam32>, n: tam32) -> sabitsüre<dtam32>
```

**Ne yapar:** Modül-içi 32-bit rotate-left (`(x << n) | (x >> (32 - n))`). ChaCha20 quarter-round'un temel ilkel'i (import yokluğu için kopyalanmış).

**Örnek:**
```kemgu
değişken r: sabitsüre<dtam32> = _sifre_rotl_u32(d, 16);
```

**Kenar durum:** `n` public sabit, 1..31 aralığında olmalı.

### chacha20_qr

```kemgu
işlev chacha20_qr(
    state: Dizi<sabitsüre<dtam32>>,
    a_idx: tam32, b_idx: tam32, c_idx: tam32, d_idx: tam32
) -> tam32
```

**Ne yapar:** ChaCha20 quarter-round (RFC 8439 §2.1). `state`'in dört elemanına (a/b/c/d indeksleri) ADD-XOR-ROTL dizisini uygular ve yerinde günceller: `a+=b; d^=a; d=ROTL(d,16); c+=d; b^=c; b=ROTL(b,12); a+=b; d^=a; d=ROTL(d,8); c+=d; b^=c; b=ROTL(b,7)`. İndeksler PUBLIC parametre (4-deep tuple yerine ayrı argümanlar). Her zaman `0` döner.

**Örnek:**
```kemgu
chacha20_qr(state, 0, 4, 8, 12);  // birinci sütun round'u
```

**Kenar durum:** İndeksler PUBLIC olmalı (gizli indeksli erişim CT002 ihlali olur). `state` en az 16 eleman; indeksler 0..15 aralığında olmalı.

### chacha20_cekirdek

```kemgu
işlev chacha20_cekirdek(state: Dizi<sabitsüre<dtam32>>) -> tam32
```

**Ne yapar:** ChaCha20 iç çekirdeği — 10 double round (toplam 20 round). Her double round'da 4 sütun (column) + 4 köşegen (diagonal) quarter-round çağrısı yapar. `state` (16-eleman 4×4 matris) yerinde karıştırılır. Her zaman `0` döner.

**Örnek:**
```kemgu
chacha20_cekirdek(state);  // 20 round karıştırma uygula
```

**Kenar durum:** `state` tam 16 eleman içermelidir. Round sayacı PUBLIC (10 sabit) — dallanma içeriğe bağlı değil. Çekirdek tek başına final-add yapmaz (bkz. `chacha20_blok`).

### chacha20_blok

```kemgu
işlev chacha20_blok(
    key: Dizi<sabitsüre<dtam32>>,
    counter: dtam32,
    nonce: Dizi<sabitsüre<dtam32>>,
    keystream: Dizi<sabitsüre<dtam32>>
) -> tam32
```

**Ne yapar:** ChaCha20 block function (RFC 8439 §2.3). 64-baytlık (16 dtam32) keystream bloğu üretir. State = [4 sabit "expand 32-byte k", 8-kelime key, counter, 3-kelime nonce]. İç state kopyası saklanır, `chacha20_cekirdek` ile karıştırılır, ardından orijinal state element-wise eklenir (`keystream[i] = mixed[i] + orig[i]`). Her zaman `0` döner.

**Örnek:**
```kemgu
// key: 8 dtam32, nonce: 3 dtam32 (96-bit), counter: blok sayacı
chacha20_blok(key, 1, nonce, keystream);  // keystream[0..15] dolar
```

**Kenar durum:** `key` tam 8, `nonce` tam 3, `keystream` tam 16 eleman içermelidir. `counter` PUBLIC (tüm CT ChaCha implementasyonları gibi). Constants little-endian: 0x61707865, 0x3320646E, 0x79622D32, 0x6B206574.

### chacha20_xor_blok

```kemgu
işlev chacha20_xor_blok(
    keystream: Dizi<sabitsüre<dtam32>>,
    metin: Dizi<sabitsüre<dtam32>>,
    cikti: Dizi<sabitsüre<dtam32>>,
    n: tam32
) -> tam32
```

**Ne yapar:** ChaCha20 şifreleme/şifre-çözme (her ikisi de aynı XOR operasyonu): `cikti[i] = metin[i] ^ keystream[i]` (i = 0..n). Kelime (dtam32) birimli çalışır. Her zaman `0` döner.

**Örnek:**
```kemgu
chacha20_xor_blok(keystream, duz_metin, sifreli, 16);  // sifrele
chacha20_xor_blok(keystream, sifreli, duz_metin, 16);  // ayni cagri ile coz
```

**Kenar durum:** `n = 0` ise hiçbir kelime yazılmaz. Tüm diziler en az `n` eleman içermeli. Byte-level + counter increment tam stream API V2'de (V1 yalnız 16-word blok birimi).

### poly1305_r_clamp

```kemgu
işlev poly1305_r_clamp(r: Dizi<sabitsüre<dtam8>>) -> tam32
```

**Ne yapar:** Poly1305 `r` anahtarını clamp eder (RFC 8439 §2.5): `r[3], r[7], r[11], r[15] &= 0x0F` ve `r[4], r[8], r[12] &= 0xFC`. Belirli bitleri temizleyerek `r`'yi Z_p alanına uygun hale getirir. Her zaman `0` döner.

**Örnek:**
```kemgu
poly1305_r_clamp(r);  // r belirli pozisyonlarda maskelenir
```

**Kenar durum:** `r` en az 16 eleman içermelidir (sabit indeksler 3,4,7,8,11,12,15). İndeksler sabit/public — CT-OK.

### poly1305_blok_birikim

```kemgu
işlev poly1305_blok_birikim(
    h: Dizi<sabitsüre<dtam32>>,
    r: Dizi<sabitsüre<dtam32>>,
    msg_blok: Dizi<sabitsüre<dtam32>>
) -> tam32
```

**Ne yapar:** Poly1305 blok birikim — gerçek MAC `(h + r * msg_block) mod (2^130-5)` olmalıdır. **V1 PLACEHOLDER**: gerçek 130-bit çarpma + Barrett azaltma yerine yalnız `h[i] = h[i] ^ (r[i] & msg_blok[i])` (i = 0..3) yapar — **GERÇEK POLY1305 DEĞİL**. Her zaman `0` döner.

**Örnek:**
```kemgu
poly1305_blok_birikim(h, r, msg_blok);  // V1: stub birikim
```

**Kenar durum:** Yalnız ilk 4 kelime işlenir. V1 çıktısı kriptografik bir MAC değildir — kimlik doğrulama güvenliği sağlamaz. Tam modüler azaltma (Barrett/Montgomery) V2'de. Tüm aritmetik sabit-süre (DIV/MOD yok).

### aead_chacha20_poly1305_sifrele

```kemgu
işlev aead_chacha20_poly1305_sifrele(
    key: Dizi<sabitsüre<dtam32>>,
    nonce: Dizi<sabitsüre<dtam32>>,
    aad: Dizi<sabitsüre<dtam32>>,
    aad_uzunluk: tam32,
    plaintext: Dizi<sabitsüre<dtam32>>,
    plaintext_uzunluk: tam32,
    ciphertext: Dizi<sabitsüre<dtam32>>,
    tag: Dizi<sabitsüre<dtam32>>
) -> tam32
```

**Ne yapar:** ChaCha20-Poly1305 AEAD şifreleme (RFC 8439 §2.8). Adımlar: (1) counter=0 ile one-time-key (otk) üret, (2) counter=1 keystream ile plaintext'i XOR'layarak ciphertext üret, (3) Poly1305 ile tag hesapla. **V1 iskelet**: otk ve keystream gerçek üretilir, fakat tag üretimi `poly1305_blok_birikim` stub'ına bağlıdır ve tam AEAD framing (AAD || pad16 || CT || pad16 || len) yapılmaz. Her zaman `0` döner.

**Örnek:**
```kemgu
aead_chacha20_poly1305_sifrele(key, nonce, aad, 0, plaintext, 16, ciphertext, tag);
// ciphertext gerçek ChaCha20 şifreli; tag V1'de stub (güvenli değil)
```

**Kenar durum:** `key` 8, `nonce` 3, `otk`/`keystream` 16 kelime. **tag V1'de kriptografik değildir** — gerçek kimlik doğrulama V2'de. `plaintext_uzunluk` ≤ 16 (tek blok). Üretim için kullanılmamalıdır.

### aead_chacha20_poly1305_dogrula

```kemgu
işlev aead_chacha20_poly1305_dogrula(
    key: Dizi<sabitsüre<dtam32>>,
    nonce: Dizi<sabitsüre<dtam32>>,
    aad: Dizi<sabitsüre<dtam32>>,
    aad_uzunluk: tam32,
    ciphertext: Dizi<sabitsüre<dtam32>>,
    ciphertext_uzunluk: tam32,
    beklenen_tag: Dizi<sabitsüre<dtam32>>,
    plaintext: Dizi<sabitsüre<dtam32>>
) -> sabitsüre<mantıksal>
```

**Ne yapar:** AEAD doğrulama (decrypt + tag karşılaştırma). Tag'i sabit-sürede karşılaştırır: 4 kelimeyi OR-toplar, birikim 0 ise eşit (`acc == 0`). `if tag != computed_tag` deseni TIMING SALDIRISI (CRYPTO-1) olduğu için CT karşılaştırma kritiktir. **V1 stub**: tag yeniden-hesaplama (`hesaplanan_tag`) yapılmaz — tüm-sıfır dizi ile `beklenen_tag` karşılaştırılır, bu yüzden gerçek doğrulama yoktur.

**Örnek:**
```kemgu
değişken gecerli: sabitsüre<mantıksal> =
    aead_chacha20_poly1305_dogrula(key, nonce, aad, 0, ct, 16, tag, pt);
// gecerli gizli; dallanmak için ifşa(gecerli) gerekir
```

**Kenar durum:** V1'de `hesaplanan_tag` sıfır kaldığından doğrulama anlamlı değildir — yalnız sabit-süre karşılaştırma deseninin iskeleti. Tag karşılaştırma 4 kelime (128-bit) üzerinden; erken çıkış yok (sabit-süre). Tam decrypt + tag üretimi V2'de.
