# Stdlib — `sonuc.kem`

`sonuc` modülü, KEMGU'nun yerleşik `sonuç<T, E>` tipinin etrafına saran çözümleme, dönüştürme ve birleştirme yardımcıları sağlar. Modül exception/`Optional.get()` tuzağından kaçınmak için tasarlanmıştır: her hata durumu açık ve ertelenmiş (deferred) biçimde ele alınır, C++ exception unwinding maliyeti yoktur. API iki paralel katmandan oluşur: yerleşik `sonuç<T, E>` üzerine kurulu **native API** (Madde C/D parser fix ve generic callback inference ile çalışır) ve parser fix gelene kadar geçici olarak korunan, `seçimlik` çiftine dayalı yapı tabanlı **`KSonuc<T, E>` API**'si. Tüm çözümleme `eşleş` ile `tamam(v)` / `hata(e)` (ya da `değer(v)` / `hiç`) desenleri üzerinden yapılır.

## Genel v1 Sınırlamaları

- **CODEGEN KISITI (native API):** `sonuç<T, E>` DÖNDÜREN generic fonksiyonlar (`tamam_yap`, `hata_yap`, `harita`, `harita_hata`, `bagla`, `opsiyonel_to_sonuc`), `E` tipi `i32`'ye inmeyen bir tiple (ör. `metin` → `ptr`) instantiate edilince `--llvm` codegen'inde yanlış IR üretir (dönüş struct'ında `E` `i32` varsayılır). Tip kontrolünden geçer ama codegen bozulur. `E` tamsayı olduğunda doğru çalışır.
  - **Geçici çözüm:** Hata tipi olarak tamsayı kod kullanın; ya da doğrudan `tamam(v)` / `hata(m)` yazıp sonucu hemen `eşleş` ile tüketin (struct'ı fonksiyon sınırından geçirmeyin). Bu codegen sınırı stdlib dışıdır.
- **`KSonuc<T, E>` (geçici yapı tabanlı API):** Parser fix gelene kadar korunan paralel API. `seçimlik<T>` + `seçimlik<E>` çiftiyle temsil edilir. `harita` / `harita_hata` / `bagla` / `dene` muadilleri yalnızca somut `tam32`/`metin` tipleri için sağlanır (generic callback v2'de gelecek). Yeni kod native `sonuç<T, E>` tercih etmelidir.

---

## Native `sonuç<T, E>` API

### tamam_yap

```
işlev tamam_yap<T, E>(d: T) -> sonuç<T, E>
```

Verilen değeri başarılı bir `sonuç` (`tamam`) olarak sarar. Generic context'te derleyici tip çıkarsamasını netleştiren bir yapıcı wrapper'dır.

Örnek:

```
değişken r: sonuç<tam32, tam32> = tamam_yap(42);  // tamam(42)
```

Kenar durum: `sonuç<T, E>` döndürdüğü için yukarıdaki CODEGEN KISITI geçerlidir — `E` tamsayı olmayan bir tiple instantiate edilirse codegen bozulur. Doğrudan `tamam(d)` yazmakla aynı semantiğe sahiptir.

### hata_yap

```
işlev hata_yap<T, E>(h: E) -> sonuç<T, E>
```

Verilen hata değerini başarısız bir `sonuç` (`hata`) olarak sarar. Generic context'te tip çıkarsamasını netleştiren yapıcı wrapper'dır.

Örnek:

```
değişken r: sonuç<tam32, tam32> = hata_yap(404);  // hata(404)
```

Kenar durum: CODEGEN KISITI gereği `E` tamsayı olmalı; doğrudan `hata(h)` ile eşdeğerdir.

### basarili_mi

```
işlev basarili_mi<T, E>(r: sonuç<T, E>) -> mantıksal
```

Sonuç `tamam` ise `doğru`, `hata` ise `yanlış` döner.

Örnek:

```
basarili_mi(tamam(7))   // doğru
basarili_mi(hata(1))    // yanlış
```

Kenar durum: İki koldan biri kesinlikle eşleşir; gövde sonundaki `ver yanlış;` yalnızca tip kontrolünü tamamlayan ulaşılamaz bir varsayılandır.

### basarisiz_mi

```
işlev basarisiz_mi<T, E>(r: sonuç<T, E>) -> mantıksal
```

Sonuç `hata` ise `doğru`, `tamam` ise `yanlış` döner. `basarili_mi`'nin tersidir.

Örnek:

```
basarisiz_mi(hata(1))   // doğru
basarisiz_mi(tamam(7))  // yanlış
```

Kenar durum: Gövde sonundaki `ver yanlış;` ulaşılamaz varsayılandır.

### ya_da

```
işlev ya_da<T, E>(r: sonuç<T, E>, varsayilan: T) -> T
```

Başarılı değeri döner; sonuç `hata` ise `varsayilan` değerini döner. Hata bilgisi yok sayılır.

Örnek:

```
ya_da(tamam(7), 0)   // 7
ya_da(hata(1), 0)    // 0 (varsayılan)
```

Kenar durum: `hata` durumunda hata değeri tamamen atılır; yalnızca `varsayilan` döner.

### harita

```
işlev harita<T, U, E>(r: sonuç<T, E>, f: işlev(T) -> U) -> sonuç<U, E>
```

Sonuç başarılıysa içindeki değere `f` fonksiyonunu uygular ve `tamam(f(v))` döner; `hata` ise hata aynen geçer. (Madde D generic callback inference ile çalışır.)

Örnek:

```
harita(tamam(21), |x: tam32| x * 2)   // tamam(42)
harita(hata(1),   |x: tam32| x * 2)   // hata(1) (f çağrılmaz)
```

Kenar durum: `hata` kolunda `f` hiç çağrılmaz, hata değeri korunur. `sonuç<U, E>` döndürdüğünden CODEGEN KISITI geçerli (`E` tamsayı olmalı). Tüm `eşleş` kolları `ver` ile bittiği için açık varsayılan dönüş gerekmez.

### harita_hata

```
işlev harita_hata<T, E, F>(r: sonuç<T, E>, f: işlev(E) -> F) -> sonuç<T, F>
```

Sonuç başarısızsa hata değerine `f` uygulanır ve `hata(f(e))` döner; `tamam` ise değer aynen geçer. Hata tipini dönüştürmenin (`E` → `F`) standart yoludur.

Örnek:

```
harita_hata(hata(404), |e: tam32| e + 1000)  // hata(1404)
harita_hata(tamam(7),  |e: tam32| e + 1000)  // tamam(7) (f çağrılmaz)
```

Kenar durum: `tamam` kolunda `f` çağrılmaz. CODEGEN KISITI: hem `E` hem `F` tamsayı değilse codegen bozulabilir.

### bagla

```
işlev bagla<T, U, E>(r: sonuç<T, E>, f: işlev(T) -> sonuç<U, E>) -> sonuç<U, E>
```

Başarılıysa `f`'i çağırır ve onun döndürdüğü yeni `sonuç`'u döner (zincirleme); `hata` ise hata aynen geçer. `flatMap` karşılığıdır. (Madde D ile çalışır.)

Örnek:

```
işlev böl(x: tam32) -> sonuç<tam32, tam32> {
    eğer x == 0 { ver hata(1); }
    ver tamam(100 / x);
}
bagla(tamam(4), böl)   // tamam(25)
bagla(tamam(0), böl)   // hata(1)  (f sıfır kontrolüyle hata döndürdü)
bagla(hata(9), böl)    // hata(9)  (f çağrılmaz, önceki hata geçer)
```

Kenar durum: `harita`'dan farkı `f`'in kendisinin bir `sonuç` döndürmesidir, böylece iç içe sarmalama (`sonuç<sonuç<...>>`) oluşmaz. CODEGEN KISITI geçerli.

### sonuc_to_opsiyonel

```
işlev sonuc_to_opsiyonel<T, E>(r: sonuç<T, E>) -> seçimlik<T>
```

Sonuç başarılıysa `değer(v)`, `hata` ise `hiç` döner. Hata bilgisi atılır — başarıyı varlık/yokluk olarak modeller.

Örnek:

```
sonuc_to_opsiyonel(tamam(7))  // değer(7)
sonuc_to_opsiyonel(hata(1))   // hiç
```

Kenar durum: `hata` durumunda hata değeri kaybolur; korunması gerekiyorsa `sonuc_hata_opsiyonel` kullanın. Gövde sonundaki `ver hiç;` ulaşılamaz varsayılandır.

### sonuc_hata_opsiyonel

```
işlev sonuc_hata_opsiyonel<T, E>(r: sonuç<T, E>) -> seçimlik<E>
```

Sonuç `hata` ise `değer(e)`, başarılıysa `hiç` döner. `sonuc_to_opsiyonel`'in hata tarafı muadilidir.

Örnek:

```
sonuc_hata_opsiyonel(hata(404))  // değer(404)
sonuc_hata_opsiyonel(tamam(7))   // hiç
```

Kenar durum: `tamam` durumunda başarı değeri atılır. Gövde sonundaki `ver hiç;` ulaşılamaz varsayılandır.

### opsiyonel_to_sonuc

```
işlev opsiyonel_to_sonuc<T, E>(opt: seçimlik<T>, hata_degeri: E) -> sonuç<T, E>
```

`seçimlik` `değer` içeriyorsa `tamam(v)`, `hiç` ise verilen `hata_degeri` ile `hata` döner. Bir `seçimlik`'e "neden boş" bilgisi eklemenin standart yoludur.

Örnek:

```
opsiyonel_to_sonuc(değer(7), 404)  // tamam(7)
opsiyonel_to_sonuc(hiç, 404)       // hata(404)
```

Kenar durum: `sonuç<T, E>` döndürdüğünden CODEGEN KISITI burada da geçerlidir — `E` tamsayı olmalı, metin hata kodu için şimdilik tamsayı kullanın. Gövde sonundaki `ver hata(hata_degeri);` ulaşılamaz varsayılandır.

---

## `KSonuc<T, E>` — Geçici yapı tabanlı API

`KSonuc<T, E>`, parser fix gelene kadar korunan yapı tabanlı sonuç temsilidir. İki opsiyonel alandan oluşur:

```
yapı KSonuc<T, E> {
    deger_d: seçimlik<T>;
    hata_d: seçimlik<E>;
}
```

Native API'den farkı, codegen kısıtından etkilenmeden `metin` gibi tamsayı olmayan hata tiplerini de struct alanı olarak taşıyabilmesidir. Yeni kod native `sonuç<T, E>`'yi tercih etmelidir.

### k_tamam

```
işlev k_tamam<T, E>(d: T) -> KSonuc<T, E>
```

Başarılı bir `KSonuc` oluşturur: `deger_d = değer(d)`, `hata_d = hiç`.

Örnek:

```
k_tamam(42)   // KSonuc { deger_d: değer(42), hata_d: hiç }
```

### k_hata

```
işlev k_hata<T, E>(h: E) -> KSonuc<T, E>
```

Başarısız bir `KSonuc` oluşturur: `deger_d = hiç`, `hata_d = değer(h)`.

Örnek:

```
k_hata("dosya yok")   // KSonuc { deger_d: hiç, hata_d: değer("dosya yok") }
```

### k_basarili_mi

```
işlev k_basarili_mi<T, E>(r: KSonuc<T, E>) -> mantıksal
```

`deger_d` alanı `değer` içeriyorsa `doğru`, `hiç` ise `yanlış` döner.

Örnek:

```
k_basarili_mi(k_tamam(7))         // doğru
k_basarili_mi(k_hata("x"))        // yanlış
```

Kenar durum: Yalnızca `deger_d` alanına bakar (hata alanına bakmaz); gövde sonundaki `ver yanlış;` ulaşılamaz varsayılandır.

### k_basarisiz_mi

```
işlev k_basarisiz_mi<T, E>(r: KSonuc<T, E>) -> mantıksal
```

`hata_d` alanı `değer` içeriyorsa `doğru`, `hiç` ise `yanlış` döner.

Örnek:

```
k_basarisiz_mi(k_hata("x"))   // doğru
k_basarisiz_mi(k_tamam(7))    // yanlış
```

Kenar durum: Yalnızca `hata_d` alanına bakar; gövde sonundaki `ver yanlış;` ulaşılamaz varsayılandır.

### k_ya_da

```
işlev k_ya_da<T, E>(r: KSonuc<T, E>, varsayilan: T) -> T
```

`deger_d` varsa içindeki değeri, yoksa `varsayilan`'ı döner. Native `ya_da`'nın `KSonuc` muadilidir.

Örnek:

```
k_ya_da(k_tamam(7), 0)        // 7
k_ya_da(k_hata("x"), 0)       // 0
```

Kenar durum: `hiç` durumunda hata değeri yok sayılır, yalnızca `varsayilan` döner.

### k_hata_ya_da

```
işlev k_hata_ya_da<T, E>(r: KSonuc<T, E>, varsayilan: E) -> E
```

`hata_d` varsa içindeki hatayı, başarılıysa `varsayilan`'ı döner.

Örnek:

```
k_hata_ya_da(k_hata("net"), "?")   // "net"
k_hata_ya_da(k_tamam(7), "?")      // "?"
```

Kenar durum: Başarılı durumda hiçbir hata olmadığından `varsayilan` döner.

### k_secimlik

```
işlev k_secimlik<T, E>(r: KSonuc<T, E>) -> seçimlik<T>
```

`KSonuc`'un başarı tarafını `seçimlik<T>` olarak verir (doğrudan `deger_d` alanını döner). `sonuc_to_opsiyonel`'in `KSonuc` muadilidir.

Örnek:

```
k_secimlik(k_tamam(7))    // değer(7)
k_secimlik(k_hata("x"))   // hiç
```

### k_hata_secimlik

```
işlev k_hata_secimlik<T, E>(r: KSonuc<T, E>) -> seçimlik<E>
```

`KSonuc`'un hata tarafını `seçimlik<E>` olarak verir (doğrudan `hata_d` alanını döner).

Örnek:

```
k_hata_secimlik(k_hata("x"))   // değer("x")
k_hata_secimlik(k_tamam(7))    // hiç
```

### k_harita_tam

```
işlev k_harita_tam(r: KSonuc<tam32, metin>,
                   f: işlev(tam32) -> tam32) -> KSonuc<tam32, metin>
```

Başarılıysa `deger_d` değerine `f` uygular (`k_tamam(f(v))`); başarısızsa hatayı aynen geçirir. Native `harita`'nın somut tip muadilidir (generic callback v2'de gelecek; bu yüzden tip `tam32`/`metin` olarak sabittir).

Örnek:

```
k_harita_tam(k_tamam(21), |x: tam32| x * 2)   // k_tamam(42)
k_harita_tam(k_hata("x"), |x: tam32| x * 2)   // k_hata("x")
```

Kenar durum: Hem `deger_d` hem `hata_d` `hiç` olan geçersiz/boş `KSonuc` durumunda fonksiyon `r`'yi olduğu gibi döner (iç içe `eşleş`'lerin `hiç` kolları). `hata_d` doluysa hatayı yeniden sarar.

### k_harita_hata_tam

```
işlev k_harita_hata_tam(r: KSonuc<tam32, metin>,
                        g: işlev(metin) -> metin) -> KSonuc<tam32, metin>
```

Başarısızsa `hata_d` hatasına `g` uygular (`k_hata(g(h))`); başarılıysa değeri aynen geçirir. Native `harita_hata`'nın somut tip muadilidir.

Örnek:

```
k_harita_hata_tam(k_hata("io"), |e: metin| e)    // k_hata(g("io"))
k_harita_hata_tam(k_tamam(7),   |e: metin| e)     // k_tamam(7)
```

Kenar durum: İkisi de `hiç` olan boş `KSonuc`'ta `r` olduğu gibi döner. `deger_d` doluysa değeri yeniden sarar (`g` çağrılmaz).

### k_bagla_tam

```
işlev k_bagla_tam(r: KSonuc<tam32, metin>,
                  f: işlev(tam32) -> KSonuc<tam32, metin>) -> KSonuc<tam32, metin>
```

Başarılıysa `f`'i çağırır ve döndürdüğü `KSonuc`'u verir (zincirleme); başarısızsa hatayı aynen geçirir. `flatMap` karşılığı; native `bagla`'nın somut tip muadilidir.

Örnek:

```
işlev böl(x: tam32) -> KSonuc<tam32, metin> {
    eğer x == 0 { ver k_hata("sıfıra bölme"); }
    ver k_tamam(100 / x);
}
k_bagla_tam(k_tamam(4), böl)        // k_tamam(25)
k_bagla_tam(k_hata("x"), böl)       // k_hata("x")  (f çağrılmaz)
```

Kenar durum: Boş `KSonuc` (`deger_d` ve `hata_d` ikisi de `hiç`) durumunda `r` olduğu gibi döner.

### k_alternatif

```
işlev k_alternatif<T, E>(a: KSonuc<T, E>, b: KSonuc<T, E>) -> KSonuc<T, E>
```

İlk argüman `a` başarılıysa onu döner; değilse ikinci argüman `b`'yi döner. "İlki olmazsa diğerini dene" deseni.

Örnek:

```
k_alternatif(k_tamam(1), k_tamam(2))   // k_tamam(1)
k_alternatif(k_hata("x"), k_tamam(2))  // k_tamam(2)
k_alternatif(k_hata("x"), k_hata("y")) // k_hata("y")
```

Kenar durum: `a` başarılı değilse (`a.deger_d` = `hiç`) — hata mı yoksa boş mu olduğuna bakılmaksızın — koşulsuz olarak `b` döner. Gövde sonundaki `ver b;` ulaşılamaz varsayılandır.

### k_dene_tam

```
işlev k_dene_tam(r: KSonuc<tam32, metin>,
                 devam: işlev(tam32) -> KSonuc<tam32, metin>) -> KSonuc<tam32, metin>
```

`r` başarılıysa `devam` fonksiyonunu çağırır, hatalıysa hatayı aynen yayar. "Hatayı yay" (`?` operatörü gibi) deseni. Uygulaması doğrudan `k_bagla_tam(r, devam)` çağrısıdır.

Örnek:

```
k_dene_tam(k_tamam(4), böl)     // böl(4) sonucu
k_dene_tam(k_hata("x"), böl)    // k_hata("x") (devam çağrılmaz)
```

Kenar durum: `k_bagla_tam` ile birebir aynı davranır; boş `KSonuc`'ta `r` döner.

### k_esit_mi

```
işlev k_esit_mi<T, E>(a: KSonuc<T, E>, b: KSonuc<T, E>) -> mantıksal
```

İki `KSonuc`'u yapısal olarak karşılaştırır: ikisi de aynı başarı değerini (`va == vb`) ya da aynı hata değerini (`ha == hb`) taşıyorsa `doğru`, aksi halde `yanlış` döner.

Örnek:

```
k_esit_mi(k_tamam(7), k_tamam(7))     // doğru
k_esit_mi(k_tamam(7), k_tamam(9))     // yanlış
k_esit_mi(k_hata("x"), k_hata("x"))   // doğru
k_esit_mi(k_tamam(7), k_hata("x"))    // yanlış
```

Kenar durumlar:
- Biri başarılı diğeri hatalıysa `yanlış` döner.
- Her ikisi de tamamen boş (`deger_d` ve `hata_d` ikisi de `hiç`) geçersiz durumdaysa — kaynaktaki yoruma göre "a tamamen boş — geçersiz durum; b de tamamen boş ise eşit" — iki boş `KSonuc` eşit (`doğru`) sayılır.
- Karşılaştırma `==` operatörüne dayanır; `T` ve `E` tiplerinin eşitlik karşılaştırmasını desteklemesi gerekir.
- Gövdedeki çok sayıda `ver yanlış;` satırı `eşleş` kollarını tamamlayan ulaşılamaz varsayılanlardır.
