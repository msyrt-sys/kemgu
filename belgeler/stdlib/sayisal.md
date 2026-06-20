# Stdlib — `sayisal` (Sayısal Yardımcılar)

`stdlib/temel/sayisal.kem` modülü, tamsayı ve kesirli tipler için "hesap
makinesi seviyesinde" temel sayısal algoritmalar sağlar. Tüm işlevler
**generic** (`<T>`) tanımlıdır ve derleme zamanında her somut tip için ayrı
özelleşmiş kod üretilir (monomorphization). Modül saf KEMGU'dur; runtime veya
FFI bağımlılığı yoktur ve aritmetik operatörler (`+`, `-`, `*`, `/`, `%`),
karşılaştırma ile `iken` döngüsü gibi dil primitifleri üzerine kuruludur.

**Genel v1 sınırlamaları:**
- Generic'lerin gerçek bir **constraint sistemi** henüz yoktur. Bu yüzden bir
  literali (`1` gibi) `T` tipine güvenli biçimde üretmek mümkün değildir —
  `us` işlevinin `n == 0` durumundaki davranışı bu kısıtlamadan etkilenir
  (aşağıya bakın).
- Taşma (overflow) koruması zayıftır; küçük tiplerde ara hesaplar taşabilir.
- `obe`/`ekok` Öklid algoritması `%` operatörü gerektirdiğinden pratikte
  tamsayı tipleriyle anlamlıdır.

---

## İşlevler

### ortalama

```kem
işlev ortalama<T>(a: T, b: T) -> T {
    ver (a + b) / 2;
}
```

**Ne yapar:** İki sayının aritmetik ortalamasını `(a + b) / 2` ile hesaplar.

**Örnek:**
```kem
ortalama(10, 20)   // -> 15
```

**Kenar durum:** Toplam `a + b` ara hesabı tipin sınırını aşarsa taşar
(kaynak yorumu: taşma riski tam giderilmemiştir; ileride "güvenli versiyon"
planlanıyor). Tamsayı tipinde `/ 2` tam bölme olduğundan tek toplamlarda
sonuç aşağı yuvarlanır (ör. `ortalama(10, 21)` -> `15`).

---

### us

```kem
işlev us<T>(x: T, n: tam32) -> T {
    ...
}
```

**Ne yapar:** `x` değerinin `n`'inci kuvvetini (`x^n`) ardışık çarpma ile
hesaplar. `sonuc` başlangıçta `x`'tir ve döngü `i = 1`'den `n`'e kadar `x` ile
çarpar.

**Örnek:**
```kem
us(2, 3)   // -> 8   (2 * 2 * 2)
us(5, 1)   // -> 5
```

**Kenar durum (v1 sınırı — kaynaktan):**
- `n` **negatif olmamalıdır** (sözleşme gereği; negatif `n` için döngü hiç
  çalışmaz ve `x` döner — anlamlı değildir).
- `n == 0` durumu **doğru `1` döndüremez.** Generic constraint sistemi
  olmadığından `1`'i `T` tipinde üretmek mümkün değildir; kaynak yorumunda bu
  açıkça "özür dilerim sonucu" olarak nitelenir ve fonksiyon `1` yerine
  başlangıç değeri `x`'i (`sonuc`) döndürür. Pratik öneri: işlevi her zaman
  `n > 0` ile çağırın. (`sifir<T>()` / `bir<T>()` helper'ları veya constraint
  sistemi geldiğinde düzeltilecektir.)

---

### obe

```kem
işlev obe<T>(a: T, b: T) -> T {
    ...
}
```

**Ne yapar:** İki tamsayının en büyük ortak bölenini (OBE / GCD) Öklid
algoritması ile hesaplar. `y != 0` olduğu sürece `x % y` ile kalan alınarak
ilerlenir; döngü bittiğinde `x` döner.

**Örnek:**
```kem
obe(48, 36)   // -> 12
obe(17, 5)    // -> 1   (aralarında asal)
```

**Kenar durum:**
- `b == 0` ise döngü hiç çalışmaz ve `a` (yani `x`) döner — `obe(a, 0) == a`.
- `a == 0` ve `b != 0` ise ilk iterasyonda `x` ve `y` yer değiştirir, sonuçta
  `b` döner.
- `%` operatörü gerektirdiğinden tamsayı tipleriyle kullanılması beklenir.

---

### ekok

```kem
işlev ekok<T>(a: T, b: T) -> T {
    ver (a * b) / obe(a, b);
}
```

**Ne yapar:** İki tamsayının en küçük ortak katını (EKOK / LCM)
`(a * b) / obe(a, b)` formülüyle hesaplar. İçeride `obe` işlevini çağırır.

**Örnek:**
```kem
ekok(4, 6)    // -> 12
ekok(21, 6)   // -> 42
```

**Kenar durum:**
- `a == 0` ve `b == 0` ise `obe(0, 0)` `0` döner ve `(a * b) / 0` **sıfıra
  bölme** üretir — bu girdiden kaçınılmalıdır.
- `a * b` ara çarpımı tipin sınırını aşabilir (taşma riski); büyük değerlerde
  dikkatli olunmalıdır. (`obe(a, b)`'ye bölme yapılması taşma olasılığını
  tek başına gidermez, çünkü çarpım bölmeden önce yapılır.)

---

## Notlar

Modüldeki dört işlevin tamamı üst düzey public'tir. `obe`, `ekok` tarafından
dahili olarak da kullanılır; ayrı bir gizli (private) yardımcı işlev
bulunmamaktadır.
