# stdlib::dizi — `Dizi<T>` Yardımcıları

`Dizi<T>` üzerinde çalışan generic algoritmaların ve sorguların yer aldığı modül. Tamamen saf KEMGU ile yazılmıştır; hiçbir runtime/FFI bağımlılığı yoktur. Modül; sorgular (uzunluk, boşluk), erişim (`ilk`/`son`/`al`), arama, yerinde mutasyon (`ters_cevir`/`sirala`), fold/reduce, predicate tabanlı sorgular, gerçek generic functional API (`harita`/`filtre`/`indirgeme`) ve skaler küme işlemleri (`kesisim`/`birlesim`/`fark`) gibi katmanlar üzerine kuruludur. Tasarım kuralı eager evaluation'dır; iterator yoktur (Rust/C++ STL karmaşasından bilinçli kaçınılır).

## Genel v1 sınırlamaları

- **Allocator gelişimi:** Modülün ilk katmanı yazılırken derleyici dizi için dinamik allocator sunmuyordu; bu yüzden o dönem yeni-dizi döndürmesi gereken işlemler (`harita`, `filtre`, `dilimle`, `birleştir`) **in-place** veya **fold** stilinde verildi (`harita_yerinde_tam`, `say_tam` gibi). Adım 7 ile heap dizi literali + generic callback çıkarsama geldikten sonra gerçek generic `harita`/`filtre`/`indirgeme` ve küme işlemleri `dizi_olustur`/`dizi_ekle` primitifleri üzerine eklendi.
- **Callback'ler:** İlk katmandaki callback alan işlevler (`indirgeme_tam`, `say_tam`, `tum_mu_tam`, `bir_mu_tam`, `harita_yerinde_tam`) v1'de tam32 odaklıdır; constraint sistemi sonrası generic hale gelecektir. Adım 7 ile gelen `harita`/`filtre`/`indirgeme` ise gerçekten generic'tir.
- **Generic kısıtlar:** Arama işlevleri (`icerir`, `bul`, `indeks_bul`, küme işlemleri, `pozisyonlar`, `say_esit`, `tekrar_eden_mi`, `esit_mi`) T tipinin `==` desteklemesini gerektirir. `sirala` ise T tipinin `<` (`>`) desteklemesini gerektirir.
- **Codegen F4 sınırı (önemli):** Fonksiyon-dönüşü bir diziyi ÇAĞIRAN scope'ta doğrudan indekslemek (`s = kesisim(a,b); s[0]`) codegen sınırına takılır. Bunun yerine sonucu `uzunluk`/`toplam_tam` gibi parametre-üzerinden-indirgeyen bir işleve geçirin.

---

## Sorgular (tam generic)

### uzunluk

```
işlev uzunluk<T>(xs: Dizi<T>) -> tam32
```

Ne yapar: Dizideki eleman sayısını döner (döngüyle sayar).

Örnek:
```
değişken n: tam32 = uzunluk([10, 20, 30]);  // n = 3
```

Kenar durum: Boş dizi için `0` döner.

### bos_mu

```
işlev bos_mu<T>(xs: Dizi<T>) -> mantıksal
```

Ne yapar: Dizinin boş olup olmadığını (`uzunluk == 0`) döner.

Örnek:
```
ver bos_mu([]);  // doğru
```

Kenar durum: Boş dizi için `doğru`, en az bir elemanlı dizi için `yanlış`.

### dolu_mu

```
işlev dolu_mu<T>(xs: Dizi<T>) -> mantıksal
```

Ne yapar: Dizide en az bir eleman bulunup bulunmadığını (`uzunluk > 0`) döner.

Örnek:
```
ver dolu_mu([1]);  // doğru
```

Kenar durum: Boş dizi için `yanlış`.

---

## Erişim (tam generic)

### ilk

```
işlev ilk<T>(xs: Dizi<T>) -> seçimlik<T>
```

Ne yapar: İlk elemanı `değer(...)` olarak döner.

Örnek:
```
ver ilk([5, 6, 7]);  // değer(5)
```

Kenar durum: Dizi boşsa `hiç` döner.

### son

```
işlev son<T>(xs: Dizi<T>) -> seçimlik<T>
```

Ne yapar: Son elemanı (`xs[n - 1]`) `değer(...)` olarak döner.

Örnek:
```
ver son([5, 6, 7]);  // değer(7)
```

Kenar durum: Dizi boşsa (`n == 0`) `hiç` döner.

### al

```
işlev al<T>(xs: Dizi<T>, i: tam32) -> seçimlik<T>
```

Ne yapar: `i` indeksindeki elemanı güvenli biçimde `değer(...)` olarak döner.

Örnek:
```
ver al([5, 6, 7], 1);  // değer(6)
```

Kenar durum: `i < 0` ise `hiç`; `i >= uzunluk` ise `hiç`. Sınır dışı erişimde panik/UB yerine güvenli `hiç` döner.

---

## Arama (T tipi `==` destekli olmalı)

### icerir

```
işlev icerir<T>(xs: Dizi<T>, hedef: T) -> mantıksal
```

Ne yapar: Diziye eşit (`==`) bir eleman olup olmadığını döner.

Örnek:
```
ver icerir([1, 2, 3], 2);  // doğru
```

Kenar durum: Boş dizi veya eşleşme yoksa `yanlış`.

### bul

```
işlev bul<T>(xs: Dizi<T>, hedef: T) -> seçimlik<T>
```

Ne yapar: `hedef`'e eşit ilk elemanı `değer(...)` olarak döner.

Örnek:
```
ver bul([4, 8, 8], 8);  // değer(8) (ilk eşleşme)
```

Kenar durum: Eşleşme yoksa `hiç`.

### indeks_bul

```
işlev indeks_bul<T>(xs: Dizi<T>, hedef: T) -> seçimlik<tam32>
```

Ne yapar: `hedef`'e eşit ilk elemanın indeksini `değer(...)` olarak döner.

Örnek:
```
ver indeks_bul([4, 8, 8], 8);  // değer(1)
```

Kenar durum: Eşleşme yoksa `hiç`.

---

## Yerinde mutasyon (in-place, generic)

### ters_cevir

```
işlev ters_cevir<T>(xs: Dizi<T>) -> Dizi<T>
```

Ne yapar: Diziyi yerinde (in-place) ters çevirir ve aynı diziyi döner (zincirleme kullanım için). İki uçtan ortaya doğru takas yapar.

Örnek:
```
ver ters_cevir([1, 2, 3]);  // [3, 2, 1]
```

Kenar durum: `uzunluk < 2` ise (boş veya tek elemanlı) dizi değiştirilmeden olduğu gibi döner.

### sirala

```
işlev sirala<T>(xs: Dizi<T>) -> Dizi<T>
```

Ne yapar: Diziyi yerinde artan sırada sıralar (bubble sort, `>` ile komşu takası) ve aynı diziyi döner. T tipi `<`/`>` destekli olmalıdır.

Örnek:
```
ver sirala([3, 1, 2]);  // [1, 2, 3]
```

Kenar durum: `uzunluk < 2` ise dizi olduğu gibi döner. **Performans:** O(n²) — küçük diziler için yeterlidir; hızlı sıralama constraint sistemi + recursive helper API ile sonra eklenecek (kaynak notu).

---

## Fold / Reduce (concrete tam32)

### indirgeme_tam

```
işlev indirgeme_tam(xs: Dizi<tam32>, baslangic: tam32,
                    op: işlev(tam32, tam32) -> tam32) -> tam32
```

Ne yapar: Sol-fold uygular. `baslangic` değerinden başlayıp her elemana `op(birikim, eleman)` uygular.

Örnek:
```
indirgeme_tam([1, 2, 3], 0, |a, b| a + b);  // ((0+1)+2)+3 = 6
```

Kenar durum: Boş dizide `baslangic` doğrudan döner.

### toplam_tam

```
işlev toplam_tam(xs: Dizi<tam32>) -> tam32
```

Ne yapar: `Dizi<tam32>` elemanlarının toplamını döner.

Örnek:
```
ver toplam_tam([10, 20, 12]);  // 42
```

Kenar durum: Boş dizide `0` döner.

### carpim_tam

```
işlev carpim_tam(xs: Dizi<tam32>) -> tam32
```

Ne yapar: `Dizi<tam32>` elemanlarının çarpımını döner.

Örnek:
```
ver carpim_tam([2, 3, 7]);  // 42
```

Kenar durum: Boş dizide `1` döner (çarpımın birim elemanı).

### min_tam

```
işlev min_tam(xs: Dizi<tam32>) -> seçimlik<tam32>
```

Ne yapar: En küçük elemanı `değer(...)` olarak döner.

Örnek:
```
ver min_tam([3, 1, 2]);  // değer(1)
```

Kenar durum: Dizi boşsa `hiç` döner.

### maks_tam

```
işlev maks_tam(xs: Dizi<tam32>) -> seçimlik<tam32>
```

Ne yapar: En büyük elemanı `değer(...)` olarak döner.

Örnek:
```
ver maks_tam([3, 1, 2]);  // değer(3)
```

Kenar durum: Dizi boşsa `hiç` döner.

---

## Predicate tabanlı (concrete tam32)

### say_tam

```
işlev say_tam(xs: Dizi<tam32>, pred: işlev(tam32) -> mantıksal) -> tam32
```

Ne yapar: Predicate'i (`pred`) karşılayan eleman sayısını döner.

Örnek:
```
say_tam([1, 2, 3, 4], |x| x > 2);  // 2
```

Kenar durum: Boş dizide `0` döner.

### tum_mu_tam

```
işlev tum_mu_tam(xs: Dizi<tam32>, pred: işlev(tam32) -> mantıksal) -> mantıksal
```

Ne yapar: Bütün elemanların predicate'i karşılayıp karşılamadığını döner.

Örnek:
```
tum_mu_tam([2, 4, 6], |x| x > 0);  // doğru
```

Kenar durum: Boş dizide `doğru` döner (vacuous truth). Predicate'i karşılamayan ilk elemanda kısa devre yaparak `yanlış` döner.

### bir_mu_tam

```
işlev bir_mu_tam(xs: Dizi<tam32>, pred: işlev(tam32) -> mantıksal) -> mantıksal
```

Ne yapar: En az bir elemanın predicate'i karşılayıp karşılamadığını döner.

Örnek:
```
bir_mu_tam([1, 2, 3], |x| x == 2);  // doğru
```

Kenar durum: Boş dizide `yanlış` döner. İlk eşleşmede kısa devre yaparak `doğru` döner.

---

## Yerinde harita (concrete tam32)

### harita_yerinde_tam

```
işlev harita_yerinde_tam(xs: Dizi<tam32>,
                          f: işlev(tam32) -> tam32) -> Dizi<tam32>
```

Ne yapar: Her elemanı `f(eleman)` ile yerinde değiştirir ve aynı diziyi döner.

Örnek:
```
ver harita_yerinde_tam([1, 2, 3], |x| x * 2);  // [2, 4, 6]
```

Kenar durum: Boş dizide hiçbir değişiklik olmadan dizi döner. **Not:** Yeni `Dizi` dönen pure harita için (eskiden) dinamik alloc primitifi gerekiyordu (KIRMIZI_QUEUE); bu işlev şimdilik mutasyon kullanır. Generic ve yeni-dizi dönen sürüm için aşağıdaki `harita`'ya bakın.

---

## Eşitlik

### esit_mi

```
işlev esit_mi<T>(a: Dizi<T>, b: Dizi<T>) -> mantıksal
```

Ne yapar: İki diziyi eleman-eleman karşılaştırır; tüm konumlar eşitse `doğru` döner. T tipi `==`/`!=` destekli olmalıdır.

Örnek:
```
ver esit_mi([1, 2, 3], [1, 2, 3]);  // doğru
```

Kenar durum: Uzunluklar farklıysa (`na != nb`) erken `yanlış` döner. İki boş dizi `doğru`. İlk eşitsiz konumda kısa devre yaparak `yanlış` döner.

---

## Generic functional API (Adım 7: heap dizi literal + generic callback)

> Bu katman, heap dizi literali (Madde B v2) ve generic callback tip çıkarsama (Madde D v2) ile birleştirilerek gerçekten generic hale getirildi. `dizi_olustur`/`dizi_ekle` primitifleri üzerine kuruludur.

### harita

```
işlev harita<T, U>(xs: Dizi<T>, f: işlev(T) -> U) -> Dizi<U>
```

Ne yapar: Her elemana `f` uygular ve yeni bir `Dizi<U>` döner. `T` ve `U`, `f`'in imzasından çıkarsanır. Sonuç dizisi `uzunluk(xs)` kapasiteyle oluşturulur.

Örnek:
```
harita([1, 2, 3], |x| x * 10);  // yeni Dizi<tam32>: [10, 20, 30]
```

Kenar durum: Boş dizide boş `Dizi<U>` döner. Dönen diziyi çağıran scope'ta doğrudan indekslemeyin (codegen F4 sınırı); `uzunluk`/`toplam_tam` gibi bir işleve geçirin.

### filtre

```
işlev filtre<T>(xs: Dizi<T>, pred: işlev(T) -> mantıksal) -> Dizi<T>
```

Ne yapar: Predicate'i karşılayan elemanları yeni bir `Dizi<T>`'de toplar.

Örnek:
```
filtre([1, 2, 3, 4], |x| x > 2);  // yeni Dizi<tam32>: [3, 4]
```

Kenar durum: Hiçbir eleman karşılamazsa veya girdi boşsa boş `Dizi<T>` döner. Dönen diziyi doğrudan indekslemeyin (codegen F4 sınırı).

### indirgeme

```
işlev indirgeme<T, U>(xs: Dizi<T>, baslangic: U,
                      op: işlev(U, T) -> U) -> U
```

Ne yapar: Sol-fold (birikim). `baslangic`'tan başlayıp her elemana `op(birikim, eleman)` uygular. `T` ve `U` çıkarsanır.

Örnek:
```
indirgeme(xs, 0, |a, b| a + b);  // toplam
```

Kenar durum: Boş dizide `baslangic` doğrudan döner.

---

## Skaler küme işlemleri + konum/sayım sorguları

> Tümü `==` destekli T tipler için generic. Küme işlemleri sonuç dizisini **tekrarsız** (set semantiği) üretir: aynı değer en fazla bir kez. Yeni `Dizi` döndürenler mevcut allocator (`dizi_olustur`/`dizi_ekle`) üzerine kuruludur. Dönen dizileri çağıran scope'ta doğrudan indekslemeyin (codegen F4 sınırı) — sonucu parametre-üzerinden-indirgeyen bir işleve geçirin.

### kesisim

```
işlev kesisim<T>(a: Dizi<T>, b: Dizi<T>) -> Dizi<T>
```

Ne yapar: Hem `a`'da hem `b`'de bulunan elemanları tekrarsız olarak yeni bir `Dizi<T>`'de döner (`a`'nın sırasıyla).

Örnek:
```
kesisim([1, 2, 2, 3], [2, 3, 4]);  // [2, 3] (tekrarsız)
```

Kenar durum: Ortak eleman yoksa veya bir girdi boşsa boş dizi. `a`'daki tekrarlar sonuçta yalnızca bir kez yer alır (set semantiği).

### birlesim

```
işlev birlesim<T>(a: Dizi<T>, b: Dizi<T>) -> Dizi<T>
```

Ne yapar: `a` veya `b`'de bulunan tüm elemanları tekrarsız olarak döner (`a` önce, ardından `b`'nin yeni elemanları).

Örnek:
```
birlesim([1, 2], [2, 3]);  // [1, 2, 3]
```

Kenar durum: Her iki girdi boşsa boş dizi. Tekrar eden değerler (girdi içi veya girdiler arası) sonuçta tek kez yer alır.

### fark

```
işlev fark<T>(a: Dizi<T>, b: Dizi<T>) -> Dizi<T>
```

Ne yapar: `a`'da olup `b`'de olmayan elemanları tekrarsız olarak döner.

Örnek:
```
fark([1, 2, 3], [2]);  // [1, 3]
```

Kenar durum: `a`'nın tüm elemanları `b`'de varsa veya `a` boşsa boş dizi. `a`'daki tekrarlar sonuçta tek kez yer alır.

### pozisyonlar

```
işlev pozisyonlar<T>(xs: Dizi<T>, hedef: T) -> Dizi<tam32>
```

Ne yapar: `hedef`'e eşit elemanların TÜM indekslerini yeni bir `Dizi<tam32>`'de döner.

Örnek:
```
pozisyonlar([5, 8, 5, 8], 8);  // [1, 3]
```

Kenar durum: Eşleşme yoksa boş dizi döner (`indeks_bul`'un aksine yalnız ilkini değil tümünü döner).

### son_n

```
işlev son_n<T>(xs: Dizi<T>, n: tam32) -> Dizi<T>
```

Ne yapar: Son `n` elemanı yeni bir `Dizi<T>`'de döner.

Örnek:
```
son_n([1, 2, 3, 4], 2);  // [3, 4]
```

Kenar durum: `n <= 0` ise boş dizi. `n >= uzunluk` ise tüm elemanların kopyası (başlangıç indeksi 0'a sınırlanır).

### say_esit

```
işlev say_esit<T>(xs: Dizi<T>, hedef: T) -> tam32
```

Ne yapar: `hedef` değere eşit eleman sayısını döner.

Örnek:
```
say_esit([5, 8, 5, 8], 8);  // 2
```

Kenar durum: Eşleşme yoksa veya dizi boşsa `0`.

### tekrar_eden_mi

```
işlev tekrar_eden_mi<T>(xs: Dizi<T>) -> mantıksal
```

Ne yapar: Herhangi bir elemanın birden fazla kez geçip geçmediğini döner (O(n²) çift karşılaştırma).

Örnek:
```
tekrar_eden_mi([1, 2, 2]);  // doğru
 tekrar_eden_mi([1, 2, 3]); // yanlış
```

Kenar durum: Boş veya tek elemanlı dizide `yanlış`. İlk tekrar bulunduğunda kısa devre yaparak `doğru` döner.
