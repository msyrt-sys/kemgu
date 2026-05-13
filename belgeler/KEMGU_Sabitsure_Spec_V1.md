# KEMGU Sabitsüre (Constant-Time) Spec V1

**Durum:** TASLAK (Direktif Hedef 1 — Kırılamaz Güvenlik altında).
**Spec içi alt-adımlar otomatik onaylı; Direktif Ek v1.2 ile MERGE bekler.**

---

## CT.0 — Motivasyon ve Üç Stratejik Hedef Bağlantısı

KEMGU `sabitsüre<T>` tipi, bir değerin **gizli (secret)** olduğunu ve onun
üzerinde yapılacak hiçbir işlemin **çalışma süresi**, **bellek erişim deseni**
veya **kontrol akışı** yoluyla yan kanal (side-channel) bilgi sızdırmaması
gerektiğini tip sisteminde işaretler. Tür yapısının kendisi bir **bilgi akış**
disiplinidir (information-flow control; Sabelfeld & Myers 2003) — fakat amacı
güvenli akış değil, **constant-time disiplini**dir.

### Stratejik Hedef Bağlantısı

- **HEDEF 1 (Kırılamaz Güvenlik):** OneTimePad, ECDH, RSA, AES gibi kripto
  ilkellerinde **yan kanal saldırılarının** önlenmesi. Bilinen saldırılar:
  - **Kocher 1996 (CRYPTO):** RSA `modpow` zamanlama saldırısı — modüler
    üs alma adımları `e`'nin bitlerine bağlı koşullu dallanma içeriyordu.
  - **Brumley & Boneh 2003 (USENIX Sec):** Apache `mod_ssl` üzerinden ağ
    üzerinden uzaktan RSA private key çıkarımı.
  - **Bernstein 2005 (eprint):** AES T-table cache-timing saldırısı —
    `s_box[plaintext ^ key]` bellek erişim deseni gizli `key`'i sızdırır.
  - **Brumley & Tuveri 2011 (ESORICS):** OpenSSL ECDSA — `k` değişkeninin
    biti üzerinde branching `k`'yi yan kanaldan açığa çıkardı.
  - **Spectre v1/v2 (Kocher et al. 2019, S&P):** Speculative execution
    bellek isolation'ı ihlal eder.
  - **Hertzbleed (Wang et al. 2022, USENIX Sec):** DVFS power → frekans →
    timing kanalı.

  `sabitsüre<T>` ile bu sınıfların **derleme zamanında engellenmesi** hedeflenir.
  Hata mesajı çalışma zamanında değil, programcının ekranında görünür.

- **HEDEF 2 (Maksimum Performans):** Zero-overhead — `sabitsüre<T>` runtime
  temsili tıpkı `T` gibidir. Hiçbir refcount, taint flag, dynamic check yok.
  LLVM speculation barrier (`lfence` / `dsb sy + isb`) sadece **gerekli olduğu
  yerlerde** (sabitsüre dönüşlü işlev sınırlarında) emisyon edilir.

- **HEDEF 3 (Evrensel OS):** Page table, capability handle, OTP anahtarı,
  TLS key, Wireguard handshake, oturum cookie'si vb. tüm OS kripto state'i
  doğal olarak `sabitsüre<T>` ile etiketlenir.

### ASLA Listesi Hatırlatması

- ASLA implicit conversion: `T -> sabitsüre<T>` otomatiktir (taint = upgrade),
  fakat `sabitsüre<T> -> T` **asla** otomatik değil — `ifşa(s)` çağrısı zorunlu.
- ASLA exception: `ifşa` derleme-zamanı bir operatör, runtime hata değil.
- ASLA null: `sabitsüre<seçimlik<T>>` modellenebilir, `sabitsüre<T>?` yasak.

---

## CT.1 — Tip Tanımı

```
sabitsüre<T> : tip      (T : sabitsüre-yetenekli tip)
```

`sabitsüre` bir tip kurucusudur; runtime temsili `T` ile aynıdır
(zero-overhead). Constant-time disiplini tip sisteminde takip edilir.

### CT-WRAP (Sarılabilen Tipler — V1)

`T`'nin **sabitsüre-yetenekli** olması gerekir. V1'de izinlenler:

| Kategori | Örnek tipler | Sebep |
|----------|--------------|-------|
| Tamsayı  | tam8/16/32/64, dtam8/16/32/64 | CPU'da sabit-süre add/sub/xor/and/or var |
| Karakter | karakter (Unicode kod noktası) | tam32 alt-küme |
| Mantıksal| mantıksal | i1 üzerinde CT op'ları (select) var |
| Dizi (yetenekli T)| Dizi<sabitsüre<tam8>>, sabitsüre<Dizi<dtam8>> | byte-wise CT op |

V1'de **YASAK** (CT006 SABITSURE_WRAP_INVALID):
- `sabitsüre<kesirli32>` / `sabitsüre<kesirli64>` — FP op'ları (fdiv, fsqrt,
  fma denormal handling) variable-time. NaN/Inf süresi farklı.
- `sabitsüre<metin>` — UTF-8 değişken-genişlikli; karakter sayısı/uzunluk
  iterasyonu varies. Sabit-süre byte karşılaştırma için `Dizi<sabitsüre<dtam8>>`.
- `sabitsüre<yapı>` — V2'de "sabitsüre yapı" kavramı (her alan sabitsüre);
  V1'de dış sarmalayıcı zorunlu (alan-bazlı `sabitsüre<dtam8>`).
- `sabitsüre<seçimlik<T>>` — tag check branching gerektirir.
- `sabitsüre<tekkez<T>>` — lineerlik ve constant-time ortogonal; ileride
  destek (`tekkez<sabitsüre<T>>` izinli, içeride sabitsüre alan).

---

## CT.2 — Sözdizim

Bir yeni anahtar kelime:

- `sabitsüre` — tip kurucusu ve **declassification operatörü** birlikte.

`ifşa` (declassification) ayrı keyword DEĞİL; **built-in işlev** olarak çözüldü:
`ifşa(s: sabitsüre<T>) -> T`. Bu sayede toplam keyword sayısı +1 (33 → 34).

### Üretim (Wrap)

```
sabitsüre_yarat(v: T) -> sabitsüre<T>       // built-in intrinsic
```

Implicit yükseltme: `T` bekleyen yere `sabitsüre<T>` geçirilirse hata; tersi
(yani `sabitsüre<T>` bekleyen yere `T`) **otomatik wrap** edilebilir
(public → secret upgrade her zaman güvenli). V1: explicit `sabitsüre_yarat`
zorunlu, otomatik upgrade YOK (basitlik).

### Sözdizimsel Örnekler

```kemgu
// Üretim
değişken k: sabitsüre<dtam32> = sabitsüre_yarat(0xC0DE_1234);

// Aritmetik (sonuç da sabitsüre — taint yayılır)
değişken k2: sabitsüre<dtam32> = k ^ sabitsüre_yarat(0xFFFF_FFFF);

// Karşılaştırma (sonuç sabitsüre<mantıksal>!)
değişken eşit: sabitsüre<mantıksal> = k == k2;

// Declassification — açıkça ifşa
değişken aşikar: dtam32 = ifşa(k);

// HATA: gizli üzerinde dallanma (CT001)
eğer eşit { yazdir("eşit"); }   // → CT001

// DOĞRU: önce ifşa edip dallan
eğer ifşa(eşit) { yazdir("eşit"); }   // OK (ama leak — sorumluluk programcıda)
```

---

## CT.3 — Tip Kuralları

Notasyon: `Γ ⊢ e : τ`. `Γ ⊢_ct e` ifadesi `e`'nin constant-time disiplinine
uyduğu anlamına gelir (taintede dallanma/index/div yok).

### CT-FORM (Tip Formasyonu)

```
T : sabitsure-yetenekli-tip      (CT-WRAP listesinde)
─────────────────────────────────
sabitsüre<T> : tip
```

`sabitsüre<sabitsüre<T>>` — V1'de gereksiz; otomatik `sabitsüre<T>`'ye
indirgenmez (nesting sabitsüre redundancy hatası `CT006`).

### CT-PROD (Producer)

```
Γ ⊢ v : T          T sabitsure-yetenekli
─────────────────────────────────────────
Γ ⊢ sabitsüre_yarat(v) : sabitsüre<T>
```

### CT-DECLASS (Açık İfşa)

```
Γ ⊢ s : sabitsüre<T>
──────────────────────
Γ ⊢ ifşa(s) : T
```

`ifşa` runtime cost'u sıfırdır — sadece tipi düzleştirir. Programcının
sorumluluğu: ifşa noktası bilinçli bir bilgi-akış sınırı olmalı (örn. ECDSA
imzanın `r,s` çıktısı public; signing scalar `k` private).

### CT-ARITH (Aritmetik — Taint Yayılır)

```
Γ ⊢ a : sabitsüre<T>     Γ ⊢ b : sabitsüre<T>     op ∈ {+, -, *, &, |, ^, <<, >>}
────────────────────────────────────────────────────────────────────────────────
Γ ⊢ a op b : sabitsüre<T>

Γ ⊢ a : sabitsüre<T>     Γ ⊢ b : T     op ∈ {+, -, *, &, |, ^, <<, >>}
─────────────────────────────────────────────────────────────────────────
Γ ⊢ a op b : sabitsüre<T>       (public yayılır secrete — upgrade güvenli)
```

`<<` ve `>>` için **kaydırma miktarı** (sağ taraf) **PUBLIC olmalı** —
variable-shift bazı CPU'larda variable-time (örn. eski ARM Cortex-M). V1'de
shift miktarı sabitsüre ise CT008.

### CT-DIV (Bölme/Mod YASAK)

```
Γ ⊢ a : sabitsüre<T>     op ∈ {/, %}
─────────────────────────────────────
HATA: CT004 SABITSURE_DIVMOD
```

Sebep: x86 `idiv`/`div` opcode'ları **variable-time** — bölen değerine göre
mikroop sayısı değişir (Intel Optimization Manual §3.4.1). ARM `udiv`/`sdiv`
de aynı şekilde. Sabit süre alternatif: Barrett / Montgomery reduction
manuel yazılır (`sabitsüre<T>` üzerinde sadece +/-/*/and/or/xor/shl/lshr).

### CT-CMP (Karşılaştırma)

```
Γ ⊢ a : sabitsüre<T>     Γ ⊢ b : sabitsüre<T>     op ∈ {==, !=, <, >, <=, >=}
──────────────────────────────────────────────────────────────────────────────
Γ ⊢ a op b : sabitsüre<mantıksal>
```

Sonuç **sabitsüre** çünkü iki gizli değer arasındaki eşitlik **bilgidir**
(timing değil, semantik). Eğer programcı sonuca göre dallanmak isterse
`ifşa(...)` zorunlu — bu noktada bir leak meydana gelir ama programcı
bilinçli olarak izin vermiş olur.

İmplementasyon: `==` için constant-time XOR + OR reduction
(`(a ^ b) == 0` LLVM IR'de `icmp eq` ama Intel'de mikroop seviyesinde sabit
süre). `<` için tüm bitleri taramak gerekiyor — V1: `icmp slt` kabul (modern
x86/ARM'da sabit süre); V2'de bit-paralel constant-time karşılaştırma.

### CT-BRANCH (Kontrol Akışı YASAK)

```
Γ ⊢ kosul : sabitsüre<mantıksal>
─────────────────────────────────────────
HATA (her biri ayrı kod):
  eğer kosul { ... }     → CT001 SABITSURE_IF_BRANCH
  iken kosul { ... }     → CT001 SABITSURE_WHILE_BRANCH
  eşleş v { ... }        → CT001 SABITSURE_MATCH (v sabitsüre ise)
  eğer kosul { e1 } değilse { e2 }  → CT001 (her dal)
```

Düzeltme: `ifşa(kosul)` ile dallanma izni; veya **constant-time select**
operatörü `sabitsüre_seç(kosul, t, f)` (gelecekte built-in).

### CT-INDEX (Veri-Bağımlı Bellek Erişimi YASAK)

```
Γ ⊢ idx : sabitsüre<T>
───────────────────────
HATA: a[idx] → CT002 SABITSURE_INDEX
```

Sebep: Cache-line granülaritesi. Bernstein 2005 AES cache-timing saldırısı
tam bu zayıflığı sömürdü: `T_box[ p ^ k ]` erişim ad­resi `k`'yi sızdırdı.
Düzeltme: **constant-time table lookup** — tüm tabloyu okuyup XOR'lı maske
ile seçim. Veya programcı bilinçli olarak `a[ifşa(idx)]`.

### CT-DOWNGRADE (Implicit Aşağı-Dönüşüm YASAK)

```
Γ ⊢ s : sabitsüre<T>     beklenen tip: T
─────────────────────────────────────────
HATA: CT003 SABITSURE_LEAK
```

Atama, `ver`, çağrı argümanı, yapı alanı, dizi elemanı — hepsinde tip katı.
Düzeltme: `ifşa(s)`.

### CT-CALL (Public İşleve Geçirme YASAK)

```
Γ ⊢ s : sabitsüre<T>     işlev f(p: T) -> U    (parametre normal T)
─────────────────────────────────────────────────────────────────────
HATA: f(s) → CT003 (cf. CT-DOWNGRADE)
```

`yazdir`, `bellek_al`, `bellek_kopyala` gibi built-in I/O işlevleri public
parametreli — sabitsüre geçirilemez. Programcı `ifşa(...)` ile bilinçli
"leak point" oluşturmalı.

### CT-WRAP-CHECK (Yetenekli Tip Kontrolü)

```
T ∉ {tam*, dtam*, karakter, mantıksal, Dizi<U yetenekli>}
──────────────────────────────────────────────────────────
HATA: sabitsüre<T> → CT006 SABITSURE_WRAP_INVALID
```

Özellikle:
- `sabitsüre<kesirli32>` → CT006 (FP non-CT)
- `sabitsüre<metin>` → CT006 (UTF-8 varies)
- `sabitsüre<sabitsüre<T>>` → CT006 (nesting redundancy)

### CT-SHIFT (Sabit-Süre Kaydırma)

```
Γ ⊢ x : sabitsüre<T>     Γ ⊢ s : sabitsüre<U>
──────────────────────────────────────────────
HATA: x << s → CT008 SABITSURE_SHIFT_AMOUNT
```

Kaydırma miktarı public olmalı. Pratikte sabit (3, 7 gibi); değişken olmalı
ise public counter.

---

## CT.4 — LLVM Backend ve Speculation Barrier

V1 codegen disiplini:

1. **Tip kontrol fazında** tüm CT001-CT008 derleme hatasıdır — yanlış kod
   LLVM IR aşamasına ulaşmaz.
2. **IR seviyesinde** `sabitsüre<T>` ↔ `T` aynı IR tipi (zero-overhead).
3. **Speculation barrier**: bir işlev `sabitsüre<T>` döndürüyorsa veya
   parametresi varsa, gövdesinin **başına** bir `lfence` (x86) veya
   `dsb sy + isb` (ARM64) emisyonu yapılır — Spectre v1 mitigation.
4. **Declassification noktasında** (`ifşa(s)`) ek bir speculation barrier
   emisyonu — kontrol akışı public hale dönerken speculative window kapanır.

Pratikte minimal emisyon: `call void @llvm.x86.sse2.lfence()` (x86) veya
`call void @llvm.aarch64.isb(i32 15)` (ARM). LLVM'nin
`-mspeculative-load-hardening` flag'i aktif derleme önerilir; KEMGU compiler
çıktısı LLVM IR text olduğundan, ileride `--ct-strict` flag'i ile clang'a
geçilebilir.

V1 kapsamı (basit emisyon): Her `sabitsüre_yarat` çağrısının LLVM
karşılığından **sonra** ve her `ifşa` çağrısından **sonra** bir tek
`call void @llvm.x86.sse2.lfence()` yerleştirilir. Bu, hem mental modelin
test edilmesini sağlar hem de ileride genişletilebilir bir hook'tur.

V2 (gelecek): seçenek olarak `-mllvm -x86-slh-loads` ve `--ct-strict`
flag ile clang'ı bilgilendir; ARM SVE/NEON için ayrı barrier; Rust'taki
`std::hint::black_box` benzeri optimizer-opaque marker.

---

## CT.5 — Hata Kodları Özeti

| Kod | Anlam | Tipik Yer |
|-----|-------|-----------|
| CT001 | SABITSURE_IF_BRANCH       | `eğer/iken/eşleş` koşulu sabitsüre |
| CT002 | SABITSURE_INDEX           | `arr[sabitsüre_idx]` — cache-timing |
| CT003 | SABITSURE_LEAK            | sabitsüre→T implicit (atama, çağrı, ver) |
| CT004 | SABITSURE_DIVMOD          | sabitsüre üzerinde `/` veya `%` |
| CT005 | SABITSURE_PRODUCER_ARITY  | `sabitsüre_yarat(...)` 1 arg gerekir |
| CT006 | SABITSURE_WRAP_INVALID    | yetenekli olmayan T (kesirli, metin, nesting) |
| CT007 | SABITSURE_DECLASS_ARITY   | `ifşa(...)` 1 arg + operand sabitsüre |
| CT008 | SABITSURE_SHIFT_AMOUNT    | kaydırma miktarı sabitsüre |

---

## CT.6 — Stdlib Tasarımı (V2'de uygulanır)

İlk sürümde **API taslakları**, implementasyon V2'de:

- `kripto::ct_eq(a: Dizi<sabitsüre<dtam8>>, b: Dizi<sabitsüre<dtam8>>) -> sabitsüre<mantıksal>`
  — sabit-süre byte karşılaştırma (OpenSSL `CRYPTO_memcmp` muadili).
- `kripto::ct_seç(c: sabitsüre<mantıksal>, t: sabitsüre<T>, f: sabitsüre<T>) -> sabitsüre<T>`
  — sabit-süre seçim (LLVM `select` deyimi).
- `kripto::ct_xor(a: sabitsüre<T>, b: sabitsüre<T>) -> sabitsüre<T>`
  — XOR (zaten + ile aynı kural, kolaylık).
- `kripto::otp_anahtar() -> sabitsüre<Dizi<dtam8>>`
  — random pool → sabitsüre wrap (tekkez ile combine gelecek).

V2 hedef:
- AES-NI sabit-süre table-free implementation (sabitsüre dizi operasyonları).
- Curve25519 sabit-süre Montgomery ladder.
- HKDF, HMAC-SHA2 — tüm intermediate'ler sabitsüre.

---

## CT.7 — Test Minimum Sayısı: **30**

Test dağılımı (`test/test_sabitsure.c`):

| Grup | Test # | Konu |
|------|--------|------|
| S1   | 1-4    | Tip ifadesi + sabitsüre_yarat (producer) — pozitif |
| S2   | 5-8    | Aritmetik taint yayılımı (+, ^, &, <<) — pozitif |
| S3   | 9-12   | `ifşa` declassification — pozitif |
| S4   | 13-16  | CT001 if/while/match branch — negatif |
| S5   | 17-19  | CT002 array index — negatif |
| S6   | 20-22  | CT003 implicit downgrade (atama, ver, çağrı) — negatif |
| S7   | 23-25  | CT004 div/mod — negatif |
| S8   | 26-28  | CT006 invalid wrap (kesirli, metin, nesting) — negatif |
| S9   | 29-32  | Timing attack senaryoları (RSA, AES, OTP, ECDSA k) |

30+ test eşiği checkpoint tetikleyicidir.

---

## CT.8 — Uygulama Sırası

1. **Lexer**: `sabitsüre` keyword (toplam 34).
2. **AST**: `DUGUM_TIP_SABITSURE` + `.veri.tip_sabitsure.ic_tip`.
3. **Parser**: `parse_tip` → `sabitsüre<T>`.
4. **Tip sistemi**: `TIP_SABITSURE` kategori + `tip_olustur_sabitsure` +
   `tip_sabitsure_mi`. Recursive nominal eşitlik, yazdırma.
5. **Tip kontrol** (kritik kısım):
   - `sabitsüre_yarat(...)` producer intrinsic
   - `ifşa(...)` declassification intrinsic
   - CT001 branch yasağı: DUGUM_EGER.kosul, DUGUM_IKEN.kosul, DUGUM_ESLES.deger
   - CT002 index yasağı: DUGUM_INDEKS.indeks
   - CT003 downgrade yasağı: tip_belirle_beklenen — beklenen T iken arg sabitsüre<T>
   - CT004 div/mod: DUGUM_IKILI(OP_BOLU/OP_MOD)
   - CT006 wrap check: ast_tip_to_bilgi'de yetenekli tip kontrolü
   - CT008 shift: DUGUM_IKILI(OP_SOLA_KAYDIR/OP_SAGA_KAYDIR) sağ taraf
6. **LLVM**: minimal — `sabitsüre<T>` IR olarak `T`; `sabitsüre_yarat` ve
   `ifşa` çağrılarından sonra `call void @llvm.x86.sse2.lfence()` emisyonu
   (target=x86; ARM ileride).
7. **`test/test_sabitsure.c`**: 30+ test, ASan temiz.
8. **Örnek**: `test/ornekler/otp_anahtar.kem` (timing-safe OTP karşılaştırma
   pseudo-kod; tip kontrol geçer).

---

## CT.9 — Spec Sınırları (V2'ye Bırakılanlar)

- **`sabitsüre_seç`** built-in (CT-safe ternary): `mantıksal x sabitsüre<T> x sabitsüre<T> -> sabitsüre<T>`.
- **Sabitsüre yapı** alanları (`yapı sabitsüre RSAState { ... }`).
- **Inter-procedural taint** (call graph analiz; v1 yerel kural).
- **Speculation barrier auto-placement** (LLVM pass).
- **ARM64 `dsb sy + isb` emisyonu** (v1 x86 lfence).
- **Stdlib `kripto::ct_*` modülü** (V2'de implement).
- **`sabitsüre<tekkez<T>>`** birleşimi (linear + CT).
- **Hertzbleed mitigation**: DVFS pinning (OS-level, dil dışı).

---

## CT.10 — Tasarım Gerekçeleri (Niye Bu Yol?)

**Niye Rust'taki `subtle` crate yetersiz?**
Rust `subtle::ConstantTimeEq` trait olarak elden çalışır — programcı her
operasyonu manuel olarak `subtle::Choice` ile sarmalı. Tip sisteminde
zorlama yok; bir `if secret_value == ...` kodu sessizce compile olur.
KEMGU'da tip sistemi bunu **derleme zamanında** reddeder.

**Niye Go `crypto/subtle.ConstantTimeCompare` yetersiz?**
Aynı: opt-in, manuel. Yan kanalları kapatmak programcı sorumluluğu.
"Forgot to use ConstantTimeCompare" — CVE-2023-39532 (etcd) gibi gerçek
örnekler bunu kanıtlar.

**Niye Jasmin/FaCT (DSL'ler) yerine genel-amaçlı KEMGU?**
Jasmin/FaCT kripto-DSL'ler — sadece küçük kernel kodu için kullanılır.
KEMGU genel-amaçlı bir sistem dili olduğundan, OS, TLS stack, P2P
protocol, OTP key handling vb. **aynı dilde** yazılır. Kripto kodu
"diğer her şeyin" içine gömülüdür.

**Niye `gizli` yerine `sabitsüre`?**
"Gizli" semantik (bilgi-akış); "sabitsüre" operasyonel (constant-time
disiplin). KEMGU'nun amacı semantik bilgi akışı değil, ölçülebilir
operasyonel garanti.

**Niye iki ayrı keyword (sabitsüre + ifşa) yerine bir + builtin?**
Keyword tabloyu mümkün olduğunca küçük tutuyoruz (33 → 34, +1 yerine +2).
`ifşa` semantik olarak "declassification operatörü" ama runtime'da no-op;
built-in işlev olarak çözmek lexer ve parser tablosunu sade tutuyor.
Spec içinde her zaman explicit yazılır; programcı için kavramsal fark yok.

---
