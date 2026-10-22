# Tur 23 — turbofish (`f::<T1, T2>(args)`) PARSER — C + üç self-host parser

Yeni sözdizimi: ifade konumunda `ad::<Tip, ...>(argümanlar)` = AÇIK generic
argümanlı işlev çağrısı. Bugün HER İKİ derleyici bunu reddediyor (C `P123`,
self-host `P000` — ölçüldü). Bu tur YALNIZ parser + AST dump'ı ekler; tip
kontrol ve codegen SONRAKİ turlarda. Aşağıdaki dosyalar DIŞINDA hiçbir dosyaya
DOKUNMA. Her değişiklik bir ```duzenle bloğu; ESKI metin dosyada AYNEN ve TEK
kez geçmeli (aşağıda verdiğim ESKI metinleri harfiyen kopyala, boşluklar dâhil).

## Belirsizlik kuralı (tasarım — kodda yorum olarak yaz)
1. `f<T>(x)` ifade konumunda generic çağrı DEĞİLDİR; eskisi gibi `(f < T) > (x)`
   karşılaştırmasıdır (hiçbir mevcut davranış değişmez).
2. Generic argüman ifade konumunda YALNIZ `::<` ile açılır. `::` sonrası `<`
   gelirse bu bir tip argüman listesidir (yol devamı tanımlayıcı ister, `<`
   olamaz → çakışma yok).
3. Liste kapanınca HEMEN `(` gelmeli (turbofish yalnız bir çağrıya bağlanır).
   Gelmezse `P125` hatası. `f::<A>::<B>()` → ikinci `::`da P125.
4. CAGRI düğümünün konumu, turbofish'siz çağrıyla AYNI: `(` simgesinin konumu.

## C (3 dosya)

### 1. `src/ast.h` — cagri yapısına iki alan
ESKI (tam olarak):
```
        struct {
            Dugum *hedef;          /* cagrilan ifade */
            Dugum **argumanlar;
            int sayi;
        } cagri;
```
YENI:
```
        struct {
            Dugum *hedef;          /* cagrilan ifade */
            Dugum **argumanlar;
            int sayi;
            /* [D1 turbofish] `f::<T1,T2>(..)` ACIK generic argumanlari
             * (tip dugumleri). Turbofish yoksa NULL / 0 — dugum_olustur
             * sifirlar, yani tum eski cagri yollari degismez. */
            Dugum **tip_arg;
            int tip_arg_sayi;
        } cagri;
```

### 2. `src/ifade.c` — `parse_sonek_op` içindeki `TOK_CIFT_IKI_NOKTA` dalı
ESKI (tam olarak):
```
        case TOK_CIFT_IKI_NOKTA: {
            parser_ilerle(p);
            Token sag_tok = parser_bekle(p, TOK_TANIMLAYICI, "P123",
```
YENI: aynı üç satır, ama `parser_ilerle(p);` satırından HEMEN sonra şu blok girer
(blok bitince orijinal `Token sag_tok = ...` satırı ve devamı aynen kalır):
```
            /* [D1 turbofish] `::<` -> ACIK generic argumanli cagri.
             * Belirsizlik kurali: ifade konumunda `f<T>(x)` KARSILASTIRMADIR
             * (degismedi); generic argumanlar YALNIZ `::<` ile acilir. `::`
             * sonrasi yol devami tanimlayici ister, `<` olamaz -> cakisma
             * yok. Liste kapaninca HEMEN `(` gelmeli; turbofish yalniz bir
             * cagriya baglanir (`f::<A>::<B>()` -> P125). CAGRI dugumu `(`
             * konumunda kurulur (turbofish'siz cagriyla ayni). */
            if (parser_eslesir(p, TOK_KUCUK)) {
                Liste targs;
                liste_baslat(&targs);
                (void)parse_generic_listesi(p, "P318", "P319", 1, &targs);
                if (!parser_eslesir(p, TOK_SOL_PAREN)) {
                    parser_hata(p, parser_simdiki(p), "P125",
                        "turbofish '::<..>' sonrasi '(' bekleniyor "
                        "(yalniz islev cagrisi)", NULL);
                    return sol;
                }
                Dugum *c = parse_sonek_op(p, sol);   /* '(' -> DUGUM_CAGRI */
                if (c && c->tip == DUGUM_CAGRI) {
                    c->veri.cagri.tip_arg = liste_array_yap(&targs, p->arena);
                    c->veri.cagri.tip_arg_sayi = targs.sayi;
                }
                return c;
            }
```
`parse_generic_listesi` bu dosyada `parse_sonek_op`tan SONRA tanımlı, `static`
ve İLERİ BİLDİRİMİ YOK (ölçüldü). Bu yüzden AYRI bir duzenle bloğu ZORUNLU:
ESKI olarak `/* === Sonek operatorler (cagri, erisim, indeks, yol) === */`
satırını kullan, YENI = aşağıdaki ileri bildirim + boş satır + o satır:
```
static Dugum *parse_generic_listesi(Parser *p, const char *kod_ac,
                                     const char *kod_kapa, int min_say,
                                     Liste *out_args);
```

### 3. `src/ast_yazdir.c` — iki CAGRI dalı
(a) Düz dump (`ast_duz_yaz`, `--ast`) — ESKI (tam olarak):
```
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
```
YENI:
```
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            /* [D1 turbofish] sira: hedef, ACIK tip argumanlari, argumanlar.
             * Tip dugumleri (TIP_*) ifade olamaz -> dump belirsiz degil. */
            for (int i = 0; i < d->veri.cagri.tip_arg_sayi; i++)
                ast_duz_yaz(d->veri.cagri.tip_arg[i], c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
```
(b) Girintili dump (`ast_yazdir_indent`) — ESKI (tam olarak):
```
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
```
YENI:
```
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            if (d->veri.cagri.tip_arg_sayi > 0) {   /* [D1 turbofish] */
                indent_yaz(c, derinlik + 1);
                fprintf(c, "tip_argumanlari (%d):\n", d->veri.cagri.tip_arg_sayi);
                cocuk_listesi_yaz(c, d->veri.cagri.tip_arg,
                                  d->veri.cagri.tip_arg_sayi, derinlik + 1);
            }
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
```

## Self-host (3 dosya: `selfhost/parser.kem`, `selfhost/checker.kem`, `selfhost/codegen.kem`)

KEMGU dizgi literallerinde KAÇIŞ YOK. Türkçe karakterleri olduğu gibi yaz.
Self-host'ta CAGRI çocukları `[hedef, arg1, arg2, ...]` — bu invaryanta
checker/codegen her yerde dayanır, DEĞİŞTİRME. Tip argümanları PARALEL TABLOYA
(yan-kanal) gider; dump onları C ile aynı sırada basar.

Her üç dosyada AYNI dört değişiklik:

### A. `yapı Ayr` — dört alan
- `selfhost/parser.kem`: ESKI (tam olarak)
  `    sk_pending: tam32;   // >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)`
  satırının hemen ardına (satır KALIR) şunu ekle.
- `selfhost/checker.kem` ve `selfhost/codegen.kem`: yapının SON alanı
  `    th_sut: Dizi<tam32>;` satırıdır ve hemen ardından `}` gelir. ESKI olarak
  şu İKİ satırı kullan (dosyada tek kez geçer):
  ```
      th_sat: Dizi<tam32>;
      th_sut: Dizi<tam32>;
  ```
  (checker.kem'de iki satırın başında 4 boşluk var; codegen.kem'de de 4 boşluk.)
  YENI = aynı iki satır + ardından eklenen alanlar.

Eklenecek alanlar:
```
    // [D1 turbofish] `f::<T1,T2>(..)` yan-kanalı: CAGRI düğümü → AÇIK tip
    // argüman düğümleri. tf_cagri[k] CAGRI idx; tipler tf_tip[tf_bas[k] ..
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
```

### B. `ayr_olustur` — dört alanı başlat
- parser.kem: ESKI `        sk_pending: 0` (bu dosyada ayr_olustur içinde,
  ardından satır sonu ve `    };`). Dikkat: parser.kem'de `sk_pending: 0`
  SON alandır (sonunda virgül yok). ESKI olarak şu İKİ satırı kullan:
  ```
          sk_pending: 0
      };
  ```
  YENI:
  ```
          sk_pending: 0,
          tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
      };
  ```
- checker.kem ve codegen.kem: ayr_olustur'un SON satırı (her iki dosyada aynı
  metin, dosya başına tek kez):
  `        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0`
  YENI: `        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0,`
  ve ardından yeni satır
  `        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []`

### C. `parse_sonek` — `CIFT_IKI_NOKTA` dalı + iki yardımcı
ESKI (üç dosyada da harfiyen bu, her dosyada tek kez):
```
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
```
YENI:
```
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        // [D1 turbofish] `::<` → AÇIK generic argümanlı çağrı (C ifade.c
        // aynası; belirsizlik kuralı orada). Liste kapanınca HEMEN `(`
        // gelmeli (P125). CAGRI `(` konumunda kurulur (özyineli parse_sonek).
        eğer sim_mi(p, "KUCUK") {
            değişken targs: Dizi<tam32> = parse_generic_args(p);
            eğer sim_mi(p, "SOL_PAREN") == yanlış {
                HATA_SATIRI
                ver sol;
            }
            değişken tc: tam32 = parse_sonek(p, sol);
            tf_kaydet(p, tc, targs);
            ver tc;
        }
        değişken sag: metin = sim_lex(p);
```
`HATA_SATIRI` yerine:
- parser.kem: `p.hata_say = p.hata_say + 1;`
- checker.kem ve codegen.kem: `parse_hata_kaydet(p, "P125");`

İki yardımcıyı her dosyada `// ---- Öncelik tabloları` ... değil, `parse_sonek`
işlevinin HEMEN ÖNCESİNE ekle. ESKI olarak her dosyadaki
`işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {` satırını kullan
(tek kez geçer), YENI = aşağıdaki iki işlev + o satırın kendisi:
```
// [D1 turbofish] CAGRI düğümünün AÇIK tip argümanlarını yan-kanala kaydet.
işlev tf_kaydet(p: &değişken Ayr, cagri: tam32, tipler: Dizi<tam32>) -> tam32 {
    dizi_ekle(p.tf_cagri, cagri);
    dizi_ekle(p.tf_bas, dizi_boyut(p.tf_tip));
    dizi_ekle(p.tf_say, dizi_boyut(tipler));
    değişken i: tam32 = 0;
    iken i < dizi_boyut(tipler) { dizi_ekle(p.tf_tip, dizi_al(tipler, i)); i = i + 1; }
    ver 0;
}
// CAGRI idx → tf tablosundaki kayıt indeksi (-1 = turbofish yok).
işlev tf_bul(p: &değişken Ayr, cagri: tam32) -> tam32 {
    değişken i: tam32 = 0;
    iken i < dizi_boyut(p.tf_cagri) {
        eğer dizi_al(p.tf_cagri, i) == cagri { ver i; }
        i = i + 1;
    }
    ver 0 - 1;
}
```
(Önce `parse_sonek` satırının kendisinden önceki yorum satırını bozmamak için
ESKI'yi yalnız `işlev parse_sonek(...)` satırı tut; yardımcılar yorumdan sonra,
işlevden önce girer — sorun değil.)

### D. `duz_yaz` — tip argümanlarını C sırasıyla bas
ESKI (üç dosyada da harfiyen bu, tek kez):
```
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
```
YENI:
```
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken tfk: tam32 = tf_bul(p, idx);   // [D1 turbofish] -1 = yok
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        // C ast_duz_yaz sırası: hedef (çocuk 0), AÇIK tip argümanları, argümanlar.
        eğer (i == 0) ve (tfk >= 0) {
            değişken ti: tam32 = 0;
            iken ti < dizi_al(p.tf_say, tfk) {
                duz_yaz(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, tfk) + ti), derinlik + 1);
                ti = ti + 1;
            }
        }
        i = i + 1;
    }
```

Toplam blok sayısı: C 5 + self-host 3×5 = 20.
