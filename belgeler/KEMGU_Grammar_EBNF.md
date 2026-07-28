# KEMGU EBNF Grammar Tanımı

Bu belge KEMGU dilinin tam formal grammar tanımını içerir.
Parser implementasyonu bu grammar'a dayanır.

## Notasyon Kuralları
```
=     tanım
|     alternatif
[ ]   seçimlik (0 veya 1)
{ }   tekrar (0 veya daha fazla)
( )   gruplama
;     kural sonu
"x"   terminal (anahtar kelime/sembol)
X     non-terminal
```

---

## 1. Üst Düzey Yapılar

```ebnf
program         = { üst_öğe } ;

üst_öğe         = modül_tanımı
                | kullan_bildirimi
                | işlev_tanımı
                | yapı_tanımı
                | özellik_tanımı
                | uygula_bloğu
                | sabit_tanımı
                | dışa_bildirimi ;

modül_tanımı    = "modül" tanımlayıcı "{" { üst_öğe } "}" ;

kullan_bildirimi = "kullan" modül_yolu ";" ;

modül_yolu      = tanımlayıcı { "::" tanımlayıcı } ;

dışa_bildirimi  = "dışa" ( işlev_tanımı | yapı_tanımı | özellik_tanımı | sabit_tanımı ) ;
```

## 2. Tanımlar

```ebnf
işlev_tanımı    = "işlev" tanımlayıcı "(" [ parametre_listesi ] ")" [ "->" tip ] blok ;

parametre_listesi = parametre { "," parametre } ;

parametre       = tanımlayıcı ":" tip ;

yapı_tanımı     = "yapı" tanımlayıcı [ "<" tip_param_listesi ">" ] "{" { alan_tanımı } "}" ;

alan_tanımı     = tanımlayıcı ":" tip ";" ;

tip_param_listesi = tanımlayıcı { "," tanımlayıcı } ;

özellik_tanımı  = "özellik" tanımlayıcı [ "<" tip_param_listesi ">" ] "{" { özellik_öğe } "}" ;

özellik_öğe     = işlev_imzası ";"
                | işlev_tanımı ;

işlev_imzası    = "işlev" tanımlayıcı "(" [ parametre_listesi ] ")" [ "->" tip ] ;

uygula_bloğu    = "uygula" [ "<" tip_param_listesi ">" ] tip
                  [ ":" özellik_yolu { "+" özellik_yolu } ] "{" { işlev_tanımı } "}" ;

özellik_yolu    = modül_yolu ;

sabit_tanımı    = "sabit" tanımlayıcı ":" tip "=" ifade ";" ;
```

## 3. Deyimler (Statements)

```ebnf
blok            = "{" { deyim } "}" ;

deyim           = değişken_tanımı
                | atama_deyimi
                | ifade_deyimi
                | ver_deyimi
                | eğer_deyimi
                | iken_deyimi
                | için_deyimi
                | eşleş_deyimi
                | güvensiz_bloğu ;

değişken_tanımı = "değişken" tanımlayıcı [ ":" tip ] "=" ifade ";" ;

atama_deyimi    = atama_hedefi "=" ifade ";" ;

atama_hedefi    = tanımlayıcı { "." tanımlayıcı | "[" ifade "]" } ;

ifade_deyimi    = ifade ";" ;

ver_deyimi      = "ver" [ ifade ] ";" ;

eğer_deyimi     = "eğer" ifade blok { "değilse" "eğer" ifade blok } [ "değilse" blok ] ;

iken_deyimi     = "iken" ifade blok ;

için_deyimi     = "için" tanımlayıcı ":" ifade blok ;

eşleş_deyimi    = "eşleş" ifade "{" { eşleş_kolu } "}" ;

eşleş_kolu      = desen "=>" ( blok | ifade ";" ) ;

desen           = literal
                | tanımlayıcı
                | tanımlayıcı "(" [ desen_listesi ] ")"
                | "_" ;

desen_listesi   = desen { "," desen } ;

güvensiz_bloğu  = "güvensiz" [ "[" güvensiz_açıklama "]" ] blok ;

güvensiz_açıklama = tanımlayıcı ":" metin_literali ;
```

## 4. İfadeler (Expressions) — Pratt Parser

İfadeler Pratt parser ile parse edilir. Öncelik tablosu (düşükten yükseğe):

```
Seviye  Operatörler           Birleşme       Açıklama
──────  ─────────────────     ──────────     ──────────────────
  1     veya                  Sol            Mantıksal VEYA
  2     ve                    Sol            Mantıksal VE
  3     == !=                 Sol            Eşitlik
  4     < > <= >=             Sol            Karşılaştırma
  5     + -                   Sol            Toplama/çıkarma
  6     * / %                 Sol            Çarpma/bölme/mod
  7     değil - & *           Önek (sağ)     Tekli operatörler
  8     . [] ()               Sol            Erişim/indeks/çağrı
```

```ebnf
ifade           = tekli_ifade { ikili_op tekli_ifade } ;
                  (* Pratt parser öncelik tablosuna göre çözülür *)

tekli_ifade     = önek_op tekli_ifade
                | sonek_ifade ;

önek_op         = "değil" | "-" | "&" | "&" "değişken" | "*" ;

sonek_ifade     = birincil_ifade { sonek_op } ;

sonek_op        = "." tanımlayıcı
                | "[" ifade "]"
                | "(" [ argüman_listesi ] ")"
                | "::" tanımlayıcı ;

argüman_listesi = ifade { "," ifade } ;

ikili_op        = "+" | "-" | "*" | "/" | "%"
                | "==" | "!=" | "<" | ">" | "<=" | ">="
                | "ve" | "veya" ;

birincil_ifade  = tam_literal
                | kesirli_literal
                | metin_literali
                | karakter_literali
                | "doğru" | "yanlış"
                | "boş"
                | tanımlayıcı
                | "(" ifade ")"
                | lambda_ifade
                | yapı_oluşturma
                | dizi_oluşturma
                | blok_ifade ;

lambda_ifade    = "|" [ parametre_listesi ] "|" ( blok | ifade ) ;

yapı_oluşturma  = tanımlayıcı "{" [ alan_atama { "," alan_atama } ] "}" ;

alan_atama      = tanımlayıcı ":" ifade ;

dizi_oluşturma  = "[" [ ifade { "," ifade } ] "]" ;

blok_ifade      = blok ;
```

## 5. Tip Sistemi

```ebnf
tip             = basit_tip
                | referans_tip
                | pointer_tip
                | dizi_tipi
                | seçimlik_tip
                | sonuç_tip
                | işlev_tipi
                | kullanıcı_tip ;

basit_tip       = "tam8" | "tam16" | "tam32" | "tam64"
                | "dtam8" | "dtam16" | "dtam32" | "dtam64"
                | "kesirli32" | "kesirli64"
                | "mantıksal"
                | "karakter"
                | "metin"
                | "boş" ;

referans_tip    = "&" tip
                | "&" "değişken" tip ;

pointer_tip     = "*" tip ;

(* DZ Spec V1: N istege bagli statik uzunluk (tamsayi literali; sabit ifade
   V1'de YOK — vektör<T,N> ile ayni kisit). Yoklugu "bilinmiyor" demektir. *)
dizi_tipi       = "Dizi" "<" tip [ "," tamsayi_literal ] ">" ;

seçimlik_tip    = "seçimlik" "<" tip ">" ;

sonuç_tip       = "sonuç" "<" tip "," tip ">" ;

işlev_tipi      = "işlev" "(" [ tip_listesi ] ")" "->" tip ;

tip_listesi     = tip { "," tip } ;

kullanıcı_tip   = modül_yolu [ "<" tip_listesi ">" ] ;
```

---

## Ambiguity Notları

### 1. Tanımlayıcı + `{` Belirsizliği
`x { ... }` ifadesi:
- Yapı oluşturma mı? `Nokta { x: 1, y: 2 }`
- Blok ifade mi? `x; { deyim; }`

**Çözüm:** `{` öncesindeki tanımlayıcı büyük harfle başlıyorsa yapı oluşturma,
değilse blok ifade. Alternatif: context-dependent parsing (parser duruma göre karar verir).
İlk implementasyonda: tanımlayıcı sonrası `{` geldiğinde, `{` içinde `isim:` kalıbı
varsa yapı oluşturma olarak parse et.

### 2. `<` Belirsizliği
`a < b` ifadesi:
- Karşılaştırma mı? `a < b`
- Generic tip argümanı mı? `a<b>`

**Çözüm:** Tip kontekstinde (tip beklenen yerlerde) `<` her zaman generic.
İfade kontekstinde `<` her zaman karşılaştırma. Parser context'e göre karar verir.

### 3. `*` Belirsizliği
`*x` ifadesi:
- Çarpma operatörü mü? (ikili)
- Pointer dereference mi? (tekli)

**Çözüm:** Önek pozisyonunda (ifade başı, operatör sonrası) → dereference.
İkili pozisyonunda (ifade sonrası) → çarpma. Pratt parser bu ayrımı doğal olarak yapar.

### 4. `-` Belirsizliği
`-x` ifadesi:
- Negatif mi? (tekli)
- Çıkarma mı? (ikili)

**Çözüm:** `*` ile aynı — Pratt parser prefix/infix ayrımı ile çözer.
