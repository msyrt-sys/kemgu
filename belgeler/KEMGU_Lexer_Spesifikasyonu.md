# KEMGU Lexer Spesifikasyonu

**Sürüm:** 0.1  
**Tarih:** 2026-04-11  
**Durum:** Faz 0 — Tasarım  
**Dosya Uzantısı:** `.kem`  
**Kaynak Kodlama:** UTF-8 (zorunlu, BOM opsiyonel — varsa görmezden gelinir)

---

## 1. Genel Kurallar

- Kaynak dosyalar zorunlu olarak UTF-8 kodlanır. Geçersiz UTF-8 byte dizisi → derleme hatası.
- Dil **case-sensitive**'dir: `hasta` ve `Hasta` farklı tanımlayıcılardır.
- İfadeler (statement) **zorunlu noktalı virgül** (`;`) ile sonlandırılır.
- Satır sonu (`\n`, `\r\n`) whitespace olarak işlenir, özel anlamı yoktur.
- Metin interpolasyonu yoktur — metin literalleri tek bir token olarak işlenir.

---

## 2. Karakter Seti

### 2.1 Beyaz Boşluk (Whitespace)

| Karakter | Unicode | Açıklama |
|----------|---------|----------|
| Boşluk | U+0020 | Space |
| Yatay sekme | U+0009 | Tab |
| Satır sonu | U+000A | Line Feed (LF) |
| Satır başı | U+000D | Carriage Return (CR) |

`\r\n` dizisi tek bir satır sonu olarak sayılır (satır numarası takibi için).

Diğer Unicode whitespace karakterleri (U+00A0 no-break space, U+2003 em space vb.) beyaz boşluk olarak **kabul edilmez** — tanımlayıcıda kullanılamazlar, metin literali dışında hatadır.

### 2.2 Tanımlayıcı Karakter Seti (Whitelist)

KEMGU, genel Unicode Letter kategorisi yerine **kapalı bir karakter seti** kullanır:

**Başlangıç karakteri** (identifier_start):

| Aralık | Karakterler |
|--------|------------|
| ASCII Latin küçük | a-z |
| ASCII Latin büyük | A-Z |
| Alt çizgi | _ |
| Türkçe küçük | ç (U+00E7), ğ (U+011F), ı (U+0131), ö (U+00F6), ş (U+015F), ü (U+00FC) |
| Türkçe büyük | Ç (U+00C7), Ğ (U+011E), İ (U+0130), Ö (U+00D6), Ş (U+015E), Ü (U+00DC) |

**Devam karakteri** (identifier_continue):

- Tüm başlangıç karakterleri
- ASCII rakamlar: 0-9

**Önemli notlar:**

- Türkçe `ı` (U+0131) ve `İ` (U+0130) ayrı karakterlerdir. Locale bağımlı case dönüşümü **yapılmaz** — Lexer byte seviyesinde karşılaştırır.
- Maksimum tanımlayıcı uzunluğu: 255 UTF-8 byte (derleyici limiti, dil kuralı değil).
- Tanımlayıcılar anahtar kelimelerle çakışamaz — anahtar kelime tablosuna bakılır.

### 2.3 UTF-8 Byte Desenleri

C implementasyonunda karakter tanıma için kullanılacak byte desenleri:

| Karakter | UTF-8 Byte(lar) | Uzunluk |
|----------|-----------------|---------|
| a-z | 0x61–0x7A | 1 byte |
| A-Z | 0x41–0x5A | 1 byte |
| 0-9 | 0x30–0x39 | 1 byte |
| _ | 0x5F | 1 byte |
| ç | 0xC3 0xA7 | 2 byte |
| Ç | 0xC3 0x87 | 2 byte |
| ğ | 0xC4 0x9F | 2 byte |
| Ğ | 0xC4 0x9E | 2 byte |
| ı | 0xC4 0xB1 | 2 byte |
| İ | 0xC4 0xB0 | 2 byte |
| ö | 0xC3 0xB6 | 2 byte |
| Ö | 0xC3 0x96 | 2 byte |
| ş | 0xC5 0x9F | 2 byte |
| Ş | 0xC5 0x9E | 2 byte |
| ü | 0xC3 0xBC | 2 byte |
| Ü | 0xC3 0x9C | 2 byte |

---

## 3. Token Tipleri

### 3.1 Anahtar Kelimeler (30 adet)

Anahtar kelimeler byte-seviyesinde exact match ile eşleştirilir. Case-insensitive eşleşme **yapılmaz**.

| Anahtar Kelime | Kategori | Açıklama |
|---------------|----------|----------|
| `eğer` | Kontrol akışı | if |
| `değilse` | Kontrol akışı | else |
| `için` | Kontrol akışı | for |
| `iken` | Kontrol akışı | while |
| `eşleş` | Kontrol akışı | match |
| `ver` | Kontrol akışı | return |
| `işlev` | Tanımlama | function |
| `yapı` | Tanımlama | struct |
| `özellik` | Tanımlama | trait |
| `modül` | Tanımlama | module |
| `değişken` | Değişken | mutable variable (let mut) |
| `sabit` | Değişken | immutable variable (let / const) |
| `doğru` | Literal | true |
| `yanlış` | Literal | false |
| `boş` | Literal | void / unit |
| `ve` | Mantıksal | logical AND (&&) |
| `veya` | Mantıksal | logical OR (\|\|) |
| `değil` | Mantıksal | logical NOT (!) |
| `kullan` | Modül | use / import |
| `dışa` | Modül | export / pub |
| `tamam` | Sonuç tipi | Ok variant |
| `hata` | Sonuç tipi | Err variant |
| `bölge` | Bellek | region |
| `uygula` | Tip sistemi | impl |
| `kendin` | Tip sistemi | self |
| `seçimlik` | Tip sistemi | Option<T> |
| `sonuç` | Tip sistemi | Result<T,E> |
| `değer` | Tip sistemi | Some variant |
| `hiç` | Tip sistemi | None variant |
| `güvensiz` | Bellek | unsafe block (ham pointer / düşük seviye işlemler için) |

**Not:** `değer` ve `hiç` `seçimlik<T>` ile kullanım için anahtar kelime statüsünde
onaylandı (lexer.h: `TOK_DEGER`, `TOK_HIC`). `güvensiz` anahtar kelimesi formalizasyon
sürecinde Katman 3 (Güvensiz Blok) için eklendi (lexer.h: `TOK_GUVENSIZ`).

### 3.2 Tanımlayıcı (Identifier)

```
identifier = identifier_start (identifier_continue)*
```

Anahtar kelime tablosuyla eşleşiyorsa → anahtar kelime token'ı döner, değilse → tanımlayıcı token'ı.

### 3.3 Sayı Literalleri

#### Tam Sayılar

| Format | Önek | Geçerli Rakamlar | Örnek |
|--------|------|-----------------|-------|
| Onluk | (yok) | 0-9 | `42`, `1_000_000` |
| On altılık | `0x` veya `0X` | 0-9, a-f, A-F | `0xFF`, `0x1A_2B` |
| İkilik | `0b` veya `0B` | 0-1 | `0b1010`, `0b1111_0000` |
| Sekizlik | `0o` veya `0O` | 0-7 | `0o77`, `0o755` |

#### Ondalık Sayılar

```
ondalık = rakamlar '.' rakamlar üs?
üs     = ('e' | 'E') ('+' | '-')? rakamlar
```

Örnekler: `3.14`, `2.0`, `1_000.5`, `6.022e23`, `1.6E-19`

#### Underscore Kuralları

- Rakamlar arasında serbestçe kullanılabilir: `1_000_000`
- Önek ile ilk rakam arasında **kullanılamaz**: `0x_FF` → hata
- Başta **kullanılamaz**: `_100` → bu bir tanımlayıcıdır, sayı değil
- Sonda **kullanılamaz**: `100_` → hata
- Yan yana **kullanılamaz**: `1__000` → hata
- Ondalık noktanın hemen yanında **kullanılamaz**: `1_.0` → hata, `1._0` → hata

### 3.4 Metin Literalleri (String)

#### Normal Metin

Çift tırnak ile sınırlanır: `"merhaba dünya"`

**Kaçış dizileri:**

| Dizi | Anlamı |
|------|--------|
| `\\` | Ters eğik çizgi |
| `\"` | Çift tırnak |
| `\n` | Satır sonu (LF) |
| `\r` | Satır başı (CR) |
| `\t` | Yatay sekme |
| `\0` | Null byte |
| `\x41` | Hex byte (2 hex rakam, 0x00–0x7F arası) |
| `\u{1F600}` | Unicode kod noktası (1–6 hex rakam, süslü parantez zorunlu) |

Metin literali satır sonunu **içeremez** — çok satırlı metin için `\n` kaçışı kullanılır.

#### Raw Metin (Kaçış dizisi işlenmez)

Rust tarzı `r#"..."#` sözdizimi:

```
r"basit raw metin"
r#"içinde "tırnak" var"#
r##"içinde r#"iç içe"# var"##
```

**Kurallar:**
- `r` harfinden sonra sıfır veya daha fazla `#`, ardından `"` ile başlar
- Aynı sayıda `#` ve `"` ile biter
- İçindeki hiçbir karakter özel değildir — `\n` iki karakterdir (ters eğik çizgi + n)
- Satır sonu içerebilir (normal metinden farklı olarak)

**Lexer implementasyonu:**
1. `r` oku, ardından `#` sayısını say (açılış_seviye)
2. `"` oku — metin başladı
3. `"` gördüğünde ardından açılış_seviye kadar `#` gelip gelmediğini kontrol et
4. Geliyorsa → metin bitti, gelmiyorsa → bu karakterler metnin parçası

### 3.5 Karakter Literali

Tek tırnak ile sınırlanır, tam olarak bir Unicode kod noktası içerir:

```
'a'
'ğ'
'\n'
'\u{1F600}'
```

Aynı kaçış dizileri metin literalleriyle paylaşılır.

### 3.6 Operatörler ve Noktalama

#### Aritmetik

| Token | Açıklama |
|-------|----------|
| `+` | Toplama |
| `-` | Çıkarma / Negatif |
| `*` | Çarpma |
| `/` | Bölme |
| `%` | Mod |

#### Karşılaştırma

| Token | Açıklama |
|-------|----------|
| `==` | Eşit |
| `!=` | Eşit değil |
| `<` | Küçüktür |
| `>` | Büyüktür |
| `<=` | Küçük eşit |
| `>=` | Büyük eşit |

#### Atama

| Token | Açıklama |
|-------|----------|
| `=` | Atama |
| `+=` | Topla ve ata |
| `-=` | Çıkar ve ata |
| `*=` | Çarp ve ata |
| `/=` | Böl ve ata |
| `%=` | Mod ve ata |

#### Bit İşlemleri

| Token | Açıklama |
|-------|----------|
| `&` | Bitwise AND |
| `\|` | Bitwise OR |
| `^` | Bitwise XOR |
| `~` | Bitwise NOT |
| `<<` | Sola kaydır |
| `>>` | Sağa kaydır |

**Not:** Mantıksal AND/OR için `ve`/`veya` anahtar kelimeleri kullanılır, `&&`/`||` operatörleri **yoktur**.

#### Diğer Operatörler

| Token | Açıklama |
|-------|----------|
| `->` | Fonksiyon dönüş tipi, pointer dereference |
| `=>` | Match kolu |
| `::` | Yol ayracı (path separator) |
| `.` | Alan erişimi (field access) |
| `..` | Aralık (range) |
| `...` | Kalan argümanlar (variadic — gelecek kullanım için ayrılmış) |

#### Ayraçlar ve Noktalama

| Token | Açıklama |
|-------|----------|
| `(` `)` | Parantez |
| `{` `}` | Süslü parantez |
| `[` `]` | Köşeli parantez |
| `<` `>` | Açılı parantez (tip parametresi bağlamında) |
| `,` | Virgül |
| `:` | İki nokta (tip belirteci) |
| `;` | Noktalı virgül (ifade sonlandırıcı) |

**`<` ve `>` belirsizliği:** Lexer düzeyinde `<` ve `>` her zaman karşılaştırma operatörü olarak tokenlanır. Açılı parantez (generic) kullanımı Parser tarafından bağlamdan çıkarılır.

### 3.7 Yorumlar

```
// Bu bir satır yorumudur — satır sonuna kadar devam eder

/* Bu bir
   blok yorumudur
   birden fazla satıra yayılabilir */

/* Blok yorumlar /* iç içe geçebilir */ bu şekilde */
```

**İç içe blok yorum:** KEMGU, iç içe geçen blok yorumları destekler. Lexer `/*` sayacını tutar, her `/*` artırır, her `*/` azaltır. Sayaç sıfıra döndüğünde yorum biter. Bu, kod bloklarını yorum içine almayı kolaylaştırır.

---

## 4. Token Veri Yapısı

Her token şu bilgileri taşır:

```c
typedef enum {
    // Anahtar kelimeler
    TOK_EGER,         // eğer
    TOK_DEGILSE,      // değilse
    TOK_ICIN,         // için
    TOK_IKEN,         // iken
    TOK_ESLES,        // eşleş
    TOK_VER,          // ver
    TOK_ISLEV,        // işlev
    TOK_YAPI,         // yapı
    TOK_OZELLIK,      // özellik
    TOK_MODUL,        // modül
    TOK_DEGISKEN,     // değişken
    TOK_SABIT,        // sabit
    TOK_DOGRU,        // doğru
    TOK_YANLIS,       // yanlış
    TOK_BOS,          // boş
    TOK_VE,           // ve
    TOK_VEYA,         // veya
    TOK_DEGIL,        // değil
    TOK_KULLAN,       // kullan
    TOK_DISA,         // dışa
    TOK_TAMAM,        // tamam
    TOK_HATA,         // hata
    TOK_BOLGE,        // bölge
    TOK_UYGULA,       // uygula
    TOK_KENDIN,       // kendin
    TOK_SECIMLIK,     // seçimlik
    TOK_SONUC,        // sonuç
    TOK_DEGER,        // değer
    TOK_HIC,          // hiç
    TOK_GUVENSIZ,     // güvensiz

    // Literaller
    TOK_TAMSAYI,      // 42, 0xFF, 0b1010
    TOK_ONDALIK,      // 3.14, 1.6e-19
    TOK_METIN,        // "merhaba"
    TOK_HAM_METIN,    // r#"raw"#
    TOK_KARAKTER,     // 'a'

    // Tanımlayıcı
    TOK_TANIMLAYICI,  // hasta, hastaÖzeti

    // Operatörler
    TOK_ARTI,         // +
    TOK_EKSI,         // -
    TOK_YILDIZ,       // *
    TOK_BOLU,         // /
    TOK_MOD,          // %
    TOK_ESIT,         // =
    TOK_ESIT_ESIT,    // ==
    TOK_ESIT_DEGIL,   // !=
    TOK_KUCUK,        // <
    TOK_BUYUK,        // >
    TOK_KUCUK_ESIT,   // <=
    TOK_BUYUK_ESIT,   // >=
    TOK_ARTI_ESIT,    // +=
    TOK_EKSI_ESIT,    // -=
    TOK_YILDIZ_ESIT,  // *=
    TOK_BOLU_ESIT,    // /=
    TOK_MOD_ESIT,     // %=
    TOK_VE_BIT,       // &
    TOK_VEYA_BIT,     // |
    TOK_OZVEYA_BIT,   // ^
    TOK_DEGIL_BIT,    // ~
    TOK_SOLA_KAYDIR,  // <<
    TOK_SAGA_KAYDIR,  // >>
    TOK_OK,           // ->
    TOK_KALIN_OK,     // =>
    TOK_CIFT_IKI_NOKTA, // ::
    TOK_NOKTA,        // .
    TOK_ARALIK,       // ..
    TOK_UC_NOKTA,     // ...

    // Ayraçlar
    TOK_SOL_PAREN,    // (
    TOK_SAG_PAREN,    // )
    TOK_SOL_SUSLU,    // {
    TOK_SAG_SUSLU,    // }
    TOK_SOL_KOSELI,   // [
    TOK_SAG_KOSELI,   // ]
    TOK_VIRGUL,       // ,
    TOK_IKI_NOKTA,    // :
    TOK_NOKTALI_VIRGUL, // ;

    // Özel
    TOK_DOSYA_SONU,   // EOF
    TOK_HATALI,       // Geçersiz token
} TokenTipi;

typedef struct {
    TokenTipi tip;
    const char *baslangic;   // Kaynak koddaki başlangıç pointer'ı
    int uzunluk;             // Token'ın byte uzunluğu
    int satir;               // Satır numarası (1'den başlar)
    int sutun;               // Sütun numarası (1'den başlar, UTF-8 byte konumu)
} Token;
```

---

## 5. Lexer State Machine

### 5.1 Ana Döngü

```
BAŞLANGIÇ
  │
  ├─ whitespace → atla, devam et
  │
  ├─ '/' →
  │   ├─ '/' → SATIR_YORUMU (satır sonuna kadar atla)
  │   ├─ '*' → BLOK_YORUMU (iç içe sayaçlı)
  │   ├─ '=' → TOK_BOLU_ESIT
  │   └─ (diğer) → TOK_BOLU
  │
  ├─ identifier_start →
  │   └─ TANIMLAYICI_OKU → anahtar kelime tablosu kontrolü
  │       ├─ eşleşme var → anahtar kelime token'ı
  │       └─ eşleşme yok → TOK_TANIMLAYICI
  │
  ├─ rakam (0-9) → SAYI_OKU
  │   ├─ '0' sonrası 'x'/'X' → ON_ALTILIK
  │   ├─ '0' sonrası 'b'/'B' → IKILIK
  │   ├─ '0' sonrası 'o'/'O' → SEKIZLIK
  │   ├─ '.' sonrası rakam → ONDALIK
  │   └─ 'e'/'E' → US
  │
  ├─ '"' → METIN_OKU (kaçış dizileri işlenir)
  │
  ├─ 'r' →
  │   ├─ ardından '#'* '"' → HAM_METIN_OKU
  │   └─ ardından identifier_continue → TANIMLAYICI_OKU ('r' ile başlayan tanımlayıcı)
  │
  ├─ '\'' → KARAKTER_OKU
  │
  ├─ operatör/noktalama karakteri → OPERATOR_OKU
  │   (en uzun eşleşme: '>>' > '>', '==' > '=' vb.)
  │
  └─ diğer → TOK_HATALI + hata mesajı
```

### 5.2 Raw Metin State Machine

```
r GÖRDÜK
  │
  ├─ '#' say → seviye = '#' sayısı
  ├─ '"' gördük → HAM_METIN_ICINDE
  │   │
  │   └─ her karakter için:
  │       ├─ '"' gördük → '#' saymaya başla
  │       │   ├─ seviye kadar '#' geldi → HAM_METIN_BITTI
  │       │   └─ '#' sayısı yetersiz → metin devam ediyor
  │       ├─ EOF → hata: "kapatılmamış raw metin"
  │       └─ diğer → metin içeriğine ekle
  │
  └─ '#' veya '"' değil → bu 'r' ile başlayan bir tanımlayıcı
```

### 5.3 İç İçe Blok Yorum

```
'/*' GÖRDÜK, derinlik = 1
  │
  └─ her karakter için:
      ├─ '/*' → derinlik++
      ├─ '*/' → derinlik--
      │   └─ derinlik == 0 → YORUM_BITTI
      ├─ EOF → hata: "kapatılmamış blok yorumu"
      └─ diğer → atla
```

---

## 6. Anahtar Kelime Eşleştirme

### 6.1 Strateji

Tanımlayıcı okunduktan sonra anahtar kelime tablosuna bakılır. İki implementasyon seçeneği:

**Seçenek A — Hash tablosu:** UTF-8 byte dizisi üzerinden hash hesaplanır, tabloda aranır. O(1) ortalama.

**Seçenek B — Trie (Önek ağacı):** Her byte için dallanma. UTF-8 byte'ları doğrudan trie node'ları olur. O(n) where n = kelime uzunluğu.

**Öneri:** İlk implementasyon için sıralı dizi + binary search yeterli (29 anahtar kelime için). Optimizasyon sonra yapılır.

### 6.2 Anahtar Kelime Tablosu (UTF-8 Byte Dizileri)

```c
typedef struct {
    const char *metin;    // UTF-8 byte dizisi
    int uzunluk;          // Byte uzunluğu
    TokenTipi tip;
} AnahtarKelime;

static const AnahtarKelime anahtar_kelimeler[] = {
    {"boş",       4,  TOK_BOS},
    {"bölge",     6,  TOK_BOLGE},
    {"değer",     6,  TOK_DEGER},
    {"değil",     6,  TOK_DEGIL},
    {"değilse",   8,  TOK_DEGILSE},
    {"değişken", 10,  TOK_DEGISKEN},
    {"doğru",     6,  TOK_DOGRU},
    {"dışa",      5,  TOK_DISA},
    {"eğer",      5,  TOK_EGER},
    {"eşleş",     7,  TOK_ESLES},
    {"güvensiz",  9,  TOK_GUVENSIZ},
    {"hata",      4,  TOK_HATA},
    {"hiç",       4,  TOK_HIC},
    {"için",      5,  TOK_ICIN},
    {"iken",      4,  TOK_IKEN},
    {"işlev",     6,  TOK_ISLEV},
    {"kendin",    6,  TOK_KENDIN},
    {"kullan",    6,  TOK_KULLAN},
    {"modül",     6,  TOK_MODUL},
    {"özellik",   8,  TOK_OZELLIK},
    {"sabit",     5,  TOK_SABIT},
    {"seçimlik", 10,  TOK_SECIMLIK},
    {"sonuç",     6,  TOK_SONUC},
    {"tamam",     5,  TOK_TAMAM},
    {"uygula",    6,  TOK_UYGULA},
    {"ve",        2,  TOK_VE},
    {"ver",       3,  TOK_VER},
    {"veya",      4,  TOK_VEYA},
    {"yanlış",    7,  TOK_YANLIS},
    {"yapı",      5,  TOK_YAPI},
};
// Alfabetik sıralı — binary search için
// NOT: Byte uzunlukları UTF-8 çok byte'lı karakterleri yansıtır
// "değişken" = d(1) + e(1) + ğ(2) + i(1) + ş(2) + k(1) + e(1) + n(1) = 10 byte
```

---

## 7. Hata Raporlama

Lexer hataları kullanıcı dostu olmalıdır:

### 7.1 Hata Formatı

```
hata[L001]: kapatılmamış metin literali
  --> dosya.kem:12:5
   |
12 | değişken ad = "Mehmet
   |               ^ metin burada başlıyor ama hiç kapatılmamış
   |
   = ipucu: metni kapatmak için '"' ekleyin
```

### 7.2 Hata Kodları

| Kod | Açıklama |
|-----|----------|
| L001 | Kapatılmamış metin literali |
| L002 | Kapatılmamış karakter literali |
| L003 | Kapatılmamış blok yorumu |
| L004 | Kapatılmamış raw metin literali |
| L005 | Geçersiz kaçış dizisi |
| L006 | Geçersiz sayı formatı |
| L007 | Geçersiz UTF-8 byte dizisi |
| L008 | Beklenmeyen karakter |
| L009 | Boş karakter literali |
| L010 | Karakter literalinde birden fazla karakter |
| L011 | Sayıda geçersiz underscore konumu |
| L012 | Hex/binary/octal literalde geçersiz rakam |
| L013 | Unicode kaçışında geçersiz kod noktası |
| L014 | Tanımlayıcı çok uzun (>255 byte) |

### 7.3 Satır/Sütun Takibi

Lexer, doğru hata konumu raporlamak için şunları izler:

- **Satır numarası:** `\n` veya `\r\n` gördüğünde artırılır (1'den başlar)
- **Sütun numarası:** Her byte'da artırılır, satır sonunda sıfırlanır (1'den başlar)
- **Sütun birimi:** UTF-8 byte konumu (karakter konumu değil — editörlerle uyumlu)

---

## 8. Örnek Tokenizasyon

### 8.1 Girdi

```kemgu
kemguyapı Hasta {
    ad: metin;
    yaş: tam32;
    tanı: seçimlik<metin>;
}

işlev hastaÖzeti(h: Hasta) -> metin {
    ver "Ad: " + h.ad;
}
```

### 8.2 Token Akışı

```
TOK_YAPI        "yapı"          1:1
TOK_TANIMLAYICI "Hasta"         1:6
TOK_SOL_SUSLU   "{"             1:12
TOK_TANIMLAYICI "ad"            2:5
TOK_IKI_NOKTA   ":"             2:7
TOK_TANIMLAYICI "metin"         2:9
TOK_NOKTALI_VIRGUL ";"          2:14
TOK_TANIMLAYICI "yaş"           3:5
TOK_IKI_NOKTA   ":"             3:9
TOK_TANIMLAYICI "tam32"         3:11
TOK_NOKTALI_VIRGUL ";"          3:16
TOK_TANIMLAYICI "tanı"          4:5
TOK_IKI_NOKTA   ":"             4:10
TOK_SECIMLIK    "seçimlik"      4:12
TOK_KUCUK       "<"             4:21
TOK_TANIMLAYICI "metin"         4:22
TOK_BUYUK       ">"             4:27
TOK_NOKTALI_VIRGUL ";"          4:28
TOK_SAG_SUSLU   "}"             5:1
TOK_ISLEV       "işlev"         7:1
TOK_TANIMLAYICI "hastaÖzeti"    7:7
TOK_SOL_PAREN   "("             7:18
TOK_TANIMLAYICI "h"             7:19
TOK_IKI_NOKTA   ":"             7:20
TOK_TANIMLAYICI "Hasta"         7:22
TOK_SAG_PAREN   ")"             7:27
TOK_OK          "->"            7:29
TOK_TANIMLAYICI "metin"         7:32
TOK_SOL_SUSLU   "{"             7:38
TOK_VER         "ver"           8:5
TOK_METIN       "\"Ad: \""     8:9
TOK_ARTI        "+"             8:16
TOK_TANIMLAYICI "h"             8:18
TOK_NOKTA       "."             8:19
TOK_TANIMLAYICI "ad"            8:20
TOK_NOKTALI_VIRGUL ";"          8:22
TOK_SAG_SUSLU   "}"             9:1
TOK_DOSYA_SONU  ""              10:1
```

---

## 9. C İmplementasyon Notları

### 9.1 Dosya Yapısı

```
kemgu/
├── src/
│   ├── lexer.h          // Token tipleri, Token yapısı, API
│   ├── lexer.c          // Lexer implementasyonu
│   ├── anahtar_kelime.c // Anahtar kelime tablosu ve arama
│   ├── utf8.h           // UTF-8 yardımcı fonksiyonlar
│   ├── utf8.c           // UTF-8 karakter tanıma (whitelist)
│   ├── hata.h           // Hata raporlama API
│   ├── hata.c           // Hata formatlama ve gösterimi
│   └── ana.c            // Test sürücüsü (main)
├── test/
│   ├── test_lexer.c     // Birim testleri
│   └── ornekler/        // .kem test dosyaları
├── Makefile
└── README.md
```

### 9.2 Temel API

```c
// lexer.h

typedef struct {
    const char *kaynak;       // Kaynak kodun başlangıcı
    const char *simdiki;      // Şu anki pozisyon
    int satir;                // Geçerli satır
    int sutun;                // Geçerli sütun
    const char *dosya_adi;    // Hata raporlama için
} Lexer;

// Lexer'ı başlat
void lexer_baslat(Lexer *lexer, const char *kaynak, const char *dosya_adi);

// Sonraki token'ı oku
Token lexer_sonraki_token(Lexer *lexer);

// Sonraki token'a bak (ilerlemeden)
Token lexer_onizle(Lexer *lexer);
```

### 9.3 UTF-8 Yardımcı Fonksiyonlar

```c
// utf8.h

// Byte'ın çok byte'lı dizinin başlangıcı olup olmadığını kontrol et
int utf8_baslangic_mi(unsigned char byte);

// Bir UTF-8 karakterinin byte uzunluğunu döndür (1-4)
int utf8_karakter_uzunlugu(unsigned char ilk_byte);

// Karakterin tanımlayıcı başlangıç karakteri olup olmadığını kontrol et
int utf8_tanimlayici_baslangic_mi(const char *s);

// Karakterin tanımlayıcı devam karakteri olup olmadığını kontrol et
int utf8_tanimlayici_devam_mi(const char *s);

// Türkçe karakter whitelist kontrolü
int utf8_turkce_harf_mi(const char *s, int *byte_uzunlugu);
```

---

## 10. Test Stratejisi

### 10.1 Birim Testleri

Her token tipi için en az:
- Temel durum (happy path)
- Sınır durumu (edge case)
- Hata durumu (error case)

### 10.2 Kritik Test Vakaları

```c
// Türkçe karakterler
"değişken"   → TOK_DEGISKEN (anahtar kelime olarak tanınmalı)
"değişkenX"  → TOK_TANIMLAYICI (anahtar kelime + devam = tanımlayıcı)
"ışık"       → TOK_TANIMLAYICI (ı ile başlayan tanımlayıcı)
"İstanbul"   → TOK_TANIMLAYICI (İ ile başlayan tanımlayıcı)

// ı/İ ayrımı
"ıslak"      → TOK_TANIMLAYICI
"İslak"      → TOK_TANIMLAYICI (farklı token'lar — case sensitive)

// Underscore sayılar
"1_000"      → TOK_TAMSAYI, değer = 1000
"1__000"     → TOK_HATALI, L011
"_değer"     → TOK_TANIMLAYICI (underscore ile başlayan = tanımlayıcı)
"1_"         → TOK_HATALI, L011

// Raw metin
r"basit"             → TOK_HAM_METIN
r#"tırnak "var" "#   → TOK_HAM_METIN (sondaki boşluk dahil)
r##"r#"iç"#"##       → TOK_HAM_METIN

// Operatör belirsizlikleri
"<="         → TOK_KUCUK_ESIT (tek token, iki değil)
"< ="        → TOK_KUCUK + TOK_ESIT (iki ayrı token)
"->>"        → TOK_OK + TOK_BUYUK (greedy matching: -> sonra >)
">>"         → TOK_SAGA_KAYDIR (tek token)
">> "        → TOK_SAGA_KAYDIR
"> >"        → TOK_BUYUK + TOK_BUYUK

// İç içe yorum
"/* /* iç */ dış */"  → yorum (tamamen atlanır)
"/* kapatılmamış"     → TOK_HATALI, L003
```

---

## 11. Açık Konular ve Gelecek Kararlar

| # | Konu | Durum | Not |
|---|------|-------|-----|
| 1 | `değer` ve `hiç` anahtar kelimeleri | Onay bekliyor | Proje özetinde kullanılmış ama orijinal listede yok |
| 2 | Metin interpolasyonu | Ertelendi | Faz 2'de değerlendirilebilir |
| 3 | Tip ekleri (`42u32`, `3.14f64`) | Karar gerekli | Sayı literallerine tip eki eklenecek mi? |
| 4 | Çok satırlı normal metin | Hayır | Şu an satır sonu içeremez, raw metin kullanılmalı |
| 5 | Shebang satırı | Karar gerekli | `#!/usr/bin/kemgu` desteği olacak mı? |
| 6 | Attribute/annotation syntax | Ertelendi | `#[...]` veya `@...` gibi meta bilgi syntax'ı |
