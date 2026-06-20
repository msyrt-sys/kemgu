# Stdlib — Temel Matematik (`matematik.kem`)

Bu modül, KEMGU standart kütüphanesinin temel matematik işlevlerini sağlar:
mutlak değer, en küçük/en büyük, kare/küp, sınırlama, işaret, kuvvet, kök,
modulo, asallık, fibonacci, faktöriyel, parite testleri, OBEB/EKOK (tekil ve
dizi), modüler üs, kombinatorik (kombinasyon/permütasyon) ve sayı palindromu.

Modül tamamen **saf KEMGU**'dur — hiçbir runtime/FFI bağımlılığı yoktur ve
`import` kullanmaz (self-contained). İşlevlerin çoğu generic'tir (`<T>`) ve
derleme sırasında her sayısal tip için ayrı instantiation üretilir. Tasarım
kuralı: her işlev tek sorumluluk, açık adlandırma, yan-etki yok.

## Genel v1 Sınırlamaları

- **Literal tip çıkarsama sınırı:** `2` gibi tamsayı literalleri henüz generic
  `T`'ye atanamadığından, asallık/fibonacci/faktöriyel gibi literal sabit
  gerektiren işlevler generic değil, sabit `tam32` tipinde yazılmıştır. Generic
  versiyonları, constraint satisfaction (Sayı/Tam özelliği) tamamlandıktan sonra
  gelecektir.
- **Taşma (overflow):** Tamsayı tipleri (`tam32` vb.) C tarzı serbest taşma
  yerine tipe göre kontrollüdür; ancak işlevler taşmayı otomatik tespit etmez.
  Belirli işlevler (`fibonacci` ~46. terim, `faktoriyel` 13!) taşar — bu sınırlar
  ilgili işlev bölümünde belirtilmiştir. Taşma riski olan hesaplar (modüler üs,
  kombinatorik, palindrom ters çevirme) ara değerleri `tam64`'te tutar.
- **Constraint yokluğu:** `bir<T>()` gibi tip-nötr sabit üreticileri henüz
  yazılamadığından bazı işlevler (örn. `kuvvet`) kenar durumlarda uzlaşımsal
  davranış sergiler (aşağıda belirtilmiştir).

---

## mutlak

```
işlev mutlak<T>(x: T) -> T
```

Ne yapar: `x`'in mutlak değerini döndürür. Negatifse `0 - x`, değilse `x`.

Örnek:

```
değişken d: tam32 = mutlak(-7);   // d = 7
```

Kenar durum: `x == 0` için `0` döner. `0` ve pozitif değerler olduğu gibi
döndürülür.

---

## en_kucuk

```
işlev en_kucuk<T>(a: T, b: T) -> T
```

Ne yapar: `a` ve `b`'den küçük olanı döndürür.

Örnek:

```
değişken m: tam32 = en_kucuk(3, 8);   // m = 3
```

Kenar durum: `a == b` olduğunda `b` döner (eşitlikte ikinci argüman).

---

## en_buyuk

```
işlev en_buyuk<T>(a: T, b: T) -> T
```

Ne yapar: `a` ve `b`'den büyük olanı döndürür.

Örnek:

```
değişken m: tam32 = en_buyuk(3, 8);   // m = 8
```

Kenar durum: `a == b` olduğunda `b` döner (eşitlikte ikinci argüman).

---

## kare

```
işlev kare<T>(x: T) -> T
```

Ne yapar: `x * x` değerini döndürür. Generic; her sayısal tip için çalışır.

Örnek:

```
değişken k: tam32 = kare(5);   // k = 25
```

Kenar durum: Sonuç tip `T`'de hesaplanır — büyük `x` değerlerinde taşma
mümkündür (tipe bağlı).

---

## kup

```
işlev kup<T>(x: T) -> T
```

Ne yapar: `x * x * x` değerini döndürür.

Örnek:

```
değişken k: tam32 = kup(3);   // k = 27
```

Kenar durum: Sonuç tip `T`'de hesaplanır — büyük `x` değerlerinde taşma
mümkündür (tipe bağlı).

---

## sinirla

```
işlev sinirla<T>(x: T, alt: T, ust: T) -> T
```

Ne yapar: `x`'i `[alt, ust]` aralığına çeker (clamp). `x < alt` ise `alt`,
`x > ust` ise `ust`, aksi halde `x`.

Örnek:

```
değişken s: tam32 = sinirla(12, 0, 10);   // s = 10
değişken t: tam32 = sinirla(-3, 0, 10);   // t = 0
```

Kenar durum: Aralık içindeki değerler (sınırlar dahil) olduğu gibi döner.
`alt > ust` gibi geçersiz aralıklar kontrol edilmez — bu durumda `x < alt`
kontrolü önce çalışır.

---

## isaret

```
işlev isaret<T>(x: T) -> tam32
```

Ne yapar: `x`'in işaretini döndürür: negatif için `-1`, sıfır için `0`,
pozitif için `+1`. Daima `tam32` döndürür.

Örnek:

```
değişken i: tam32 = isaret(-42);   // i = -1
değişken j: tam32 = isaret(0);     // j = 0
```

Kenar durum: `x == 0` için `0` döner.

---

## kuvvet

```
işlev kuvvet<T>(x: T, n: tam32) -> T
```

Ne yapar: `x` tabanını `n` defa kendisiyle çarparak `x^n`'i hesaplar (iteratif).

Örnek:

```
değişken p: tam32 = kuvvet(2, 10);   // p = 1024
```

Kenar durum: `n == 0` için **`x` döner** (1 değil). Bunun sebebi, constraint
sistemi olmadan tip-nötr `bir<T>()` (birim eleman) yazılamamasıdır — bu v1
uzlaşımıdır. Büyük üslerde sonuç tipi `T`'de taşabilir.

---

## kok_tam

```
işlev kok_tam(n: tam32) -> tam32
```

Ne yapar: `n`'in tamsayı karekökünü (taban) Newton-Raphson yöntemiyle hesaplar.

Örnek:

```
değişken r: tam32 = kok_tam(17);   // r = 4
```

Kenar durum: `n < 2` için `n` olduğu gibi döner (yani `0 -> 0`, `1 -> 1`).
Yorumda belirtilen "n < 0 için 0 döner" niyetine karşılık, koddaki guard
`n < 2 { ver n; }` olduğundan negatif `n` argümanı doğrudan iterasyona girmez
ve `n` döner. Negatif girdi tanımlı kullanım dışıdır.

---

## mod

```
işlev mod<T>(a: T, b: T) -> T
```

Ne yapar: `a % b` (modulo/kalan) değerini döndürür. Generic; tüm sayısal tipler.

Örnek:

```
değişken r: tam32 = mod(17, 5);   // r = 2
```

Kenar durum: `b == 0` için bölme/modulo sıfır davranışı kontrol edilmez —
çağıranın `b != 0` sağlaması gerekir.

---

## asal_mi

```
işlev asal_mi(n: tam32) -> mantıksal
```

Ne yapar: `n`'in asal olup olmadığını deneme bölmesiyle (trial division)
belirler. Çift sayıları eler, sonra `3`'ten başlayarak `i * i <= n` olana kadar
tek bölenleri dener.

Örnek:

```
değişken b: mantıksal = asal_mi(13);   // b = doğru
```

Kenar durum: `n < 2` için `yanlış`; `n == 2` için `doğru`; çift `n` (>2) için
`yanlış`.

---

## fibonacci

```
işlev fibonacci(n: tam32) -> tam32
```

Ne yapar: 0-tabanlı Fibonacci dizisinin `n`. terimini hesaplar
(`fib(0)=0`, `fib(1)=1`, `fib(2)=1`, ...). İteratif.

Örnek:

```
değişken f: tam32 = fibonacci(10);   // f = 55
```

Kenar durum: `n < 2` için `n` döner. **Taşma:** `tam32` döndürür; yaklaşık 46.
terimde taşar. `tam64` versiyonu için ayrı bir `fibonacci_genis` imzası
planlanmıştır (tip int conversion olmadığından ayrı imza gerekir — v1'de henüz
mevcut değil).

---

## faktoriyel

```
işlev faktoriyel(n: tam32) -> tam32
```

Ne yapar: `n!` (faktöriyel) değerini iteratif olarak hesaplar.

Örnek:

```
değişken f: tam32 = faktoriyel(5);   // f = 120
```

Kenar durum: `n < 1` için `1` döner (yani `n == 0` ve negatif `n` için `1`).
**Taşma:** `tam32` döndürür; `12!` sığar, `13!` taşar.

---

## cift_mi

```
işlev cift_mi<T>(n: T) -> mantıksal
```

Ne yapar: `n % 2 == 0` olup olmadığını döndürür (çift sayı testi).

Örnek:

```
değişken b: mantıksal = cift_mi(4);   // b = doğru
```

Kenar durum: `0` çift kabul edilir (`doğru`). Negatif çift sayılarda davranış
modulo'nun işaret semantiğine bağlıdır.

---

## tek_mi

```
işlev tek_mi<T>(n: T) -> mantıksal
```

Ne yapar: `n % 2 != 0` olup olmadığını döndürür (tek sayı testi).

Örnek:

```
değişken b: mantıksal = tek_mi(7);   // b = doğru
```

Kenar durum: `0` tek değildir (`yanlış`). Negatif tek sayılarda davranış
modulo'nun işaret semantiğine bağlıdır.

---

## negatif_mi

```
işlev negatif_mi<T>(x: T) -> mantıksal
```

Ne yapar: `x < 0` olup olmadığını döndürür.

Örnek:

```
değişken b: mantıksal = negatif_mi(-1);   // b = doğru
```

Kenar durum: `x == 0` için `yanlış`.

---

## pozitif_mi

```
işlev pozitif_mi<T>(x: T) -> mantıksal
```

Ne yapar: `x > 0` olup olmadığını döndürür (sıfır dahil **değil**).

Örnek:

```
değişken b: mantıksal = pozitif_mi(5);   // b = doğru
```

Kenar durum: `x == 0` için `yanlış` (sıfır pozitif sayılmaz).

---

## sifir_mi

```
işlev sifir_mi<T>(x: T) -> mantıksal
```

Ne yapar: `x == 0` olup olmadığını döndürür.

Örnek:

```
değişken b: mantıksal = sifir_mi(0);   // b = doğru
```

Kenar durum: Yalnızca tam sıfır için `doğru`.

---

## iki_kat

```
işlev iki_kat<T>(x: T) -> T
```

Ne yapar: `x * 2` (ikiyle çarpım) değerini döndürür.

Örnek:

```
değişken d: tam32 = iki_kat(21);   // d = 42
```

Kenar durum: Büyük `x` değerlerinde tip `T`'de taşma mümkündür.

---

## yarisi

```
işlev yarisi<T>(x: T) -> T
```

Ne yapar: `x / 2` (ikiye bölüm) değerini döndürür.

Örnek:

```
değişken y: tam32 = yarisi(10);   // y = 5
```

Kenar durum: Tamsayı tiplerinde tam sayı bölmesi yapılır (`yarisi(7) -> 3`,
kalan atılır).

---

## ikili_obeb

```
işlev ikili_obeb(a: tam32, b: tam32) -> tam32
```

Ne yapar: İki tamsayının en büyük ortak bölenini (OBEB/GCD) Öklid algoritmasıyla
hesaplar.

Örnek:

```
değişken g: tam32 = ikili_obeb(48, 36);   // g = 12
```

Kenar durum: `ikili_obeb(0, x) = x` (yorumda belirtilen invaryant). Negatif
girdiler için işaret davranışı tanımlı kullanım dışıdır — `obeb_dizi`/`ekok_dizi`
çağrılarında girdiler önce `mutlak` ile normalleştirilir.

---

## ikili_ekok

```
işlev ikili_ekok(a: tam32, b: tam32) -> tam32
```

Ne yapar: İki tamsayının en küçük ortak katını (EKOK/LCM) hesaplar. Taşmayı
önlemek için önce böler: `(a / OBEB(a, b)) * b`.

Örnek:

```
değişken l: tam32 = ikili_ekok(4, 6);   // l = 12
```

Kenar durum: `a == 0` veya `b == 0` ise `0` döner.

---

## obeb_dizi

```
işlev obeb_dizi(xs: Dizi<tam32>) -> tam32
```

Ne yapar: Bir dizideki tüm sayıların OBEB'ini hesaplar. Akümülatör `0`'dan
başlar (`ikili_obeb(0, x) = x` invaryantı sayesinde doğru başlangıç) ve her
eleman `mutlak` ile normalleştirilir.

Örnek:

```
değişken g: tam32 = obeb_dizi([12, 18, 24]);   // g = 6
```

Kenar durum: **Boş dizi -> `0`**. Tek eleman -> o elemanın mutlak değeri.
Negatifler mutlak değere indirgenir.

---

## ekok_dizi

```
işlev ekok_dizi(xs: Dizi<tam32>) -> tam32
```

Ne yapar: Bir dizideki tüm sayıların EKOK'unu hesaplar. Akümülatör `1`'den
(çarpımsal birim) başlar ve her eleman `mutlak` ile normalleştirilir.

Örnek:

```
değişken l: tam32 = ekok_dizi([4, 6, 8]);   // l = 24
```

Kenar durum: **Boş dizi -> `1`** (çarpımsal birim). Herhangi bir eleman `0` ise
sonuç `0`. Negatifler mutlak değere indirgenir.

---

## us_mod

```
işlev us_mod(taban: tam32, us: tam32, m: tam32) -> tam32
```

Ne yapar: `(taban^us) mod m` değerini hızlı üs (kareleme/square-and-multiply)
yöntemiyle hesaplar. Ara çarpımlar `tam64`'te tutularak `tam32` taşması önlenir.
Negatif taban modüle göre normalleştirilir.

Örnek:

```
değişken r: tam32 = us_mod(2, 10, 1000);   // r = 24  (1024 mod 1000)
```

Kenar durum: `m == 1` için `0`. `us >= 0` ve `m >= 1` varsayılır (bu önkoşullar
kontrol edilmez). Negatif `taban`, `t < 0` ise `t + m` ile pozitife çekilir.

---

## kombinasyon

```
işlev kombinasyon(n: tam32, k: tam32) -> tam64
```

Ne yapar: Binom katsayısı `C(n, k)`'yı çarpımsal formülle hesaplar. Her adımda
tam bölünme garantilidir; ara değerler `tam64`'te tutulur. Simetri kullanılarak
`k = min(k, n - k)` alınır ve iterasyon sayısı azaltılır.

Örnek:

```
değişken c: tam64 = kombinasyon(5, 2);   // c = 10
```

Kenar durum: `k < 0` veya `k > n` için `0`. `k == 0` -> `1`.

---

## permutasyon

```
işlev permutasyon(n: tam32, k: tam32) -> tam64
```

Ne yapar: `P(n, k) = n! / (n - k)! = n · (n-1) ··· (n-k+1)` permütasyon sayısını
hesaplar. Ara değerler `tam64`'te tutulur.

Örnek:

```
değişken p: tam64 = permutasyon(5, 2);   // p = 20
```

Kenar durum: `k < 0` veya `k > n` için `0`. `k == 0` -> `1`.

---

## palindrom_mu

```
işlev palindrom_mu(n: tam32) -> mantıksal
```

Ne yapar: Bir sayının ondalık basamaklarının simetrik (palindrom) olup
olmadığını döndürür. Sayıyı `tam64`'te ters çevirir (`tam32` ters çevirmenin
taşmasını önlemek için) ve orijinaliyle karşılaştırır.

Örnek:

```
değişken b: mantıksal = palindrom_mu(12321);   // b = doğru
değişken c: mantıksal = palindrom_mu(123);     // c = yanlış
```

Kenar durum: Negatif `n` -> `yanlış`. `0` -> `doğru`.
