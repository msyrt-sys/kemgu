# `satıriçi_asm` — Inline Assembly (C5 v1)

`satıriçi_asm`, KEMGU içinden ham makine komutu yazmayı sağlar. Faz 2
bare-metal dalgasının ilk adımıdır: saf-KEMGU-OS için `msr`/`mrs`, `wfi`,
port I/O gibi ayrıcalıklı erişim bu kapıdan geçer. Yalnız `güvensiz` blok
içinde geçerlidir (G002) ve **deyim-formudur** (değer döndüren ifade-formu
v2'ye ertelendi; tuple YOK).

## Sözdizimi

```kemgu
güvensiz {
    satıriçi_asm {
        mimari: x86_64                  // arch-tag (ZORUNLU — AS001)
        şablon: r#"mov $2, $0
add $$2, $0
mov $$100, $1"#                         // asm şablonu (ZORUNLU; ham string önerilir)
        çıktı("=r", &s)                 // 0+ — her çıktı bir &değişken'e yazar
        çıktı("=r", &b)
        girdi("r", g)                   // 0+ — KEMGU ifadesi operand
        bozulan("~{cc}")                // 0+ — clobber listesi
        çevrim: 24                      // opsiyonel; gerçekzamanlı'da ZORUNLU (RT007)
    }
}
```

- Clause'lar ayraçsız sıralanır (her biri bilinen bir adla başlar);
  `mimari` ve `şablon` zorunludur (P266/P267).
- Şablon LLVM inline-asm şablon sözdizimidir: `$0`, `$1`, ... operandlar
  (çıktılar önce, sonra girdiler), `$$` literal dolar. Çok satırlı şablon
  için `r#"..."#` ham string kullanın.
- **Kısıt değerleri ham LLVM/GCC stringidir** (`=r`, `r`, `m`, `{al}`,
  `~{cc}`, `~{memory}` ...). Türkçe DNA *yüzey sözcüklerinde* korunur
  (`çıktı`/`girdi`/`bozulan`); kısıt harfi sayı literali gibi teknik bir
  sabittir, Türkçeleştirilmez (onaylanan tasarım kararı A).
- Lowering: `call <tip> asm sideeffect "şablon", "kısıtlar"(girdiler)`.
  `sideeffect` HER ZAMAN konur (DCE asm'i silemez); "saf" işareti v2.

## Çıktı bağlama (GCC-tarzı lvalue)

Çıktılar mevcut değişkenlere `&değişken` referansı üzerinden yazılır:

- **0 çıktı** → saf yan-etki deyimi (`call void asm ...`).
- **1 çıktı** → asm dönüşü doğrudan değişkene store edilir.
- **Çok çıktı** → LLVM agregat dönüş (`{T0, T1, ...}`) + `extractvalue`;
  KEMGU yüzeyinde tuple İCAT EDİLMEZ.

## Tip kuralları

| Kural | Kod | Açıklama |
|-------|-----|----------|
| güvensiz-gate | `G002` | `satıriçi_asm` yalnız `güvensiz` blok içinde. |
| arch-tag | `AS001` | `mimari:` etiketi hedef mimariyle uyuşmalı. Hedef şu an sabit `x86_64` (`llvm.h: KEMGU_HEDEF_MIMARI`); hedefe-duyarlı triple **C8'in işi**. arm64-tagli asm bugün hem `--check` hem `--llvm` yolunda reddedilir — **bozuk IR asla üretilmez**. |
| operand tipleri | `AS002` | Yalnız kopyalanabilir primitif: `tamN`, `dtamN`, `mantıksal`, `karakter`, ham `*T`. Kesirli, metin, yapı, dizi, referans v1'de YOK. |
| lineer kara kutu (C.1) | `AS002` | `tekkez<T>`/`yetki<R>` asm'e DOĞRUDAN geçemez; çıktı lineer OLAMAZ. Asm lineer-nötr: lineer yükümlülük ne tüketir ne üretir. Lineer kaynağın içine erişim gerekiyorsa KEMGU seviyesinde ham adres (`*T`) çıkarılır, asm'e **adres** geçer; yükümlülük KEMGU'da izlenmeye devam eder. |
| WCET (C.3) | `RT007` | `gerçekzamanlı` bağlamda asm, açık `çevrim:` anotasyonu olmadan REDDEDİLİR; anotasyon varsa WCET toplamına eklenir. Realtime-dışı bağlamda opsiyonel. *Sessizce 0 sayılmaz.* |

## Örnekler

Değer-üreten (tek çıktı):

```kemgu
değişken x: tam32 = 0;
güvensiz {
    satıriçi_asm {
        mimari: x86_64
        şablon: r#"mov $$42, $0"#
        çıktı("=r", &x)
    }
}
```

Saf yan-etki (çıktısız, gerçekzamanlı bağlamda):

```kemgu
güvensiz {
    satıriçi_asm {
        mimari: x86_64
        şablon: r#"pause"#
        çevrim: 1
    }
}
```

ARM64 (C8 hedefe-duyarlı triple gelene kadar AS001 ile reddedilir —
kasıtlı; yanlış hedefe sessizce bozuk kod üretmek yerine derleme hatası):

```kemgu
güvensiz {
    satıriçi_asm {
        mimari: arm64
        şablon: r#"mrs $0, CNTPCT_EL0"#
        çıktı("=r", &sayac)
    }
}
```

## Sınırlamalar (v1)

- Deyim-formu yalnız; ifade-formu sugar'ı (`değişken x = satıriçi_asm ...`) v2.
- `asm goto` / `callbr` yok (dallanan asm desteklenmez).
- `inteldialect` / `alignstack` bayrakları yok.
- Kesirli (`float`/`double`) operand yok.
- Hedefe-duyarlı triple yok → yalnız `x86_64` asm derlenebilir (C8 bekliyor).
- ARM64 fonksiyonel round-trip testleri (mrs/msr/wfi) C8'e ertelendi.

## ⚠️ BORÇ NOTU — Capability baypası

> **`satıriçi_asm` capability sistemini geçici olarak baypas ediyor.**
> v1'de ayrıcalıklı asm yalnız `güvensiz` ile gate'lenir; bir
> `yetki<R>` token'ı İSTENMEZ. Bu, "yetki olmadan `msr` yazılabilir"
> demektir — bare-metal güvenlik anlatısında bilinçli, geçici bir boşluk.
> Ayrıcalıklı-asm yetki-gate'i (örn. `yetki<Donanim>` ödünç parametresi,
> MMIO deseni gibi) **ayrı, odaklanmış bir pass'te tasarlanacak**;
> kaynak türü adı/zorunluluğu Mehmet'in onayına bağlı (ADIM 0 raporu
> C.2, karar: ERTELENDİ).
