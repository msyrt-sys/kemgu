# `çeşit` — Custom Sum Type (C2.7 v1)

`çeşit`, isimli varyantlardan oluşan bir cebirsel veri tipidir (sum type /
tagged enum). "Bir değer şu çeşitlerden biri" anlamı taşır. D6'nın (gerçek
isimli hata tipleri, `sonuç<BlkAygit, BaslatHatasi>`) ön-koşuludur.

## Sözdizimi

```kemgu
çeşit Renk { Kirmizi, Yesil, Mavi }
```

- Üst düzey tanım (`işlev`/`yapı` gibi). `dışa çeşit ...` ile dışa aktarılır.
- Varyantlar virgül veya yenisatır ile ayrılır; sondaki virgül serbest.
- Erişim/inşa: `Renk::Kirmizi` (`::` ayracı — `.` alan erişimiyle çakışmaz).

### Eşleş (pattern matching)

```kemgu
eşleş r {
    Renk::Kirmizi => { ... }
    Renk::Yesil   => { ... }
    Renk::Mavi    => { ... }
}
```

### `sonuç` ile (D6)

`çeşit`, `sonuç`/`seçimlik`'in tip parametresi olarak akar — iç içe:

```kemgu
çeşit BaslatHatasi { ModernDegil, BlokDegil }

işlev blk_baslat(...) -> sonuç<bos, BaslatHatasi> {
    ...
    ver hata(BaslatHatasi::ModernDegil);
    ver tamam(bos);
}

eşleş blk_baslat(...) {
    tamam(v)                        => { ... }
    hata(BaslatHatasi::ModernDegil) => { ... }
    hata(BaslatHatasi::BlokDegil)   => { ... }
}
```

## Exhaustiveness (Maranget)

`eşleş` bir kapalı tip (`çeşit` / `seçimlik` / `sonuç`) üzerindeyse, **tüm
varyantlar kapsanmalıdır**; eksikse **M001** derleme hatası:

```
hata[M001]: esles exhaustive degil — eksik varyant(lar): [Yesil, Mavi]
```

- Wildcard `_` veya bir bağlama-deseni (`x => ...`) kapsamayı tamamlar.
- `sonuç<_, çeşit>` için: `tamam` + tüm `hata(çeşit::v)` varyantları (ya da
  `hata(_)`/`hata(e)` catch-all) gerekir (bir-seviye nesting).
- **Açık tipler** (tamsayı, `mantıksal` vb.) denetlenmez — mevcut `eşleş`'ler
  (driver durum makineleri dahil) kırılmaz.

## Codegen (LLVM IR)

- `çeşit Ad` → **discriminant**: ≤256 varyant → `i8`, değilse `i16`. Payload
  yok (v1) → C2.5'in `{i8 tag, payload}` makinesinin payloadsuz hâli.
- Discriminant değerleri **bildirim sırası** (0, 1, 2, ... — Rust'ın implicit
  discriminant kuralı gibi).
- `Ad::Varyant` → `add iN 0, <idx>` (tag sabiti).
- `eşleş Ad::V` → `icmp eq iN <scrut>, <idx>` + dallanma.
- `hata(çeşit::v)` → `tag==hata AND disc==idx` (iki `icmp` + `and i1`).
## `bos` / `boş` birim tipi

- `boş` (mevcut keyword) **birim tip + birim değer**'dir: `-> boş`,
  `sonuç<boş, E>`, `tamam(boş)` hepsi çalışır.
- `bos` (ASCII) **yalnız tip pozisyonunda** kabul edilir (`sonuç<bos, E>`) —
  birim tip alias'ı. Keyword DEĞİLDİR (aksi halde `değişken bos: ...` gibi
  mevcut `bos` adlı tanımlayıcıları kırardı). Birim **değer** için `boş`
  kullanılır.
- Codegen: birim payload `void` → `i8` dummy (C2.5).

## v1 Sınırları (→ v2)

- **Payload'lu varyant** (`Varyant(tip)`): v1'de **P354** ile reddedilir;
  keyword forward-compatible (v2'de geriye-uyumlu eklenecek).
- **Generic / özyineleme** (`çeşit Agac<T> { ... }`): **P353** ile reddedilir.
- **İç içe/derin pattern exhaustiveness**: yalnız `sonuç<_, çeşit>` bir-seviye
  destekli; daha derin nesting v2.

## Kaynak / test

Parser: `parse_cesit_tanimi`, `parse_desen` (`Cesit::Varyant`).
Typechecker: `pre_populate_cesit`, `DUGUM_YOL`, `esles_exhaustive_kontrol`.
Codegen (`llvm.c`): `cesit_kayit`, `cesit_disc_ir`, `cesit_varyant_indeksi`,
`ast_tip_to_ir`, `DUGUM_YOL`, `DUGUM_ESLES`.
Testler: `test/snapshots/cesit_temel.kem`, `cesit_sonuc.kem`.
