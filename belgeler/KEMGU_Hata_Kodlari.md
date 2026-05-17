# KEMGU Hata Kodları Tablosu

Bu belge KEMGU derleyicisinin ürettiği tüm hata kodlarını listeler. Her
kodun anlamı, tipik nedeni ve programcının atması gereken adım belirtilir.

Hata kodları aşağıdaki kategorilerde toplanır:

| Önek | Faz | Anlam |
|------|-----|-------|
| `L###` | **Lexer** | Tokenizasyon hatası (UTF-8, literal, vs.) |
| `P###` | **Parser** | Sözdizimi hatası |
| `T###` | **Tip kontrolü** | Tip uyumsuzluğu, tanımsız sembol |
| `R-*`  | **Bölge atama** | Bölge çıkarsama aksiyom adları (hata değil; bilgi) |

Derleyici çıktısı şu formatı kullanır:

```
hata[T001]: ikili operator iki tarafi ayni tip olmali
   --> dosya.kem:5:23
   |
 5 |     ver x + "hello";
   |                       ^
```

---

## L — Lexer Hataları (6 kod)

| Kod | Mesaj | Neden | Düzeltme |
|-----|-------|-------|----------|
| `L001` | kapatilmamis metin literali | `"...` ardından `"` yok | Metni `"` ile kapat |
| `L002` | kapatilmamis raw metin literali | `r#"..."#` formatında `"#` yok | Raw metni `"#` ile kapat |
| `L005` | gecersiz sayi formati | `0x`, `0b`, `0o` sonrası geçersiz karakter | Doğru tabanı kullan (0xFF, 0b101, 0o777) |
| `L009` | bos karakter literali | `''` boş | Karakter koy: `'a'` |
| `L010` | karakter literalinde birden fazla karakter | `'ab'` | Tek karakter veya metin (`"ab"`) kullan |
| `L011` | gecersiz raw metin baslangici | `r"..."` (eksik `#`) | `r#"..."#` formatını kullan |

---

## P — Parser Hataları

### P0xx — Üst düzey + temel tanımlar

| Kod | Mesaj | Neden | Düzeltme |
|-----|-------|-------|----------|
| `P001` | üst düzey tanım bekleniyor | İlk token islev/yapı/sabit/modül/kullan/dışa değil | Geçerli bir tanım anahtar kelimesiyle başla |
| `P010` | ifade bekleniyor | İfade beklenen yerde bilinmeyen token | Geçerli bir ifade yaz (literal, tanımlayıcı, parantez...) |
| `P011` | tip bekleniyor | Tip beklenen yerde başka token | Tip yaz (tam32, *T, Dizi<T> vs.) |
| `P012` | parametre adi bekleniyor | `işlev(`'den sonra geçerli tanımlayıcı yok | Parametre adı yaz |
| `P013` | parametre tipi için `:` bekleniyor | `parametre tipi` ayraç eksik | `ad: tip` formatını kullan |
| `P014` | islev adi bekleniyor | `işlev` sonrasında tanımlayıcı yok | İşlev için isim ver |
| `P015` | `(` bekleniyor | İşlev tanımında parametre listesi başı yok | Parametre listesi için `()` ekle |
| `P016` | `)` bekleniyor | Parametre listesi kapatılmadı | Eksik `)` ekle |
| `P017` | islev govdesi icin `{` bekleniyor | İşlev gövdesi yok | `{ ... }` ekle veya `;` ile imza yap |
| `P018` | alan adi bekleniyor | `yapı`'nın `{` içinde tanımlayıcı yok | Alan adı yaz (`ad: tip;`) |
| `P019` | alan tipi için `:` bekleniyor | Alan adı sonrası `:` yok | `: tip` ekle |
| `P020` | alan sonunda `;` bekleniyor | Yapı alanı `;` ile bitmeli | `;` ekle |
| `P021` | yapi adi bekleniyor | `yapı` sonrasında tanımlayıcı yok | Yapı adı yaz |
| `P022` | `{` bekleniyor (yapı gövdesi) | Yapı tanımında gövde başı yok | `{ alan: tip; ... }` ekle |
| `P023` | `}` bekleniyor (yapı gövdesi) | Yapı gövdesi kapatılmadı | Eksik `}` ekle |
| `P024` | generic tip parametresi bekleniyor | `<` sonrası tanımlayıcı yok | `T1, T2, ...` yaz |
| `P025` | `>` bekleniyor (generic) | Generic param listesi kapatılmadı | `>` ekle |

### P03x — Sabit

| Kod | Mesaj |
|-----|-------|
| `P030` | sabit adı bekleniyor |
| `P031` | `:` bekleniyor (sabit tipi) |
| `P032` | `=` bekleniyor (sabit değeri) |
| `P033` | `;` bekleniyor |

### P04x — Kullan / Dışa

| Kod | Mesaj |
|-----|-------|
| `P040` | yol bekleniyor (`kullan` sonrası) |
| `P041` | `::` veya `;` bekleniyor |
| `P042` | `;` bekleniyor (kullan sonu) |
| `P050` | `disa` sonrası tanım bekleniyor (islev/yapi/sabit) |

### P06x — Modül

| Kod | Mesaj |
|-----|-------|
| `P060` | modül adı bekleniyor |
| `P061` | `{` bekleniyor (modül gövdesi) |
| `P062` | `}` bekleniyor (modül gövdesi) |

### P07x — Blok

| Kod | Mesaj |
|-----|-------|
| `P070` | `{` bekleniyor (blok) |
| `P071` | `}` bekleniyor (blok) |

### P08x — Değişken / atama / ver

| Kod | Mesaj |
|-----|-------|
| `P080` | değişken adı bekleniyor |
| `P081` | `=` bekleniyor |
| `P082` | `;` bekleniyor |
| `P090` | `;` bekleniyor (ver) |
| `P100` | `;` bekleniyor (atama) |
| `P101` | `;` bekleniyor (ifade deyimi) |

### P11x-P15x — İfade içi

| Kod | Mesaj | Bağlam |
|-----|-------|--------|
| `P110` | `)` bekleniyor | Parantezli ifade |
| `P120` | alan adı bekleniyor | `x.???` |
| `P121` | `]` bekleniyor | `x[i]` |
| `P122` | çağrı argümanları için `)` bekleniyor | `f(...)` |
| `P123` | yol devamı bekleniyor | `::` sonrası |
| `P130` | alan adı bekleniyor (yapı oluşturma) | `Yapı { ??? }` |
| `P131` | alan için `:` bekleniyor | `{ ad: ??? }` |
| `P132` | yapı oluşturmasında `}` bekleniyor | |
| `P140` | lambda kapanış `|` bekleniyor | `|param| ifade` |
| `P150` | dizide `]` bekleniyor | `[e1, e2, ...]` |

### P16x — boyut

| Kod | Mesaj |
|-----|-------|
| `P160` | `boyut<...>` için `<` bekleniyor |
| `P161` | `boyut<...>` için `>` bekleniyor |

### P20x — Kontrol akışı

| Kod | Mesaj | Bağlam |
|-----|-------|--------|
| `P200` | öznitelik adı bekleniyor | `[...]` |
| `P201` | bolum için `:` bekleniyor | `[bolum: "..."]` |
| `P202` | bolum adı metin literali bekleniyor | |
| `P203` | bilinmeyen öznitelik (ciplak/kesme/bolum) | |
| `P204` | öznitelik listesinde `]` bekleniyor | |
| `P211` | desen bekleniyor | `eşleş`'in kolu |
| `P231` | `eşleş` sonunda `}` bekleniyor | |
| `P241` | `:` bekleniyor | `için x: koleksiyon` |

### P30x-P32x — Karmaşık tipler

| Kod | Mesaj |
|-----|-------|
| `P310` | `secimlik<...>` için `<` bekleniyor |
| `P311` | `secimlik<...>` için `>` bekleniyor |
| `P312` | `sonuc<...>` için `<` bekleniyor |
| `P313` | `,` bekleniyor (`sonuc<T,H>`) |
| `P314` | `sonuc<...>` için `>` bekleniyor |
| `P315` | `islev` tipinde `(` bekleniyor |
| `P316` | `islev` tipinde `)` bekleniyor |
| `P317` | `islev` tipinde `->` bekleniyor |
| `P318` | `<` bekleniyor (generic) |
| `P319` | `>` bekleniyor (generic) |

---

## T — Tip Kontrolü Hataları (26 kod)

| Kod | Mesaj | Neden | Düzeltme |
|-----|-------|-------|----------|
| `T001` | tip uyumsuzluğu | İki taraf farklı tip | Tipleri eşitle veya açık dönüşüm yap |
| `T002` | tanımsız sembol | Kullanılan ad tanımlı değil | `değişken` ile tanımla veya doğru ismi kullan |
| `T003` | sayısal tip bekleniyor | Aritmetik op sayısal olmayan tipte | Sayısal tip kullan (tam32, kesirli64, vs.) |
| `T004` | mantıksal tip bekleniyor | `ve`/`veya`/`değil` mantıksal olmayan tipte | `mantıksal` tipte ifade yaz |
| `T005` | indeks tamsayı olmalı | `dizi[indeks]` `indeks` tamsayı değil | `tam32`/`tam64` ifade kullan |
| `T006` | çağrı için işlev tipi gerek | İşlev olmayan değer çağrıldı | Doğru işlev adı kullan |
| `T007` | alan erişimi yapı tipi gerek | `.` operatörü yapı olmayan değerde | Yapı tipine sahip değer kullan |
| `T008` | indeksleme dizi tipi gerek | `[]` dizi olmayan değerde | `Dizi<T>` tipinde değer kullan |
| `T009` | alan bulunamadı | `x.alan` mevcut değil | Yapı tanımındaki alan adlarını kontrol et |
| `T010` | çağrı argüman sayısı uyumsuz | Parametre/argüman sayısı farklı | Parametre tanımına uygun çağrı yap |
| `T011` | bilinmeyen tip / kullanıcı tipi | Tip adı tanımlı değil | Tipi tanımla veya doğru ismi kullan |
| `T012` | yapı oluşturmada eksik alan | Bazı alanlar atanmamış | Tüm alanları doldur |
| `T013` | dizi elemanları farklı tipte | Heterojen dizi | Tek tip kullan |
| `T014` | boş dizi tipi çıkarsanamaz | `[]` context yok | Tip annot ekle: `: Dizi<tam32>` |
| `T015` | lambda parametre tip annotasyonu gerek | `|x|` tipsiz | `|x: tip|` formatını kullan |
| `T016` | yol çözümleme hatası | `x::y` çözülemedi | Modülü içe aktar |
| `T017` | yapıda bilinmeyen alan | Yapı oluşturmada fazla alan | Yapı tanımına bak |
| `T020` | ver tipi işlev dönüş tipi ile uyumsuz | `ver x` x tipi `-> T` ile farklı | Dönüş tipini veya ifadeyi düzelt |
| `T021` | koşul mantıksal olmalı | `eğer`/`iken` koşulu mantıksal değil | Karşılaştırma kullan veya boolean tipinde değer |
| `T022` | atama hedefi lvalue olmalı | `5 = x` gibi geçersiz atama | Hedef değişken/erişim/indeks olmalı |
| `T023` | ver işlev gövdesi dışında | Top-level `ver` | `ver` sadece işlev içinde |
| `T024` | çift tanım | Aynı isim iki kez | Farklı isim kullan veya scope ayır |
| `T026` | yapı tanımı çakışması | Aynı yapı adı iki kez | Yapı adını değiştir |
| `T027` | `için` koleksiyonu `Dizi<T>` olmalı | `için x: bilmediği` | `Dizi<T>` tipinde koleksiyon kullan |
| `T028` | pointer çıkarma aynı hedef tip ister | `*tam32 - *tam8` | Aynı hedef tipte pointerlar |
| `T030` | bit op tamsayı olmalı | `mantıksal & ...` | Tamsayı operandlar kullan |

---

## R — Bölge Atama Aksiyomları (hata değil; bilgi)

Bölge çözümleyici aşağıdaki aksiyomları uygular. Bu kodlar hata raporunda
görünmez; analiz katmanının iç ad alanıdır.

| Aksiyom | Kural |
|---------|-------|
| `R-LIT` | Basit literal (TAM, KESIRLI, MANTIKSAL, KARAKTER, BOŞ) → `ρ_lit` (stack, bölge yok) |
| `R-YEREL` | Bileşik literal (METIN, DİZİ, YAPI, LAMBDA) escape etmiyorsa → `ρ_yerel(f)` |
| `R-VER` | `ver e` içinde değer → `ρ_cagiran(f)` (escape) |
| `R-İTERASYON` | `iken`/`için` gövdesi içinde değer → `ρ_iterasyon(d)` |
| `R-KOŞUL` | `eğer/değilse` iki dal → LCA (daha uzun ömürlü) |
| `R-GÖREV` | `_gorev_baslat()` → yeni `ρ_sahip(t)` (Katman 2) |
| `R-BİRLEŞTİR` | `_gorev_birlestir(h)` → çağıranın bölgesi |
| `R-KANAL` | `_kanal_olustur()` → yeni `ρ_kanal(k)`; `_kanal_gonder(k, v)` → değer ρ_kanal'a transfer |

---

## Eksik / Gelecek

- `T018`, `T019`, `T025`, `T029` — atlanmış kod numaraları (gelecek için rezerve)
- Lexer: detaylı UTF-8 hata kodları (her hatalı sekansa ayrı kod)
- Bölge: hata kodları henüz yok (analyzer raporlama yapmıyor; sadece atama)
- Tip sistemi B grubu: `linear`, `sabitsüre`, `capability` kodları spec sonrası eklenecek

---

## Hata Mesajı Kalite Standardı (direktif Bölüm 5.3)

KEMGU hata mesajı şunları taşımalı:

1. **Kod** — `T001`, `P018` vs. (referans için kararlı)
2. **Mesaj** — "ne yanlış" (kısa, Türkçe)
3. **Konum** — dosya:satır:sütun (mevcut)
4. **Kaynak satırı** — `^` ile işaretli (mevcut)
5. **İpucu** — "ne yapmalısın" (kısmen var, genişletilecek — Grup 4)

Hedef format örneği:

```
hata[T001]: tip uyumsuzlugu
   --> hasta.kem:12:23
   |
12 |     hasta.yas = "yetiskin";
   |                       ^^^^^^^^^^^ metin (beklenen: tam32)
   |
   ipucu: Bir tamsayi degeri verin (örn. 18) veya hasta.yas alaninin
          tipini metin olarak yeniden tanimlayin.
```
