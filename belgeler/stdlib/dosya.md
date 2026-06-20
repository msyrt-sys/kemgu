# stdlib/dosya — Dosya G/Ç (I/O)

Bu modül, KEMGU runtime'ının düşük seviye dosya primitifleri (`runtime/kdl_runtime.c` — Madde G) üzerine kurulu bir stdlib sarmalayıcısıdır. Runtime'ın `dosya_ac`, `dosya_oku`, `dosya_yaz`, `dosya_kapat`, `dosya_var_mi`, `dosya_sil`, `dosya_yeniden_adlandir`, `dosya_boyut` gibi C-stili (`void*` / `int` dönen) fonksiyonlarını sarar ve bunları KEMGU'nun `sonuç<T, hata>` dönüş stiline çevirir; böylece her hata açıkça yüzeye çıkar, runtime patlatma (exception) olmaz.

Tasarım felsefesi: Java `FileInputStream` + checked exception tuzağına düşmemek — her hata bir `sonuç<T, metin>` ile temsil edilir (hata gövdesi açıklayıcı bir `metin`).

**Genel v1 sınırlamaları:**
- Handle ABI henüz hazır olmadığından dosya handle'ları **metin** olarak temsil edilir; geçersiz handle boş metin (`""`) ile gösterilir (sentinel).
- Dosya modları string tabanlıdır (`"okuma"`, `"yazma"`, `"ekleme"`) — runtime'ın beklediği biçim.
- `oku_satirlar` henüz gerçek satır ayrıştırması yapmaz (codepoint-aware split runtime gerektirir); v1'de boş dizi döner.

---

## Sentinel / Handle yardımcıları

### gecersiz_handle (yardımcı)

```kemgu
işlev gecersiz_handle() -> metin
```

Geçersiz handle göstergesini (boş metin) döndürür. Handle ABI gelene kadar handle'lar metin tabanlı temsil edildiğinden, `""` "geçersiz handle" anlamına gelir.

**Örnek:**
```kemgu
değişken h: metin = gecersiz_handle();   // h == ""
```

**Kenar durum:** Her zaman boş metin döner; yan etkisi yoktur.

---

### handle_gecerli_mi (yardımcı)

```kemgu
işlev handle_gecerli_mi(h: metin) -> mantıksal
```

Verilen handle'ın geçerli olup olmadığını döndürür — boş olmayan metin geçerli kabul edilir.

**Örnek:**
```kemgu
handle_gecerli_mi("");        // yanlış
handle_gecerli_mi("dosya#3"); // doğru
```

**Kenar durum:** Boş metin (`""`) → `yanlış`; diğer her metin → `doğru`.

---

## Mod sabitleri

### mod_okuma (yardımcı)

```kemgu
işlev mod_okuma() -> metin
```

Okuma modu string sabitini (`"okuma"`) döndürür.

**Örnek:**
```kemgu
ac("veri.txt", mod_okuma());
```

---

### mod_yazma (yardımcı)

```kemgu
işlev mod_yazma() -> metin
```

Yazma modu string sabitini (`"yazma"`) döndürür. Yazma modu dosyayı üzerine yazar (truncate).

**Örnek:**
```kemgu
ac("cikti.txt", mod_yazma());
```

---

### mod_ekleme (yardımcı)

```kemgu
işlev mod_ekleme() -> metin
```

Ekleme (append) modu string sabitini (`"ekleme"`) döndürür.

**Örnek:**
```kemgu
ac("log.txt", mod_ekleme());
```

---

## Açma / Kapama

### ac

```kemgu
işlev ac(yol: metin, mod: metin) -> sonuç<metin, metin>
```

Verilen yol ve mod ile bir dosya açar; başarılıysa `tamam(handle)`, aksi halde `hata(...)` döner. İçeride runtime `dosya_ac(yol, mod)` çağrılır ve dönen handle geçerlilik açısından kontrol edilir.

**Örnek:**
```kemgu
eşleş ac("veri.txt", "okuma") {
    tamam(h) => { /* h ile oku */ }
    hata(e)  => { /* e: hata mesajı */ }
}
```

**Kenar durumlar:**
- Boş yol → `hata("bos yol")`.
- Boş mod → `hata("bos mod")`.
- Runtime geçersiz handle dönerse (açılamadı) → `hata("dosya acilamadi")`.

---

### kapat

```kemgu
işlev kapat(h: metin) -> sonuç<tam32, metin>
```

Verilen handle'ı kapatır. Başarılıysa `tamam(0)` döner.

**Örnek:**
```kemgu
kapat(h);   // tamam(0)
```

**Kenar durum:** Geçersiz handle (boş metin) → `hata("gecersiz handle")` (runtime `dosya_kapat` çağrılmaz).

---

## Okuma

### oku_metin

```kemgu
işlev oku_metin(yol: metin) -> sonuç<metin, metin>
```

Dosyanın tüm içeriğini tek bir `metin` olarak okur. Başarılıysa `tamam(icerik)` döner. İçeride runtime `dosya_oku(yol)` kullanılır.

**Örnek:**
```kemgu
eşleş oku_metin("ayar.txt") {
    tamam(icerik) => { /* icerik: tüm dosya metni */ }
    hata(e)       => { /* hata */ }
}
```

**Kenar durumlar:**
- Boş yol → `hata("bos yol")`.
- Dosya yoksa (`dosya_var_mi` yanlış) → `hata("dosya yok")`.

---

### oku_satirlar

```kemgu
işlev oku_satirlar(yol: metin) -> sonuç<Dizi<metin>, metin>
```

Amaç: dosyayı satır satır okuyup `Dizi<metin>` döndürmek.

**SINIR (v1):** Codepoint-aware satır ayırma runtime desteği henüz olmadığından bu fonksiyon gerçek ayrıştırma yapmaz; v1'de **boş dizi** (`tamam([])`) döner. Kullanıcı içeriği `oku_metin` ile alıp `metin_kes` gibi araçlarla elle ayrıştırabilir.

**Örnek:**
```kemgu
eşleş oku_satirlar("liste.txt") {
    tamam(satirlar) => { /* v1: satirlar boş dizi */ }
    hata(e)         => { /* boş yol durumu */ }
}
```

**Kenar durum:** Boş yol → `hata("bos yol")`. (Diğer durumlarda her zaman boş dizi döner — v1 davranışı.)

---

## Yazma

### yaz_metin

```kemgu
işlev yaz_metin(yol: metin, icerik: metin) -> sonuç<tam32, metin>
```

Dosyayı **üzerine yazar** (truncate + write): yolu `"yazma"` modunda açar, içeriği yazar, dosyayı kapatır. Başarılıysa `tamam(n)` döner; burada `n`, runtime `dosya_yaz`'ın bildirdiği yazılan miktardır.

**Örnek:**
```kemgu
yaz_metin("cikti.txt", "Merhaba");   // tamam(yazılan_byte_sayısı)
```

**Kenar durumlar:**
- Boş yol → `hata("bos yol")`.
- Dosya açılamazsa → `hata("dosya acilamadi")`.

---

### ekle

```kemgu
işlev ekle(yol: metin, icerik: metin) -> sonuç<tam32, metin>
```

Dosyaya içerik **ekler** (append): yolu `"ekleme"` modunda açar, içeriği yazar, kapatır. Başarılıysa `tamam(n)` döner (`n` = yazılan miktar).

**Örnek:**
```kemgu
ekle("log.txt", "yeni kayıt");   // tamam(n)
```

**Kenar durumlar:**
- Boş yol → `hata("bos yol")`.
- Dosya açılamazsa → `hata("dosya acilamadi")`.

---

### ekle_satir

```kemgu
işlev ekle_satir(yol: metin, satir: metin) -> sonuç<tam32, metin>
```

Dosyaya bir satır ekler: satırı `"ekleme"` modunda yazar, ardından bir satır sonu (`"\n"`) yazar. Başarılıysa `tamam(n + 1)` döner — `n`, satırın yazılan miktarı, `+1` ise eklenen newline içindir.

**Örnek:**
```kemgu
ekle_satir("log.txt", "olay olustu");   // tamam(n + 1)
```

**Kenar durumlar:**
- Boş yol → `hata("bos yol")`.
- Dosya açılamazsa → `hata("dosya acilamadi")`.

---

## Sorgu / Yönetim

### var_mi

```kemgu
işlev var_mi(yol: metin) -> mantıksal
```

Verilen yolda dosya olup olmadığını döndürür (runtime `dosya_var_mi`). `sonuç` değil, doğrudan `mantıksal` döner.

**Örnek:**
```kemgu
eğer var_mi("ayar.txt") { /* var */ }
```

**Kenar durum:** Boş yol → `yanlış` (runtime sorgusu yapılmadan).

---

### sil

```kemgu
işlev sil(yol: metin) -> sonuç<tam32, metin>
```

Dosyayı siler (runtime `dosya_sil`). Başarılıysa `tamam(0)` döner.

**Örnek:**
```kemgu
sil("gecici.txt");   // tamam(0) ya da hata("silinemedi")
```

**Kenar durumlar:**
- Boş yol → `hata("bos yol")`.
- Runtime sıfırdan farklı (`r != 0`) dönerse → `hata("silinemedi")`.

---

### boyut

```kemgu
işlev boyut(yol: metin) -> sonuç<tam64, metin>
```

Dosyanın byte cinsinden boyutunu döndürür (runtime `dosya_boyut`). Başarılıysa `tamam(s)` döner.

**Örnek:**
```kemgu
eşleş boyut("veri.bin") {
    tamam(s) => { /* s: byte sayısı (tam64) */ }
    hata(e)  => { /* hata */ }
}
```

**Kenar durumlar:**
- Boş yol → `hata("bos yol")`.
- Dosya yoksa → `hata("dosya yok")`.
- Runtime negatif boyut dönerse (`s < 0`) → `hata("boyut okunamadi")`.

---

### yeniden_adlandir

```kemgu
işlev yeniden_adlandir(eski: metin, yeni: metin) -> sonuç<tam32, metin>
```

Dosyayı yeniden adlandırır / taşır (runtime `dosya_yeniden_adlandir`). Başarılıysa `tamam(0)` döner.

**Örnek:**
```kemgu
yeniden_adlandir("eski.txt", "yeni.txt");   // tamam(0)
```

**Kenar durumlar:**
- Boş eski yol → `hata("bos eski yol")`.
- Boş yeni yol → `hata("bos yeni yol")`.
- Runtime sıfırdan farklı (`r != 0`) dönerse → `hata("yeniden adlandirilamadi")`.

---

### kopyala

```kemgu
işlev kopyala(kaynak: metin, hedef: metin) -> sonuç<tam32, metin>
```

Bir dosyayı kopyalar: kaynağı `dosya_oku` ile okur, hedefi `"yazma"` modunda açıp içeriği yazar ve kapatır. Başarılıysa `tamam(n)` döner (`n` = yazılan miktar). Kopyalama tamamen bellek üzerinden yapılır (içerik tek seferde okunup yazılır).

**Örnek:**
```kemgu
kopyala("kaynak.txt", "yedek.txt");   // tamam(n)
```

**Kenar durumlar:**
- Boş kaynak → `hata("bos kaynak")`.
- Boş hedef → `hata("bos hedef")`.
- Kaynak dosya yoksa → `hata("kaynak yok")`.
- Hedef açılamazsa → `hata("hedef acilamadi")`.
