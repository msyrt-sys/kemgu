# `sonuç` / `seçimlik` Value Codegen (C2.5)

Bu belge, `sonuç<T,H>` ve `seçimlik<T>` tiplerinin LLVM-IR temsilini,
yapıcı (`tamam`/`hata`/`değer`/`hiç`) lowering'ini ve `eşleş` destructuring'ini
açıklar. Tamamı `src/llvm.c` (text IR üretici) içindedir; runtime/FFI yoktur
(inline IR, WCET dostu).

## Tagged-union temsili

Her iki tip de bir **tag + payload aggregate**'i olarak temsil edilir. Payload
için **ayrık alanlar** kullanılır (union/bitcast değil): bu, struct payload'ları
(ör. `sonuç<BlkAygit, Hata>`) by-value, hizalama bulmacası olmadan taşır ve
mevcut struct-by-value ABI'siyle birebir aynıdır.

| KEMGU tip        | LLVM IR aggregate        | Alanlar                                   |
|------------------|--------------------------|-------------------------------------------|
| `sonuç<T,H>`     | `{i8, <T>, <H>}`         | 0: tag, 1: `tamam` payload (T), 2: `hata` payload (H) |
| `seçimlik<T>`    | `{i8, <T>}`              | 0: tag, 1: `değer` payload (T)            |

### Tag değerleri

| Tip        | tag = 0   | tag = 1 |
|------------|-----------|---------|
| `sonuç`    | `tamam`   | `hata`  |
| `seçimlik` | `değer`   | `hiç`   |

`hiç` için payload alanı yoktur (yalnız `{i8, T}`'nin tag'i set edilir, T alanı
`undef` kalır).

### Hizalama (alignment)

Aggregate, anonim **literal struct** tipidir (`{i8, i32, ptr}` gibi). Hizalama
ve alan offset'leri LLVM'in doğal struct layout kuralına bırakılır (target data
layout'a göre); elle padding/hizalama yapılmaz. Bu, `extractvalue`/`getelementptr`
ile alan erişimini target-bağımsız tutar.

### `void`/`boş` payload

LLVM struct alanları sized olmalıdır; `boş` (void) payload `i8` dummy'ye
eşlenir (`ast_tip_to_ir`). Yapıcıda void payload için store yapılmaz.

## Yapıcı lowering (`yapici_uret`)

`tamam(x)` / `hata(e)` / `değer(x)` / `hiç`, generic `call` yerine **inline
inşa** edilir (`yapi_olustur_uret` ile aynı desen):

```llvm
%a = alloca {i8, i32, ptr}                         ; sonuç<tam32, metin>
%t = getelementptr {i8, i32, ptr}, ptr %a, i32 0, i32 0
store i8 0, ptr %t                                 ; tag = tamam
%p = getelementptr {i8, i32, ptr}, ptr %a, i32 0, i32 1
store i32 %deger, ptr %p                           ; payload (alan 1)
%v = load {i8, i32, ptr}, ptr %a                   ; by-value
ret {i8, i32, ptr} %v
```

`@tamam`/`@hata` tanımsız sembolleri tamamen kalkar.

### Beklenen-tip kanalı

Yapıcı, tam tipi (T **ve** H) tek başına bilemez (örn. `tamam(x)` yalnız T'yi
verir). Tam yapısal tip, çağrı bağlamından `LlvmGen.beklenen_tip` (AST tip
düğümü) ile akıtılır:

- `ver tamam(x)` → beklenen = aktif işlevin dönüş tipi (`aktif_donus_dugum`).
- `değişken r: sonuç<...> = tamam(x)` → beklenen = değişken annotasyonu.

İç içe yapıcılar için `beklenen_tip` payload tipiyle save/restore edilir.

## `eşleş` destructuring

`eşleş r { tamam(v) => ... hata(e) => ... }` (C1 switch lowering'inin üstüne):

1. scrutinee `{i8, ...}` aggregate olarak bir kez yüklenir.
2. Her yapıcı kolu: `extractvalue {..} %s, 0` ile tag okunur, `icmp eq i8`
   ile yapıcının tag'iyle (tamam/değer=0, hata/hiç=1) karşılaştırılır, kola
   dallanılır.
3. Kol gövdesinde: payload `extractvalue` ile çıkarılır (tamam/değer → alan 1,
   hata → alan 2) ve bağlı değişkene (`v`/`e`) alloca+store ile atanır; isim
   tablosuna eklenir. Payload tipi, scrutinee aggregate string'inden
   `agg_alan_ir` ile ayrıştırılır.
4. Her kol bloğu kendi terminator'ıyla biter (C1 invariant'ı korunur).

`hiç` deseni `DUGUM_DESEN_TANIMLAYICI` olarak parse edilir; catch-all sayılmaz
(tag==1 kontrolüne yönlendirilir).

## ABI

`sonuç`/`seçimlik` dönüş ve parametre olarak **by-value aggregate** geçer
(`define {i8, T, H} @f({i8, T, H} %r)`), mevcut struct-by-value yolunu kullanır.
Dönüş tipi artık `i32`'ye çökmez.

## Kapsam dışı (C2.7)

- Custom enum/ADT tanımı (`seçimlik Durum { Bos, Dolu }`) — yok (syntax kararı).
- `eşleş` exhaustiveness denetimi (eksik kol → derleme hatası) — yok.

## Kaynak

`src/llvm.c`: `ast_tip_to_ir` (aggregate), `yapici_uret` / `yapici_bilgi` /
`agg_alan_ir` / `yapici_desen_mi` (yardımcılar), `DUGUM_CAGRI` (yapıcı dispatch),
`tanimlayici_yukle` (`hiç`), `DUGUM_ESLES` (destructuring), `DUGUM_VER` /
`DUGUM_DEGISKEN` (beklenen-tip kanalı).
Testler: `test/snapshots/sonuc_secimlik.kem`, `test/snapshots/sonuc_struct_payload.kem`.
