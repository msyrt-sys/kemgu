# stdlib — `opsiyonel.kem`

`seçimlik<T>` (opsiyonel değer) üzerine kurulu fonksiyonel yardımcı kütüphane. Modül tamamen **saf KEMGU**'dur — hiçbir runtime/FFI bağımlılığı yoktur. Tüm işlemler `değer(v)` / `hiç` desenlerinin **pattern matching** (`eşleş`) ile ayrıştırılması üzerine kuruludur; null (`hiç` referansı) yerine `seçimlik<T>` ile değerin var/yok olduğu açıkça temsil edilir.

Tasarım kuralı: Java `Optional.get()` / `equals` tuzağına düşmemek için API tamamen fonksiyoneldir (değeri "zorla çıkar" eden bir işlev yoktur; `ya_da_varsayilan` / `ya_da_cagir` ile güvenli çözüm yapılır). Generic callback altyapısı (`harita`, `filtre`, `bagla`, `ya_da_cagir`) çok-parametreli tip çıkarsama (Madde D) sonrası tam generic hâle gelmiştir.

## v1 sınırlamaları

- `bos_olustur<T>` parametrik olarak `hiç` üretemez: `T` bilgisini bir `rumuz: T` parametresinden taşır (gerçekte kullanılmaz). Kaynak notu: "parametrik olarak boş üretmek için `<T>` bilgisi gerekiyor; şu an sadece concrete tipte test edilebilir."
- Concrete `_tam` / `_metin` varyantları yalnızca geri uyumluluk için korunmaktadır. Yeni kod generic (`harita<T,U>`, `filtre<T>` vb.) versiyonları kullanmalıdır.
- Lambda gövdeleri ifade-form (block-form gövde tip çıkarsama henüz yok); örneklerdeki lambdalar buna uygundur.

---

## Sorgular (tam generic)

### var_mi

```kemgu
işlev var_mi<T>(opt: seçimlik<T>) -> mantıksal
```

Ne yapar: `opt` bir değer içeriyorsa (`değer(v)`) `doğru`, `hiç` ise `yanlış` döner.

Örnek:

```kemgu
değişken x: seçimlik<tam32> = değer(5);
ver var_mi(x);   // doğru
```

Kenar durum: `hiç` girdisinde `yanlış` döner.

### yok_mu

```kemgu
işlev yok_mu<T>(opt: seçimlik<T>) -> mantıksal
```

Ne yapar: `opt` `hiç` ise `doğru`, değer içeriyorsa `yanlış` döner (`var_mi`'nin tersi).

Örnek:

```kemgu
değişken x: seçimlik<tam32> = hiç;
ver yok_mu(x);   // doğru
```

Kenar durum: `değer(v)` girdisinde `yanlış` döner.

### bos_mu

```kemgu
işlev bos_mu<T>(opt: seçimlik<T>) -> mantıksal
```

Ne yapar: `yok_mu` için Türkçe okuma kolaylığı sağlayan takma ad (alias); doğrudan `yok_mu(opt)` çağırır.

Örnek:

```kemgu
değişken x: seçimlik<metin> = hiç;
ver bos_mu(x);   // doğru
```

Kenar durum: `yok_mu` ile aynı davranış — `değer(v)` girdisinde `yanlış`.

---

## Çözüm (tam generic)

### ya_da_varsayilan

```kemgu
işlev ya_da_varsayilan<T>(opt: seçimlik<T>, varsayilan: T) -> T
```

Ne yapar: `opt` değer içeriyorsa içerideki değeri (`v`), `hiç` ise `varsayilan` argümanını döner. Optional'dan `T` çıkarmanın güvenli yolu.

Örnek:

```kemgu
değişken x: seçimlik<tam32> = hiç;
ver ya_da_varsayilan(x, 42);   // 42
```

Kenar durum: `hiç` girdisinde her zaman `varsayilan` döner (panik / hata yok).

### ilk_var

```kemgu
işlev ilk_var<T>(a: seçimlik<T>, b: seçimlik<T>) -> seçimlik<T>
```

Ne yapar: İki opsiyonelden ilk değer içereni döner. `a` değer ise `a`, değilse `b` döner (kısa devre; `b`'nin içeriğine bakılmaz, olduğu gibi döner).

Örnek:

```kemgu
değişken a: seçimlik<tam32> = hiç;
değişken b: seçimlik<tam32> = değer(7);
ver ilk_var(a, b);   // değer(7)
```

Kenar durum: İkisi de `hiç` ise sonuç `b` (yani `hiç`); `a` değer ise `b`'ye hiç bakılmaz.

### her_iki_var

```kemgu
işlev her_iki_var<T>(a: seçimlik<T>, b: seçimlik<T>) -> seçimlik<T>
```

Ne yapar: İkisi de değer içeriyorsa `a`'yı döner; biri (veya ikisi) eksikse `hiç` döner. (Mantıksal "VE": ikisi de var olmalı; sonuç olarak `a` seçilir.)

Örnek:

```kemgu
değişken a: seçimlik<tam32> = değer(1);
değişken b: seçimlik<tam32> = değer(2);
ver her_iki_var(a, b);   // değer(1)  (a döner)
```

Kenar durum: `a` değer ama `b` `hiç` → `hiç`; `a` `hiç` → (b'ye bakılmadan) `hiç`.

---

## Eşitlik (her T için)

### esit_mi

```kemgu
işlev esit_mi<T>(a: seçimlik<T>, b: seçimlik<T>) -> mantıksal
```

Ne yapar: İki opsiyonelin değer-eşitliğini hesaplar. İkisi de değer ise içerikler `==` ile karşılaştırılır; ikisi de `hiç` ise `doğru`; biri değer biri `hiç` ise `yanlış` döner.

Örnek:

```kemgu
değişken a: seçimlik<tam32> = değer(5);
değişken b: seçimlik<tam32> = değer(5);
ver esit_mi(a, b);   // doğru

değişken c: seçimlik<tam32> = hiç;
değişken d: seçimlik<tam32> = hiç;
ver esit_mi(c, d);   // doğru
```

Kenar durum: `hiç == hiç` → `doğru`; tek taraf `hiç` → `yanlış`. (İçerik karşılaştırması `T` üzerinde `==` gerektirir.)

---

## Yapıcılar (yardımcı)

### sarmala (yardımcı)

```kemgu
işlev sarmala<T>(x: T) -> seçimlik<T>
```

Ne yapar: `x` değerini `değer(x)` olarak sarmalar. Doğrudan `değer(x)` da yazılabilir; bu sarmalayıcı, fonksiyonel pipeline'larda (`harita`/`bagla` vb.) işlev referansı olarak kullanışlıdır.

Örnek:

```kemgu
ver sarmala(10);   // değer(10)
```

Kenar durum: Her zaman `değer` üretir; asla `hiç` dönmez.

### bos_olustur (yardımcı)

```kemgu
işlev bos_olustur<T>(rumuz: T) -> seçimlik<T>
```

Ne yapar: `hiç` (boş seçimlik) üretir. `rumuz: T` parametresi yalnızca `T` tip bilgisini taşımak içindir; **gövdede kullanılmaz**.

Örnek:

```kemgu
ver bos_olustur(0);   // hiç  (T = tam32 olarak çıkarsanır)
```

Kenar durum / sınırlama: Parametresiz olarak (sadece `<T>` ile) `hiç` üretmek henüz mümkün değil — `T`'yi taşımak için bir argüman vermek gerekir; kaynak notu: "şu an sadece concrete tipte test edilebilir."

---

## Harita / Filtre / Bağla (tam generic — Madde D sonrası)

### harita

```kemgu
işlev harita<T, U>(opt: seçimlik<T>, f: işlev(T) -> U) -> seçimlik<U>
```

Ne yapar: `opt` değer ise içerideki `v`'ye `f` uygulanır ve `değer(f(v))` döner; `hiç` ise `hiç` döner. (Functor `map`.)

Örnek:

```kemgu
değişken x: seçimlik<tam32> = değer(21);
ver harita(x, |n: tam32| n * 2);   // değer(42)
```

Kenar durum: `hiç` girdisinde `f` hiç çağrılmaz, sonuç `hiç`.

### filtre

```kemgu
işlev filtre<T>(opt: seçimlik<T>, pred: işlev(T) -> mantıksal) -> seçimlik<T>
```

Ne yapar: `opt` değer ise `pred(v)` çağrılır; `doğru` ise `değer(v)` korunur, `yanlış` ise `hiç` döner. `hiç` girdisinde doğrudan `hiç`.

Örnek:

```kemgu
değişken x: seçimlik<tam32> = değer(8);
ver filtre(x, |n: tam32| n > 5);   // değer(8)
ver filtre(x, |n: tam32| n > 100); // hiç
```

Kenar durum: `hiç` girdisinde `pred` çağrılmaz, sonuç `hiç`.

### bagla

```kemgu
işlev bagla<T, U>(opt: seçimlik<T>, f: işlev(T) -> seçimlik<U>) -> seçimlik<U>
```

Ne yapar: `opt` değer ise `f(v)` çağrılır ve `f`'in döndürdüğü `seçimlik<U>` olduğu gibi döner; `hiç` ise `hiç`. (Monad `flatMap` / `bind` karşılığı; iç içe `seçimlik` katmanını düzleştirir.)

Örnek:

```kemgu
işlev yari<U>(n: tam32) -> seçimlik<tam32> {
    eğer n % 2 == 0 { ver değer(n / 2); }
    ver hiç;
}
değişken x: seçimlik<tam32> = değer(10);
ver bagla(x, yari);   // değer(5)
```

Kenar durum: `hiç` girdisinde `f` çağrılmaz, sonuç `hiç`. `f` `hiç` döndürürse sonuç da `hiç`.

### ya_da_cagir

```kemgu
işlev ya_da_cagir<T>(opt: seçimlik<T>, f: işlev() -> T) -> T
```

Ne yapar: `opt` değer ise içerideki `v` döner; `hiç` ise tembel (lazy) üretici `f()` çağrılır ve sonucu döner. `ya_da_varsayilan`'ın tembel sürümü — varsayılan ancak gerektiğinde hesaplanır.

Örnek:

```kemgu
değişken x: seçimlik<tam32> = hiç;
ver ya_da_cagir(x, || pahali_hesap());   // pahali_hesap() çağrılır
```

Kenar durum: Değer mevcutsa `f` hiç çağrılmaz (yan etkiler tetiklenmez). Kaynakta kollardan sonra ayrıca `ver f();` bulunur; bu yalnızca tip çıkarsamanın `f()`'ten `T`'yi çıkarması içindir — kol gövdeleri her ihtimali kapsadığından pratikte erişilmez.

---

## Harita / Filtre / Bağla (tam32 — concrete, geri uyumluluk)

> Bu varyantlar yalnızca **geri uyumluluk** için korunur. Yeni kod yukarıdaki generic sürümleri (`harita`, `filtre`, `bagla`, `ya_da_cagir`) kullanmalıdır.

### harita_tam (yardımcı / geri uyumluluk)

```kemgu
işlev harita_tam(opt: seçimlik<tam32>, f: işlev(tam32) -> tam32) -> seçimlik<tam32>
```

Ne yapar: `harita<T,U>`'nun `tam32`'ye özelleşmiş hâli. Değer ise `değer(f(v))`, `hiç` ise `hiç`.

Örnek:

```kemgu
değişken x: seçimlik<tam32> = değer(20);
ver harita_tam(x, |n: tam32| n + 1);   // değer(21)
```

Kenar durum: `hiç` girdisinde `f` çağrılmaz, sonuç `hiç`.

### filtre_tam (yardımcı / geri uyumluluk)

```kemgu
işlev filtre_tam(opt: seçimlik<tam32>, pred: işlev(tam32) -> mantıksal) -> seçimlik<tam32>
```

Ne yapar: `filtre<T>`'nin `tam32` özelleşmesi. Değer ise `pred(v)`; `doğru` ise korunur, `yanlış` ise `hiç`.

Örnek:

```kemgu
değişken x: seçimlik<tam32> = değer(3);
ver filtre_tam(x, |n: tam32| n > 0);   // değer(3)
```

Kenar durum: `hiç` girdisinde `pred` çağrılmaz, sonuç `hiç`.

### bagla_tam (yardımcı / geri uyumluluk)

```kemgu
işlev bagla_tam(opt: seçimlik<tam32>, f: işlev(tam32) -> seçimlik<tam32>) -> seçimlik<tam32>
```

Ne yapar: `bagla<T,U>`'nun `tam32` özelleşmesi (flatMap). Değer ise `f(v)` döner, `hiç` ise `hiç`.

Örnek:

```kemgu
değişken x: seçimlik<tam32> = değer(4);
ver bagla_tam(x, |n: tam32| değer(n * n));   // değer(16)
```

Kenar durum: `hiç` girdisinde `f` çağrılmaz; `f` `hiç` dönerse sonuç `hiç`.

### ya_da_cagir_tam (yardımcı / geri uyumluluk)

```kemgu
işlev ya_da_cagir_tam(opt: seçimlik<tam32>, f: işlev() -> tam32) -> tam32
```

Ne yapar: `ya_da_cagir<T>`'nin `tam32` özelleşmesi. Değer ise `v`, `hiç` ise `f()` döner.

Örnek:

```kemgu
değişken x: seçimlik<tam32> = hiç;
ver ya_da_cagir_tam(x, || 99);   // 99
```

Kenar durum: Değer mevcutsa `f` çağrılmaz. Generic sürümden farklı olarak kollardan sonra fallback `ver 0;`'dır (kollar her ihtimali kapsadığından pratikte erişilmez).

---

## Harita / Filtre (metin)

### harita_metin (yardımcı / geri uyumluluk)

```kemgu
işlev harita_metin(opt: seçimlik<metin>, f: işlev(metin) -> metin) -> seçimlik<metin>
```

Ne yapar: `harita`'nın `metin`'e özelleşmiş hâli. Değer ise `değer(f(v))`, `hiç` ise `hiç`.

Örnek:

```kemgu
değişken s: seçimlik<metin> = değer("ali");
ver harita_metin(s, |m: metin| buyut(m));   // değer("ALI")  (buyut örnek bir dönüşüm)
```

Kenar durum: `hiç` girdisinde `f` çağrılmaz, sonuç `hiç`.

### filtre_metin (yardımcı / geri uyumluluk)

```kemgu
işlev filtre_metin(opt: seçimlik<metin>, pred: işlev(metin) -> mantıksal) -> seçimlik<metin>
```

Ne yapar: `filtre`'nin `metin` özelleşmesi. Değer ise `pred(v)`; `doğru` ise korunur, `yanlış` ise `hiç`.

Örnek:

```kemgu
değişken s: seçimlik<metin> = değer("merhaba");
ver filtre_metin(s, |m: metin| bos_degil(m));   // değer("merhaba")
```

Kenar durum: `hiç` girdisinde `pred` çağrılmaz, sonuç `hiç`.
