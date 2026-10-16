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

dizi_tipi       = "Dizi" "<" tip ">" ;

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

---

## 6. Sonradan Eklenen Dil Yüzeyi (D-492'de senkronlandı)

> **⚠ Bu bölüm 2026-08-27'de eklendi.** Belgenin gövdesi 2026-05-09'dan beri
> dokunulmamıştı; o tarihten sonra dile eşzamanlılık, lineer tipler, yetki ve
> SIMD girdi. Ölçüldü: **41 anahtar kelimenin 16'sı gramerde YOKTU (%39).**
> Dili tarif etmeyen bir gramer, olmayandan tehlikelidir — ona göre uygulama
> yazan yanlış yazar. `calistir_anahtar_kelime_kapisi` bu sürüklenmeyi sabitler.
>
> **Aşağıdaki üretimler KORPUSTAN ÖLÇÜLDÜ, icat edilmedi** (`test/cg_korpus/`,
> `stdlib/`). Ölçülemeyenler bölümün sonunda AYRI ve açıkça işaretli.

### 6.1 Tip kurucuları (hepsi generic, `<>` ile)

```
tekkez_tip      = "tekkez" "<" tip ">" ;              (* Linear Types V1 *)
görev_tip       = "görev" "<" tip ">" ;               (* Concurrency V1 *)
kanal_tip       = "kanal" "<" tip ">" ;
yetki_tip       = "yetki" "<" kaynak_adı ">" ;        (* Capability V1 *)
sabitsüre_tip   = "sabitsüre" "<" tip ">" ;           (* sabit-süre disiplini *)
vektör_tip      = "vektör" "<" tip "," tam_literal ">" ;   (* SIMD V1 *)

kaynak_adı      = "Dosya" | "Soket" | "Bellek" | "Donanim"
                | "OTP_Anahtar" | "MMIO" ;
```
`kaynak_adı` **kapalı bir kümedir** ve dilin kendi sabit listesindendir
(`yetki_olustur` kaynak_tipi 1–6). Yeni ad eklemek dil yüzeyi değişikliğidir.

`gönderen<T>` / `alan<T>` kanal **yön uçlarıdır** (D-303) ve anahtar kelime
DEĞİLDİR — tip pozisyonunda kullanıcı-generic tipi olarak çözülürler, böylece
`alan` serbest bir tanımlayıcı olarak kalır.

### 6.2 Yapıcılar ve desenler

```
seçimlik_yapıcı = "değer" "(" ifade ")" | "hiç" ;
sonuç_yapıcı    = "tamam" "(" ifade ")" | "hata" "(" ifade ")" ;
```
Bunlar **bağlamsaldır**: ancak beklenen tip bilindiğinde çözülürler
(annotasyonlu bağlama · `ver` · çağrı argümanı · atama — D-465).
Desen konumunda aynı biçimler bağlama yapar (`tamam(v) =>`).

### 6.3 Lineer tüketim

```
kullan_ifadesi  = "kullan" "(" ifade ")" ;   (* YALNIZ tekkez<T> — L007 *)
imha_ifadesi    = "imha" "(" ifade ")" ;     (* her lineer değeri alır *)
```
**⚠ `kullan` bağlam-duyarlıdır:** üst düzeyde `kullan a::b;` bir *import*,
ifade konumunda `kullan(t)` bir *lineer tüketim*tir. Aynı kelime, iki dilbilgisi.

### 6.4 Yetki

```
yetki_olustur   = "yetki_olustur" "(" ifade "," ifade ")" ;  (* kaynak_tipi, izin *)
geri_al_ifadesi = "geri_al" "(" ifade ")" ;                  (* TÜKETİR — CP005 *)
```
`yetki<R>` **lineerdir**: işlev sonunda tüketilmezse CP005. `mmio_*` ve
`bölge_al` ödünç alır, `geri_al` tüketir.

### 6.5 Eşzamanlılık yerleşikleri

```
görev_başlat    = "görev_başlat" "(" lambda ")" ;   (* sonuç<görev<T>, metin> *)
görev_birleştir = "görev_birleştir" "(" ifade ")" ;
kanal_oluştur   = "kanal_oluştur" "(" ifade ")" ;   (* kapasite; T bağlamdan *)
kanal_gönder    = "kanal_gönder" "(" ifade "," ifade ")" ;
kanal_al        = "kanal_al" "(" ifade ")" ;
dondur          = "dondur" "(" ifade ")" ;          (* V1: identity *)
```
`görev_başlat` **`görev<T>` değil `sonuç<görev<T>, metin>` döner** (D-301):
spawn başarısızlığı panik değil DEĞERdir (çökmezlik). Bu yüzden her çağrı bir
`eşleş` ister.

### 6.6 İşlev niteleyicisi

```
işlev_tanımı    = [ "gerçekzamanlı" ] [ "genel" ] [ "çıplak" ]
                  "işlev" tanımlayıcı ... ;
```
Ölçüldü: `gerçekzamanlı işlev pid_hesapla(...)` (Realtime Spec V1).

### 6.7 ⚠ ÖLÇÜLEMEYENLER — sözdizimi UYDURULMADI

`kendin` · `delege` · `bölge` anahtar kelimedirler (lexer tablosunda) ama
**korpusta gerçek kullanımları BULUNAMADI** — yalnız anahtar kelime
listelerinde geçiyorlar. Onlar için üretim yazmak *icat* olurdu; bu bölüm
bilerek boş bırakıldı. Kullanımları ortaya çıkınca ölçülüp eklenmelidir.
