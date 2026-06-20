# Stdlib: `metin` — Metin (String) Manipülasyon

KEMGU `metin` modülü, metin (string) işleme için snake_case Türkçe adlandırmalı bir API katmanıdır. Modül büyük ölçüde runtime primitifleri (`metin_uzunluk`, `metin_bayt`, `metin_kes`, `metin_birlestir`, `metin_esit`, `metin_kucuk`, `metin_buyuk`, `metin_icerir`, `metin_baslar`, `metin_biter`, `metin_kirp`, `metin_yer_degistir`) üzerine kurulu ince sarmalayıcılar (`kucukharf`, `buyukharf`, `baslar_ile` vb.) ile, bu primitifleri kullanan saf KEMGU genişletmelerinden (gerçek bölme/birleştirme/arama/sınıflama) oluşur. `metin` ayraç alan sürümler, `karakter` tipinin tamsayı ile karşılaştırılamamasından (T001) doğan kısıtlamayı aşar.

**Genel v1 sınırlamaları:**
- Çoğu işlem **byte tabanlıdır** (UTF-8 codepoint değil). `uzunluk`, `kes`, `indeks`, `say`, `bul_tum` gibi fonksiyonlar byte indeksi/sayısı döndürür. Yalnız `karakter_sayisi`, `yansit`, `doldur_sol`, `doldur_sag`, `basharf_buyut` codepoint-farkındadır.
- `kes` byte indekslidir; çok baytlı UTF-8 dizisini ortadan kırabilir (codepoint tabanlı `kes_kp` v2'de planlı).
- `bol` (karakter ayraçlı) bir STUB'dır — gerçek bölme için `bol_metin` kullanın.
- `birlestir` heap tahsis eder; v1'de sızıntı (leak) kabul edilir.
- Büyük/küçük harf dönüşümü Türkçe `I/i` çiftini destekler (I→ı, İ→i, ı→I, i→İ).

---

## Gerçekten çalışan ops (runtime gerekmiyor)

### esit_mi
```kemgu
işlev esit_mi(a: metin, b: metin) -> mantıksal
```
İki metnin eşit olup olmadığını döndürür (`a == b`). Metin için `==` literal/byte eşitliğidir (gerçek karşılaştırma derleyici dahili).

**Örnek:**
```kemgu
esit_mi("kem", "kem")   // doğru
esit_mi("kem", "gu")    // yanlış
```

### farkli_mi
```kemgu
işlev farkli_mi(a: metin, b: metin) -> mantıksal
```
İki metnin farklı olup olmadığını döndürür (`a != b`).

**Örnek:**
```kemgu
farkli_mi("a", "b")   // doğru
```

### bos_mu
```kemgu
işlev bos_mu(s: metin) -> mantıksal
```
Metnin boş olup olmadığını döndürür (`s == ""`, uzunluk 0).

**Örnek:**
```kemgu
bos_mu("")    // doğru
bos_mu("x")   // yanlış
```

### dolu_mu
```kemgu
işlev dolu_mu(s: metin) -> mantıksal
```
Metnin dolu (boş olmayan) olup olmadığını döndürür (`s != ""`).

**Örnek:**
```kemgu
dolu_mu("x")   // doğru
dolu_mu("")    // yanlış
```

### sarmala (yardımcı)
```kemgu
işlev sarmala(s: metin) -> seçimlik<metin>
```
Metni opsiyonel (`seçimlik<metin>`) içine sarmalar.

**Kenar durum:** Boş metin için `hiç` döner; dolu metin için `değer(s)` döner.

**Örnek:**
```kemgu
sarmala("x")   // değer("x")
sarmala("")    // hiç
```

---

## Runtime primitif tabanlı ops

### uzunluk
```kemgu
işlev uzunluk(s: metin) -> tam32
```
Metnin **byte sayısını** döndürür (UTF-8 codepoint sayısı değil). `metin_uzunluk` sarmalayıcısı.

**Kenar durum:** Çok baytlı karakterlerde byte sayısı codepoint sayısından büyüktür (örn. `"çığ"` → 6).

**Örnek:**
```kemgu
uzunluk("abc")   // 3
uzunluk("çığ")   // 6  (karakter sayısı için karakter_sayisi kullan)
```

### birlestir
```kemgu
işlev birlestir(a: metin, b: metin) -> metin
```
İki metni yan yana koyar. `metin_birlestir` sarmalayıcısı.

**Kenar durum:** Heap tahsisi yapar; v1'de sızıntı kabul edilir.

**Örnek:**
```kemgu
birlestir("kem", "gu")   // "kemgu"
```

### birlestir_uc
```kemgu
işlev birlestir_uc(a: metin, b: metin, c: metin) -> metin
```
Üç metni sırayla birleştirir (`birlestir(birlestir(a, b), c)`).

**Örnek:**
```kemgu
birlestir_uc("a", "b", "c")   // "abc"
```

### kes
```kemgu
işlev kes(s: metin, baslangic: tam32, uzunluk: tam32) -> metin
```
Alt metin (substring) döndürür: `[baslangic, baslangic+uzunluk)` byte aralığı. `metin_kes` sarmalayıcısı.

**Kenar durum:** Byte indekslidir; çok baytlı UTF-8 dizisini ortadan kırabilir — kullanıcı dikkatli olmalıdır.

**Örnek:**
```kemgu
kes("merhaba", 0, 3)   // "mer"
```

### bol (STUB)
```kemgu
işlev bol(s: metin, ayirac: karakter) -> Dizi<metin>
```
Metni bir karakter ayraca göre bölmeyi amaçlar — ancak v1'de işlevsel değildir.

**SINIRLAMA / STUB:** `karakter` tipi tamsayı ile karşılaştırılamaz (T001) ve metinden karakter çıkaran primitif yoktur; bu yüzden bu imzayla gerçek bölme yapılamaz. v1: boş kaynak için boş dizi `[]`, aksi halde tek elemanlı dizi `[s]` (kaynağın kendisi) döner. **Gerçek bölme için `bol_metin` kullanın.**

### kucukharf
```kemgu
işlev kucukharf(s: metin) -> metin
```
Metni küçük harfe dönüştürür. Türkçe-farkındadır (I→ı, İ→i). `metin_kucuk` sarmalayıcısı.

**Örnek:**
```kemgu
kucukharf("İSTANBUL")   // "istanbul"
```

### buyukharf
```kemgu
işlev buyukharf(s: metin) -> metin
```
Metni büyük harfe dönüştürür. Türkçe-farkındadır (i→İ, ı→I). `metin_buyuk` sarmalayıcısı.

**Örnek:**
```kemgu
buyukharf("ışık")   // "IŞIK"
```

### icerir
```kemgu
işlev icerir(s: metin, alt: metin) -> mantıksal
```
`s` metninin `alt` alt metnini içerip içermediğini döndürür (byte tabanlı, UTF-8 prefix-safe). `metin_icerir` sarmalayıcısı.

**Örnek:**
```kemgu
icerir("kemgu", "mg")   // doğru
```

### baslar_ile
```kemgu
işlev baslar_ile(s: metin, prefix: metin) -> mantıksal
```
`s` metninin `prefix` ön eki ile başlayıp başlamadığını döndürür. `metin_baslar` sarmalayıcısı.

**Örnek:**
```kemgu
baslar_ile("kemgu", "kem")   // doğru
```

### biter_ile
```kemgu
işlev biter_ile(s: metin, sufiks: metin) -> mantıksal
```
`s` metninin `sufiks` son eki ile bitip bitmediğini döndürür. `metin_biter` sarmalayıcısı.

**Örnek:**
```kemgu
biter_ile("dosya.kem", ".kem")   // doğru
```

### kirp
```kemgu
işlev kirp(s: metin) -> metin
```
Metnin başından ve sonundan ASCII whitespace siler (trim). `metin_kirp` sarmalayıcısı.

**Örnek:**
```kemgu
kirp("  selam  ")   // "selam"
```

### yer_degistir
```kemgu
işlev yer_degistir(s: metin, hedef: metin, yeni: metin) -> metin
```
`hedef` alt metninin **TÜM** eşleşmelerini `yeni` ile değiştirir. `metin_yer_degistir` sarmalayıcısı. (Yalnız ilkini değiştirmek için `degistir_ilk` kullanın.)

**Örnek:**
```kemgu
yer_degistir("a-b-c", "-", "_")   // "a_b_c"
```

### tekrarla
```kemgu
işlev tekrarla(s: metin, n: tam32) -> metin
```
Metni `n` kez yan yana ekler.

**Kenar durum:** `n <= 0` ise boş metin `""` döner.

**Örnek:**
```kemgu
tekrarla("ab", 3)   // "ababab"
tekrarla("x", 0)    // ""
```

### cp_bayt_sayisi (yardımcı)
```kemgu
işlev cp_bayt_sayisi(lead: tam32) -> tam32
```
Bir UTF-8 codepoint'in kaç bayttan oluştuğunu, öncü (lead) byte değerinden (0..255 işaretsiz) çıkarır:
- `0..127` (0xxxxxxx) → 1 (ASCII)
- `192..223` (110xxxxx) → 2
- `224..239` (1110xxxx) → 3
- `240..255` (11110xxx) → 4

**Kenar durum:** Devam baytları (128..191) için güvenli olarak 1 döner.

**Örnek:**
```kemgu
cp_bayt_sayisi(195)   // 2  (0xC3 — örn. ç/ö/ü öncü baytı)
cp_bayt_sayisi(65)    // 1  ('A')
```

### karakter_sayisi
```kemgu
işlev karakter_sayisi(s: metin) -> tam32
```
Metnin **UTF-8 codepoint sayısını** (byte değil) döndürür. Devam baytları (`10xxxxxx`, 128..191) sayılmaz; her öncü byte bir karakterdir. Türkçe metinde doğru sonuç verir.

**Kenar durum:** Boş metin → 0. (Dahili olarak `metin_bayt` ile işaretli tam8 → işaretsiz 0..255 dönüşümü yapılır.)

**Örnek:**
```kemgu
karakter_sayisi("çığ")   // 3  (uzunluk ise 6 döner)
```

### yansit
```kemgu
işlev yansit(s: metin) -> metin
```
Metni codepoint-farkında (UTF-8 güvenli) tersine çevirir. Her codepoint bir bütün olarak öne eklenir; çok baytlı Türkçe karakterler bozulmaz.

**Kenar durum:** Boş metin → `""`. Byte tabanlı ters çevirmenin aksine multi-byte dizileri korur.

**Örnek:**
```kemgu
yansit("açb")   // "bça"
```

---

## Genişletme — gerçek bölme / birleştirme / arama

Bu bölümdeki fonksiyonlar byte-indeksli runtime primitifleri (`metin_uzunluk`, `metin_bayt`, `metin_kes`, `metin_esit`, `metin_birlestir`) ile dizi primitifleri (`dizi_olustur`, `dizi_ekle`) üzerine kuruludur; yeni runtime gerektirmez.

### bol_metin
```kemgu
işlev bol_metin(s: metin, ayrac: metin) -> Dizi<metin>
```
`s` metnini `ayrac` alt metnine göre böler ve `Dizi<metin>` döndürür. `bol`'un (karakter ayraçlı) gerçek işlevsel eşidir.

**Kenar durum:**
- Boş kaynak (`s == ""`) → boş dizi.
- Boş ayraç (`ayrac == ""`) → tek parça (kaynağın kendisi).
- Ardışık ayraçlar boş parça üretir (`"a,,b"` → `["a", "", "b"]`).

**Örnek:**
```kemgu
bol_metin("a,b,c", ",")   // ["a", "b", "c"]
bol_metin("a,,b", ",")    // ["a", "", "b"]
```

### birlestir_liste
```kemgu
işlev birlestir_liste(parcalar: Dizi<metin>, ayrac: metin) -> metin
```
Dizideki parçaları `ayrac` ile birleştirir (join). `bol_metin`'in tersidir.

**Kenar durum:** Boş dizi → `""`. Tek eleman → o eleman (ayraç eklenmez).

**Örnek:**
```kemgu
birlestir_liste(["a", "b", "c"], "-")   // "a-b-c"
```

### indeks
```kemgu
işlev indeks(s: metin, alt: metin) -> seçimlik<tam32>
```
`alt` alt metninin `s` içindeki ilk **byte** konumunu (codepoint değil) döndürür.

**Kenar durum:**
- Boş `alt` → `değer(0)`.
- `alt`, `s`'ten uzunsa (`M > L`) → `hiç`.
- Bulunamazsa → `hiç`.

**Örnek:**
```kemgu
indeks("kemgu", "mg")   // değer(2)
indeks("kemgu", "z")    // hiç
```

### say
```kemgu
işlev say(s: metin, alt: metin) -> tam32
```
`alt` alt metninin **örtüşmeyen** geçiş sayısını döndürür (eşleşince M kadar ilerler).

**Kenar durum:** Boş `alt` → 0. `alt` daha uzunsa (`M > L`) → 0.

**Örnek:**
```kemgu
say("aaaa", "aa")   // 2  (örtüşmeyen)
say("abc", "x")     // 0
```

### on_ek_kaldir
```kemgu
işlev on_ek_kaldir(s: metin, on: metin) -> metin
```
`s` metni `on` ön eki ile başlıyorsa o ön eki siler; başlamıyorsa metni aynen döndürür.

**Örnek:**
```kemgu
on_ek_kaldir("kemgu", "kem")   // "gu"
on_ek_kaldir("kemgu", "xyz")   // "kemgu"
```

### son_ek_kaldir
```kemgu
işlev son_ek_kaldir(s: metin, son: metin) -> metin
```
`s` metni `son` son eki ile bitiyorsa o son eki siler; bitmiyorsa metni aynen döndürür.

**Örnek:**
```kemgu
son_ek_kaldir("dosya.kem", ".kem")   // "dosya"
```

### doldur_sol
```kemgu
işlev doldur_sol(s: metin, hedef_uz: tam32, dolgu: metin) -> metin
```
`s`'yi soluna `dolgu` ekleyerek hedef **karakter** (codepoint) uzunluğuna getirir (left pad). Sayı hizalama için tipik kullanım.

**Kenar durum:** Mevcut karakter uzunluğu `hedef_uz`'dan büyük veya eşitse metni aynen döndürür. `dolgu` tek karakter varsayılır.

**Örnek:**
```kemgu
doldur_sol("7", 3, "0")   // "007"
doldur_sol("1234", 3, "0")   // "1234"  (zaten daha uzun)
```

### doldur_sag
```kemgu
işlev doldur_sag(s: metin, hedef_uz: tam32, dolgu: metin) -> metin
```
`s`'nin sağına `dolgu` ekleyerek hedef karakter uzunluğuna getirir (right pad).

**Kenar durum:** Mevcut karakter uzunluğu `hedef_uz`'dan büyük veya eşitse metni aynen döndürür.

**Örnek:**
```kemgu
doldur_sag("7", 3, "0")   // "700"
```

### basharf_buyut
```kemgu
işlev basharf_buyut(s: metin) -> metin
```
Metnin ilk codepoint'ini büyük harfe çevirir, kalanını aynen bırakır. `buyukharf` Türkçe-farkındadır (i→İ).

**Kenar durum:** Boş metin → `""`.

**Örnek:**
```kemgu
basharf_buyut("istanbul")   // "İstanbul"
```

---

## Genişletme 2 — tüm-geçiş indeksleri / tek değiştirme / arası-kes / sınıflama

Bu bölüm de yalnız mevcut byte-indeksli runtime primitifleri + modül-içi `indeks`/`cp_bayt_sayisi` üzerine kuruludur. (Alt-metin sayımı için ayrı bir `say_alt` yoktur; `say` zaten örtüşmeyen geçiş sayısını verir.)

### bul_tum
```kemgu
işlev bul_tum(s: metin, alt: metin) -> Dizi<tam32>
```
`alt` alt metninin **TÜM** (örtüşmeyen) başlangıç byte indekslerini döndürür. `say` ile tutarlıdır (eşleşince M kadar atlar).

**Kenar durum:** Boş `alt` → boş dizi. `alt` daha uzunsa (`M > L`) → boş dizi.

**Örnek:**
```kemgu
bul_tum("aXaXa", "a")   // [0, 2, 4]
```

### degistir_ilk
```kemgu
işlev degistir_ilk(s: metin, hedef: metin, yeni: metin) -> metin
```
`hedef`in **yalnız ilk** geçişini `yeni` ile değiştirir. (`yer_degistir` tümünü değiştirir.)

**Kenar durum:** Boş `hedef` → metni aynen döndürür. Bulunamazsa → metni aynen döndürür.

**Örnek:**
```kemgu
degistir_ilk("a-b-c", "-", "_")   // "a_b-c"
```

### kes_arasi
```kemgu
işlev kes_arasi(s: metin, bas: metin, son: metin) -> seçimlik<metin>
```
`bas` ve `son` ayraçları arasındaki metni döndürür (`seçimlik<metin>`). `son`, `bas`ın bittiği konumdan sonra aranır (örtüşme yok); ayraçların kendisi sonuca dahil değildir.

**Kenar durum:** `bas` bulunamazsa veya `bas`tan sonra `son` bulunamazsa → `hiç`.

**Örnek:**
```kemgu
kes_arasi("<a>içerik</a>", ">", "<")   // değer("içerik")
kes_arasi("yok", "[", "]")             // hiç
```

### ascii_harf_mi (yardımcı)
```kemgu
işlev ascii_harf_mi(u: tam32) -> mantıksal
```
Verilen byte değerinin ASCII harf (A-Z: 65..90 veya a-z: 97..122) olup olmadığını döndürür.

**Örnek:**
```kemgu
ascii_harf_mi(65)   // doğru  ('A')
ascii_harf_mi(48)   // yanlış ('0')
```

### tr_harf_2byte (yardımcı)
```kemgu
işlev tr_harf_2byte(b0: tam32, b1: tam32) -> mantıksal
```
İki baytlık UTF-8 dizisinin (öncü `b0`, devam `b1`) Türkçe'ye özgü bir harf olup olmadığını döndürür:
- lead `195` (0xC3): ç Ç ö Ö ü Ü
- lead `196` (0xC4): ğ Ğ ı İ
- lead `197` (0xC5): ş Ş

**Kenar durum:** Türkçe alfabe dışındaki Latin-1 işaretlileri (é, à, ñ ...) kapsamaz → `yanlış`.

**Örnek:**
```kemgu
tr_harf_2byte(196, 159)   // doğru  (ğ)
tr_harf_2byte(195, 169)   // yanlış (é)
```

### sadece_rakam_mi
```kemgu
işlev sadece_rakam_mi(s: metin) -> mantıksal
```
Boş olmayan metnin **TÜM** baytlarının ASCII rakam (`'0'`..`'9'`, yani 48..57) olup olmadığını döndürür.

**Kenar durum:** Boş metin → `yanlış`.

**Örnek:**
```kemgu
sadece_rakam_mi("12345")   // doğru
sadece_rakam_mi("12a")     // yanlış
sadece_rakam_mi("")        // yanlış
```

### sadece_harf_mi
```kemgu
işlev sadece_harf_mi(s: metin) -> mantıksal
```
Boş olmayan metnin **TÜM** karakterlerinin harf olup olmadığını döndürür: ASCII A-Z/a-z + Türkçe (ç ğ ı i ö ş ü ve büyükleri). UTF-8 codepoint-farkındadır.

**Kenar durum:**
- Boş metin → `yanlış`.
- Rakam, boşluk, noktalama → `yanlış`.
- 3+ baytlık codepoint'ler (ve eksik 2-byte dizisi) → `yanlış`.

**Örnek:**
```kemgu
sadece_harf_mi("KemguÇığ")   // doğru
sadece_harf_mi("kem gu")     // yanlış (boşluk)
sadece_harf_mi("kem1")       // yanlış (rakam)
```
