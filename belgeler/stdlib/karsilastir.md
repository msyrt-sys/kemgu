# Stdlib — `karsilastir`

`stdlib/temel/karsilastir.kem` modülü, generic eşitlik ve sıralama yardımcıları sağlar. Tüm fonksiyonlar saf KEMGU ile yazılmıştır (runtime/FFI bağımlılığı yoktur) ve dilin temel `==`, `!=`, `<`, `>` operatörleri ile `için` / `iken` döngüleri üstüne kuruludur. Java'nın `equals`/`hashCode` tuzağına düşmemek için tasarım **fonksiyonel stildedir** (free function) — method tabanlı API yoktur.

## Genel v1 sınırlamaları

- **`kendin` (self) henüz parser'da desteklenmiyor**, bu yüzden modül tamamen serbest fonksiyonlar kullanır; method tabanlı API ileride `kendin` desteği geldiğinde eklenecektir.
- **`<` / `>` gerektiren fonksiyonlar sadece sayısal `T` içindir.** `metin` sıralaması codegen'de yoktur (yalnız `metin_esit` ile `==` desteklenir), bu yüzden `karsilastir`, `karsilastir_ters`, `sirali_mi`, `ters_sirali_mi`, `dizi_karsilastir`, `dizi_esit_mi`, `en_kucuk_dizi`, `en_buyuk_dizi`, `en_kucuk_uc`, `en_buyuk_uc` fonksiyonları `metin`/`Dizi<metin>` ile çağrılmamalıdır.
- Modül kendi kendine yeterlidir (import yoktur); bu yüzden dizi uzunluğu için yerel bir `dizi_uzunluk` helper'ı içerir.

---

## `esit_mi`

```kemgu
işlev esit_mi<T>(a: T, b: T) -> mantıksal {
    ver a == b;
}
```

**Ne yapar:** İki değerin eşit olup olmadığını döndürür (`==` operatörünün generic sarmalayıcısı).

**Örnek:**

```kemgu
ver esit_mi(3, 3);   // doğru
ver esit_mi(3, 4);   // yanlış
```

**Kenar durum:** Davranış tamamen `==` operatörüne bağlıdır. `metin` için `==` (metin_esit) desteklenir, dolayısıyla `esit_mi` metin ile kullanılabilir.

---

## `farkli_mi`

```kemgu
işlev farkli_mi<T>(a: T, b: T) -> mantıksal {
    ver a != b;
}
```

**Ne yapar:** İki değerin farklı olup olmadığını döndürür (`!=` operatörünün generic sarmalayıcısı).

**Örnek:**

```kemgu
ver farkli_mi(3, 4);   // doğru
ver farkli_mi(3, 3);   // yanlış
```

**Kenar durum:** Davranış `!=` operatörüne bağlıdır; `esit_mi`'nin tersidir.

---

## `karsilastir`

```kemgu
işlev karsilastir<T>(a: T, b: T) -> tam32 {
    eğer a < b { ver 0 - 1; }
    eğer a > b { ver 1; }
    ver 0;
}
```

**Ne yapar:** Üç değerli karşılaştırma yapar — `a < b` ise `-1`, `a == b` ise `0`, `a > b` ise `+1` döndürür.

**Örnek:**

```kemgu
ver karsilastir(2, 5);   // -1
ver karsilastir(5, 5);   //  0
ver karsilastir(7, 5);   // +1
```

**Kenar durum:** `<` / `>` gerektirir; yalnız sayısal `T` içindir (`metin` ile kullanılmamalı). Negatif sonuç `0 - 1` ile üretilir (codegen'de tam32 literal `-1` yerine).

---

## `en_kucuk_uc`

```kemgu
işlev en_kucuk_uc<T>(a: T, b: T, c: T) -> T {
    değişken m: T = a;
    eğer b < m { m = b; }
    eğer c < m { m = c; }
    ver m;
}
```

**Ne yapar:** Üç değerden en küçüğünü döndürür.

**Örnek:**

```kemgu
ver en_kucuk_uc(8, 3, 5);   // 3
```

**Kenar durum:** `<` gerektirir; sayısal `T` içindir. Eşitlik durumunda ilk karşılaşılan (sırayla `a`, ardından `b`) değer korunur — `b < m` / `c < m` katı küçüktür kontrolü kullanır.

---

## `en_buyuk_uc`

```kemgu
işlev en_buyuk_uc<T>(a: T, b: T, c: T) -> T {
    değişken m: T = a;
    eğer b > m { m = b; }
    eğer c > m { m = c; }
    ver m;
}
```

**Ne yapar:** Üç değerden en büyüğünü döndürür.

**Örnek:**

```kemgu
ver en_buyuk_uc(8, 3, 5);   // 8
```

**Kenar durum:** `>` gerektirir; sayısal `T` içindir. Eşitlik durumunda ilk karşılaşılan değer korunur (katı büyüktür kontrolü).

---

## `karsilastir_ters`

```kemgu
işlev karsilastir_ters<T>(a: T, b: T) -> tam32 {
    ver karsilastir(b, a);
}
```

**Ne yapar:** `karsilastir`'ın işaretini çevirir (azalan sıralama için ters karşılaştırıcı). `a < b` için `+1`, `a > b` için `-1` döndürür; `(b, a)` ile çağırmaya eşdeğerdir.

**Örnek:**

```kemgu
ver karsilastir_ters(2, 5);   // +1
ver karsilastir_ters(7, 5);   // -1
ver karsilastir_ters(5, 5);   //  0
```

**Kenar durum:** `karsilastir`'a delege ettiği için aynı kısıtları taşır — sayısal `T` içindir (`metin` ile kullanılmamalı).

---

## `dizi_uzunluk` (yardımcı)

```kemgu
işlev dizi_uzunluk<T>(xs: Dizi<T>) -> tam32 {
    değişken n: tam32 = 0;
    için x: xs {
        n = n + 1;
    }
    ver n;
}
```

**Ne yapar:** Bir dizinin eleman sayısını `için` döngüsüyle sayarak döndürür. Modül import kullanmadığı için yerel bir helper olarak tanımlanmıştır.

**Örnek:**

```kemgu
ver dizi_uzunluk([10, 20, 30]);   // 3
```

**Kenar durum:** Boş dizi için `0` döner. `T` üzerinde herhangi bir operatör gerektirmez (yalnız iterasyon yapar).

---

## `sirali_mi`

```kemgu
işlev sirali_mi<T>(xs: Dizi<T>) -> mantıksal {
    değişken n: tam32 = dizi_uzunluk(xs);
    eğer n < 2 { ver doğru; }
    değişken i: tam32 = 1;
    iken i < n {
        eğer xs[i - 1] > xs[i] { ver yanlış; }
        i = i + 1;
    }
    ver doğru;
}
```

**Ne yapar:** Dizinin azalmayan (non-decreasing) sırada olup olmadığını döndürür.

**Örnek:**

```kemgu
ver sirali_mi([1, 2, 2, 5]);   // doğru
ver sirali_mi([1, 3, 2]);      // yanlış
```

**Kenar durum:** Boş dizi veya tek elemanlı dizi (`n < 2`) için `doğru` döner. Eşit ardışık elemanlara izin verir (yalnız katı azalış `>` ile yanlış sayılır). `>` gerektirir; sayısal `T` içindir.

---

## `ters_sirali_mi`

```kemgu
işlev ters_sirali_mi<T>(xs: Dizi<T>) -> mantıksal {
    değişken n: tam32 = dizi_uzunluk(xs);
    eğer n < 2 { ver doğru; }
    değişken i: tam32 = 1;
    iken i < n {
        eğer xs[i - 1] < xs[i] { ver yanlış; }
        i = i + 1;
    }
    ver doğru;
}
```

**Ne yapar:** Dizinin artmayan (non-increasing) sırada olup olmadığını döndürür.

**Örnek:**

```kemgu
ver ters_sirali_mi([5, 2, 2, 1]);   // doğru
ver ters_sirali_mi([5, 1, 2]);      // yanlış
```

**Kenar durum:** Boş dizi veya tek elemanlı dizi (`n < 2`) için `doğru` döner. Eşit ardışık elemanlara izin verir (yalnız katı artış `<` ile yanlış sayılır). `<` gerektirir; sayısal `T` içindir.

---

## `dizi_karsilastir`

```kemgu
işlev dizi_karsilastir<T>(a: Dizi<T>, b: Dizi<T>) -> tam32 {
    değişken na: tam32 = dizi_uzunluk(a);
    değişken nb: tam32 = dizi_uzunluk(b);
    değişken i: tam32 = 0;
    iken i < na {
        eğer i >= nb { ver 1; }   // a daha uzun, ortak prefix eşit -> a büyük
        değişken c: tam32 = karsilastir(a[i], b[i]);
        eğer c != 0 { ver c; }
        i = i + 1;
    }
    eğer nb > na { ver 0 - 1; }    // b daha uzun -> a küçük
    ver 0;
}
```

**Ne yapar:** İki diziyi sözlük (lexicographic) sırasına göre karşılaştırır — `a < b` ise `-1`, eşit ise `0`, `a > b` ise `+1` döndürür. Eleman eleman `karsilastir<T>` kullanır; ortak önek (prefix) eşitse daha kısa olan dizi küçük sayılır.

**Örnek:**

```kemgu
ver dizi_karsilastir([1, 2], [1, 3]);      // -1 (2 < 3)
ver dizi_karsilastir([1, 2, 3], [1, 2]);   // +1 (a daha uzun, prefix eşit)
ver dizi_karsilastir([1, 2], [1, 2]);      //  0 (eşit)
```

**Kenar durum:**
- `a` tükenir ama `b` daha uzunsa (`nb > na`) `-1` döner (a küçük).
- `b` önce tükenirse (`i >= nb`) `+1` döner (a büyük).
- Her iki dizi boşsa döngü hiç çalışmaz ve `0` döner.
- `karsilastir`'a delege ettiği için `<` / `>` gerektirir; sayısal `T` içindir (`Dizi<metin>` ile çağrılmamalı).

---

## `dizi_esit_mi`

```kemgu
işlev dizi_esit_mi<T>(a: Dizi<T>, b: Dizi<T>) -> mantıksal {
    ver dizi_karsilastir(a, b) == 0;
}
```

**Ne yapar:** İki dizinin eşit olup olmadığını döndürür — `dizi_karsilastir` sonucu `0` ise eşittir.

**Örnek:**

```kemgu
ver dizi_esit_mi([1, 2, 3], [1, 2, 3]);   // doğru
ver dizi_esit_mi([1, 2], [1, 2, 3]);      // yanlış
```

**Kenar durum:** İki boş dizi eşit kabul edilir (`doğru`). `dizi_karsilastir` üzerinden çalıştığı için `<` / `>` destekli sayısal `T` içindir (`metin` ile kullanılmamalı).

---

## `en_kucuk_dizi`

```kemgu
işlev en_kucuk_dizi<T>(xs: Dizi<T>) -> seçimlik<T> {
    değişken n: tam32 = dizi_uzunluk(xs);
    eğer n == 0 { ver hiç; }
    değişken m: T = xs[0];
    için x: xs {
        eğer x < m { m = x; }
    }
    ver değer(m);
}
```

**Ne yapar:** Dizideki en küçük elemanı `seçimlik<T>` olarak döndürür. `sayisal` modülündeki `tam32` tabanlı `dizi_min`'in generic karşılığıdır.

**Örnek:**

```kemgu
ver en_kucuk_dizi([8, 3, 5]);   // değer(3)
ver en_kucuk_dizi([]);          // hiç
```

**Kenar durum:** Boş dizi (`n == 0`) için `hiç` döner; aksi halde `değer(m)` döner. `<` gerektirir; sayısal `T` içindir.

---

## `en_buyuk_dizi`

```kemgu
işlev en_buyuk_dizi<T>(xs: Dizi<T>) -> seçimlik<T> {
    değişken n: tam32 = dizi_uzunluk(xs);
    eğer n == 0 { ver hiç; }
    değişken m: T = xs[0];
    için x: xs {
        eğer x > m { m = x; }
    }
    ver değer(m);
}
```

**Ne yapar:** Dizideki en büyük elemanı `seçimlik<T>` olarak döndürür.

**Örnek:**

```kemgu
ver en_buyuk_dizi([8, 3, 5]);   // değer(8)
ver en_buyuk_dizi([]);          // hiç
```

**Kenar durum:** Boş dizi (`n == 0`) için `hiç` döner; aksi halde `değer(m)` döner. `>` gerektirir; sayısal `T` içindir.
