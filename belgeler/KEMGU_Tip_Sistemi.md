# KEMGU Tip Sistemi

KEMGU tip sistemi **nominal eşitlik + lokal/bidirectional çıkarsama +
monomorphization** modeline dayanır.

Bu belge tip kontrolünün operasyonel davranışını açıklar — formal teoremler
(Type Soundness vs.) ayrı belgeye gelecek.

## Modüller

- `src/tip.h`/`src/tip.c` — `TipBilgisi` temsili (16 kategori), eşitlik, yazdırma
- `src/sembol.h`/`src/sembol.c` — Sembol tablosu (scope hiyerarşisi)
- `src/tip_kontrol.h`/`src/tip_kontrol.c` — Tip kontrol motoru, 26 hata kodu

## Tip Kategorileri

### Basit tipler (14 adet)

| KEMGU adı | LLVM tipi | Boyut (byte) |
|-----------|-----------|--------------|
| `tam8` / `dtam8` | `i8` | 1 |
| `tam16` / `dtam16` | `i16` | 2 |
| `tam32` / `dtam32` | `i32` | 4 |
| `tam64` / `dtam64` | `i64` | 8 |
| `kesirli32` | `float` | 4 |
| `kesirli64` | `double` | 8 |
| `karakter` | `i32` | 4 (UTF-32 code point) |
| `mantıksal` | `i1` | 1 |
| `metin` | `{ ptr, i64 }` | 16 (slice: ptr + uzunluk) |
| `boş` | `void` | 0 |

### Bileşik tipler

| Tip | LLVM eşlemesi | Sözdizimi |
|-----|---------------|-----------|
| `TIP_REFERANS` | `ptr` | `&T`, `&değişken T` |
| `TIP_POINTER` | `ptr` | `*T` (güvensiz) |
| `TIP_DIZI` | `{ ptr, i64 }` (slice) | `Dizi<T>` |
| `TIP_SECIMLIK` | `{ i32, i64 }` (tag + payload) | `seçimlik<T>` |
| `TIP_SONUC` | `{ i32, i64 }` | `sonuç<T, H>` |
| `TIP_ISLEV` | `ptr` (function pointer) | `işlev(T1, T2) -> T` |
| `TIP_YAPI` | `%struct.X` (named) | `yapı X { ... }` |
| `TIP_GENERIC_PARAM` | (substitusyon) | `T` (yapı içinde) |

### Özel

| Tip | Anlam |
|-----|-------|
| `TIP_BILINMIYOR` | Henüz çözülmemiş (cikarsama beklenir) |
| `TIP_HATA` | Tip kontrolü başarısız (yer tutucu — fazla hatayı önler) |

## Eşitlik: Nominal

İki tipin "aynı" sayılması için **isim + structural args** eşit olmalı:

```kem
yapı Hasta { ad: metin; yas: tam32; }
yapı Personel { ad: metin; yas: tam32; }

değişken h: Hasta = ...;
değişken p: Personel = h;   // HATA T001: tip uyumsuzlugu
```

Rust/Java tarzı. Yapısal eşitlik (TypeScript stili) kabul edilmedi —
direktif "açık her şey" ilkesine ters.

`tip_esit(a, b)` recursive karşılaştırma yapar:
- Basit kategori → kategori eşitliği
- Referans/Pointer/Dizi → hedef/eleman recursive
- Yapı → ad + tip_arg recursive
- İşlev → param sayısı + sıralı recursive + dönüş recursive

## Sembol Tablosu

`Scope` parent pointer'lı linked list.

| Scope türü | Bağlam |
|------------|--------|
| `SCOPE_GLOBAL` | Program kökü |
| `SCOPE_MODUL` | `modül X { ... }` |
| `SCOPE_ISLEV` | İşlev gövdesi (parametreler dahil) |
| `SCOPE_BLOK` | İç blok, döngü gövdesi |
| `SCOPE_YAPI` | Yapı alanları + generic params |

Sembol kategorileri:

| Kategori | Anlam |
|----------|-------|
| `SEMBOL_DEGISKEN` | `değişken x = ...` |
| `SEMBOL_SABIT` | `sabit X = ...` |
| `SEMBOL_PARAMETRE` | İşlev/lambda parametresi |
| `SEMBOL_ISLEV` | İşlev tanımı |
| `SEMBOL_YAPI` | Yapı tanımı |
| `SEMBOL_OZELLIK` | `özellik` (trait) — kullanımı yarım |
| `SEMBOL_MODUL` | Modül |
| `SEMBOL_GENERIC_PARAM` | `yapı X<T>` içindeki T |

### Lookup Davranışı

- `sembol_bul(scope, ad, n)` — parent zincirini gezer
- `sembol_bul_yerel(scope, ad, n)` — sadece bu scope (shadowing kontrolü için)
- `sembol_yapi_alani(yapi_sembolu, ad, n)` — yapı'nın iç scope'unda alan arar
- `sembol_modul_scope(modul_sembolu)` — modülün scope'unu döner

## Tip Kontrol Akışı

```
tip_kontrol_program(prog):
    pre_populate(prog)              # Pass 1: yapılar + işlevler global'e
    for uye in prog:
        tip_kontrol_tanim(uye)      # Pass 2: gövde kontrolü
```

### Pre-populate (iki geçişli)

**Geçiş 1a — yapılar:**
- Her `DUGUM_YAPI` için yapı_scope oluştur
- Generic params (`<T>`) yapı_scope'a ekle
- Alanları yapı_scope'a ekle (alan tipleri o context'te resolve)
- Yapı sembolünü global'e ekle

**Geçiş 1b — işlevler ve sabitler:**
- Her `DUGUM_ISLEV` için imza tipi (param tipleri + dönüş) oluştur
- Sembol global'e

Bu ikili geçiş **forward reference**'a izin verir — `f` `g`'yi g'den önce çağırabilir.

### Tanım kontrolü

Her tanım için:
- **İşlev:** Yeni SCOPE_ISLEV, parametreler eklenir, aktif_donus_tipi set edilir, gövde recursive
- **Yapı:** Pre-populate yeterli (alanlar tip resolve'undan geçer)
- **Sabit:** Annot vs değer eşleşmesi (bidirectional)
- **Modül:** Üyeler recursive

### İfade tip belirleme

`tip_belirle(d)` — AST visitor. Her ifade için:

| Düğüm | Tip |
|-------|-----|
| `DUGUM_TAM` | Default `TIP_TAM32` (bidirectional ile context'e göre değişir) |
| `DUGUM_KESIRLI` | `TIP_KESIRLI64` |
| `DUGUM_METIN` | `TIP_METIN` |
| `DUGUM_MANTIKSAL` | `TIP_MANTIKSAL` |
| `DUGUM_TANIMLAYICI` | Sembol tablosundan |
| `DUGUM_IKILI` | Operatöre göre (aritmetik, karşılaştırma, mantıksal, bit) |
| `DUGUM_TEKLI` | `-`, `değil`, `~`, `&`, `*` operatörlerine göre |
| `DUGUM_CAGRI` | İşlev tipinin dönüş tipi |
| `DUGUM_ERISIM` | Yapı alan tipi (otomatik dereference) |
| `DUGUM_INDEKS` | `Dizi<T>` → `T` |
| `DUGUM_YAPI_OLUSTUR` | Yapı tipi (alanlar kontrol edilir) |
| `DUGUM_DIZI_OLUSTUR` | `Dizi<T>` (T ilk elemandan) |
| `DUGUM_LAMBDA` | `işlev(T1, T2) -> T` (parametre annot zorunlu) |
| `DUGUM_BOYUT` | `TIP_DTAM64` (bidirectional ile daralabilir) |
| `DUGUM_YOL` | Modül üyesinin tipi |

### Bidirectional çıkarsama

`tip_belirle_beklenen(d, beklenen)` — context-aware:

- **Sayı literal** + tamsayı beklenen → o tipte (`tam8`, `tam64` vs.)
- **Kesirli literal** + `kesirli32`/`kesirli64` → o tipte
- **Boş dizi** + `Dizi<T>` → `Dizi<T>`
- **Yapı oluşturma** + generic yapı → tip_arg substitusyon
- **İkili aritmetik** + tamsayı → her iki operand'a yayım
- **Çağrı arg** → parametre tipi context'i
- **`boyut<T>`** + tamsayı beklenen → o tipte (default dtam64'ten daralır)

Bağlam zincirleme yayılır:

```kem
değişken x: tam8 = 1 + 2;        // 1 ve 2 ikisi de tam8
değişken xs: Dizi<tam64> = [];   // boş dizi -> Dizi<tam64>
f(g(5));                          // g param tam16 -> 5 tam16
```

## Generic Substitusyon (Monomorphization-ready)

`Kutu<T>` için tip kontrol:

```kem
yapı Kutu<T> { eleman: T; }
değişken k: Kutu<tam32> = Kutu { eleman: 5 };  // T → tam32 substitusyon
değişken e: tam32 = k.eleman;                   // k.eleman → tam32
```

`substitusyon(t, yapi_sem, yapi_tipi)` recursive olarak `TIP_GENERIC_PARAM`'leri
concrete tipe map eder. Yapı oluşturma + alan erişimi her ikisinde de kullanılır.

**LLVM monomorphization codegen henüz yok** — şu an tip sistemi farkında ama
LLVM `Kutu<tam32>` ve `Kutu<metin>` aynı `%struct.Kutu`'ya gider. İleri iyileştirme.

## Hata Kodları

`belgeler/KEMGU_Hata_Kodlari.md` — tip hataları T001–T030.

## İntrinsic Predeclared

`tip_kontrol_baslat` global scope'a şunları ekler:

- **Inline asm + volatile** (9 intrinsic):
  - `_asm(metin) -> boş`
  - `_oku_volatile_dtam{8,16,32,64}(adres) -> dtamN`
  - `_yaz_volatile_dtam{8,16,32,64}(adres, deger) -> boş`

- **Atomic ops + barriers** (23 intrinsic):
  - `_atomik_oku_dtamN`, `_atomik_yaz_dtamN`, `_atomik_topla_dtamN`
  - `_atomik_takas_dtamN`, `_atomik_cas_dtamN`
  - `_bellek_engeli`, `_oku_engeli`, `_yaz_engeli`

- **Concurrency Katman 2** (5 intrinsic):
  - `_gorev_baslat`, `_gorev_birlestir`
  - `_kanal_olustur`, `_kanal_gonder`, `_kanal_al`

Toplam: **37 predeclared intrinsic** — sistem programlama için.

## Test

`test/test_tip_kontrol.c` — **104 test** ASan altında temiz.

## Bilinen Sınırlamalar

- `hiç`/`değer(...)` ifade context'inde çözümlenmiyor (parser eksiği)
- `eşleş` desen tanımlayıcıları kol gövdesi scope'una otomatik eklenmiyor
- Trait (`özellik` + `uygula`) sistemi yarım — codegen yok
- Bölge polimorfizmi yok — `&ρ T` syntax eklenmedi
- Linear types yok — B grubu spec bekliyor (direktif Hedef 1 — Bölüm 6)
- constant_time qualifier yok — B grubu
- Capability tokens yok — B grubu
