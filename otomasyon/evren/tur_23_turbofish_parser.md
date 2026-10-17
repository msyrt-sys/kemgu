# Tur 23 — turbofish_parser — `deepseek-v4.1-flash`

- Tarih: 2026-10-17 18:40:16 · süre 207.0 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 21147, "total_tokens": 34346, "completion_tokens": 13199, "prompt_tokens_details": {"cached_tokens": 10624, "created_cache_tokens": 10496}, "completion_tokens_details": {"reasoning_tokens": 6630}}`
- Değişiklik blokları: 20 — duzenle:src/ast.h, duzenle:src/ifade.c, duzenle:src/ifade.c, duzenle:src/ast_yazdir.c, duzenle:src/ast_yazdir.c, duzenle:selfhost/parser.kem, duzenle:selfhost/parser.kem, duzenle:selfhost/parser.kem, duzenle:selfhost/parser.kem, duzenle:selfhost/parser.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem
- İstem: [tur_23_turbofish_parser_istem.md](tur_23_turbofish_parser_istem.md)

## Yanıt

```duzenle:src/ast.h
<<<<<<< ESKI
        struct {
            Dugum *hedef;          /* cagrilan ifade */
            Dugum **argumanlar;
            int sayi;
        } cagri;
=======
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
>>>>>>> YENI
```

```duzenle:src/ifade.c
<<<<<<< ESKI
/* === Sonek operatorler (cagri, erisim, indeks, yol) === */
=======
static Dugum *parse_generic_listesi(Parser *p, const char *kod_ac,
                                     const char *kod_kapa, int min_say,
                                     Liste *out_args);

/* === Sonek operatorler (cagri, erisim, indeks, yol) === */
>>>>>>> YENI
```

```duzenle:src/ifade.c
<<<<<<< ESKI
        case TOK_CIFT_IKI_NOKTA: {
            parser_ilerle(p);
            Token sag_tok = parser_bekle(p, TOK_TANIMLAYICI, "P123",
=======
        case TOK_CIFT_IKI_NOKTA: {
            parser_ilerle(p);
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
            Token sag_tok = parser_bekle(p, TOK_TANIMLAYICI, "P123",
>>>>>>> YENI
```

```duzenle:src/ast_yazdir.c
<<<<<<< ESKI
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
=======
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            /* [D1 turbofish] sira: hedef, ACIK tip argumanlari, argumanlar.
             * Tip dugumleri (TIP_*) ifade olamaz -> dump belirsiz degil. */
            for (int i = 0; i < d->veri.cagri.tip_arg_sayi; i++)
                ast_duz_yaz(d->veri.cagri.tip_arg[i], c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
>>>>>>> YENI
```

```duzenle:src/ast_yazdir.c
<<<<<<< ESKI
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
=======
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            if (d->veri.cagri.tip_arg_sayi > 0) {   /* [D1 turbofish] */
                indent_yaz(c, derinlik + 1);
                fprintf(c, "tip_argumanlari (%d):\n", d->veri.cagri.tip_arg_sayi);
                cocuk_listesi_yaz(c, d->veri.cagri.tip_arg,
                                  d->veri.cagri.tip_arg_sayi, derinlik + 1);
            }
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
>>>>>>> YENI
```

```duzenle:selfhost/parser.kem
<<<<<<< ESKI
    sk_pending: tam32;   // >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)
=======
    sk_pending: tam32;   // >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)
    // [D1 turbofish] `f::<T1,T2>(..)` yan-kanalı: CAGRI düğümü → AÇIK tip
    // argüman düğümleri. tf_cagri[k] CAGRI idx; tipler tf_tip[tf_bas[k] ..
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
>>>>>>> YENI
```

```duzenle:selfhost/parser.kem
<<<<<<< ESKI
        sk_pending: 0
    };
=======
        sk_pending: 0,
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
    };
>>>>>>> YENI
```

```duzenle:selfhost/parser.kem
<<<<<<< ESKI
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
=======
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
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
>>>>>>> YENI
```

```duzenle:selfhost/parser.kem
<<<<<<< ESKI
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
=======
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        // [D1 turbofish] `::<` → AÇIK generic argümanlı çağrı (C ifade.c
        // aynası; belirsizlik kuralı orada). Liste kapanınca HEMEN `(`
        // gelmeli (P125). CAGRI `(` konumunda kurulur (özyineli parse_sonek).
        eğer sim_mi(p, "KUCUK") {
            değişken targs: Dizi<tam32> = parse_generic_args(p);
            eğer sim_mi(p, "SOL_PAREN") == yanlış {
                p.hata_say = p.hata_say + 1;
                ver sol;
            }
            değişken tc: tam32 = parse_sonek(p, sol);
            tf_kaydet(p, tc, targs);
            ver tc;
        }
        değişken sag: metin = sim_lex(p);
>>>>>>> YENI
```

```duzenle:selfhost/parser.kem
<<<<<<< ESKI
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
=======
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
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
=======
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
    // [D1 turbofish] `f::<T1,T2>(..)` yan-kanalı: CAGRI düğümü → AÇIK tip
    // argüman düğümleri. tf_cagri[k] CAGRI idx; tipler tf_tip[tf_bas[k] ..
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
=======
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0,
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
=======
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
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
=======
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        // [D1 turbofish] `::<` → AÇIK generic argümanlı çağrı (C ifade.c
        // aynası; belirsizlik kuralı orada). Liste kapanınca HEMEN `(`
        // gelmeli (P125). CAGRI `(` konumunda kurulur (özyineli parse_sonek).
        eğer sim_mi(p, "KUCUK") {
            değişken targs: Dizi<tam32> = parse_generic_args(p);
            eğer sim_mi(p, "SOL_PAREN") == yanlış {
                parse_hata_kaydet(p, "P125");
                ver sol;
            }
            değişken tc: tam32 = parse_sonek(p, sol);
            tf_kaydet(p, tc, targs);
            ver tc;
        }
        değişken sag: metin = sim_lex(p);
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
=======
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
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
=======
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
    // [D1 turbofish] `f::<T1,T2>(..)` yan-kanalı: CAGRI düğümü → AÇIK tip
    // argüman düğümleri. tf_cagri[k] CAGRI idx; tipler tf_tip[tf_bas[k] ..
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
=======
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0,
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
=======
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
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
=======
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        // [D1 turbofish] `::<` → AÇIK generic argümanlı çağrı (C ifade.c
        // aynası; belirsizlik kuralı orada). Liste kapanınca HEMEN `(`
        // gelmeli (P125). CAGRI `(` konumunda kurulur (özyineli parse_sonek).
        eğer sim_mi(p, "KUCUK") {
            değişken targs: Dizi<tam32> = parse_generic_args(p);
            eğer sim_mi(p, "SOL_PAREN") == yanlış {
                parse_hata_kaydet(p, "P125");
                ver sol;
            }
            değişken tc: tam32 = parse_sonek(p, sol);
            tf_kaydet(p, tc, targs);
            ver tc;
        }
        değişken sag: metin = sim_lex(p);
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
=======
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
>>>>>>> YENI
```

Var olan dosyalarda değişiklik — 20 blok:

**C (5 blok):**
1. `src/ast.h` — `cagri` yapısına `tip_arg`/`tip_arg_sayi` eklendi.
2. `src/ifade.c` — `parse_generic_listesi` ileri bildirimi eklendi (`parse_sonek_op`tan sonra, gövde tanımlı, statik; ileri bildirim zorunlu).
3. `src/ifade.c` — `TOK_CIFT_IKI_NOKTA` dalına `::<` turbofish yolu eklendi; liste kapanınca `(` yoksa `P125`; CAGRI düğümü özyineli `parse_sonek_op`la `(` konumunda kurulur.
4. `src/ast_yazdir.c` — `ast_duz_yaz` CAGRI dalında hedef → tip argümanları → argümanlar sırası.
5. `src/ast_yazdir.c` — `ast_yazdir_indent` CAGRI dalında `tip_argumanlari` alt-listesi.

**Self-host (3×5 = 15 blok):**
- Her dosyada A: `Ayr` yapısına dört `tf_*` alanı ve açıklama yorumu.
- B: `ayr_olustur` içinde dört alanın başlatılması (parser.kem `sk_pending: 0` sonrası; checker/codegen `neg_lit: 0` sonrası).
- C-yardımcı: `tf_kaydet` ve `tf_bul` işlevleri `parse_sonek`'in hemen öncesine.
- C-dal: `CIFT_IKI_NOKTA` içinde `KUCUK` görülürse `parse_generic_args`, `(` yoksa hata (parser.kem `p.hata_say = p.hata_say + 1;`, checker/codegen `parse_hata_kaydet(p, "P125")`), sonra özyineli `parse_sonek` + `tf_kaydet`.
- D: `duz_yaz` içinde `tf_bul` ile tip argümanlarını C sırasıyla basma (yalnız `i == 0`, hedeften hemen sonra).

Çocuk invaryantı `[hedef, arg1, ...]` korundu; tip argümanları yan-kanal tablolarında.

VARSAYIMLAR
- `parse_generic_args`, `parse_generic_listesi`, `sim_mi`, `dizi_ekle`, `dizi_al`, `dizi_boyut`, `parse_hata_kaydet` self-host'ta ve `parse_generic_listesi` C'de zaten mevcut; imzaları verilen desenle uyumlu (`Dizi<tam32>` döner / `Liste*` doldurur).
- `src/ifade.c`'de `TOK_KUCUK` = `<`, `TOK_SOL_PAREN` = `(` token sabitleri mevcut; `parse_sonek_op` özyineli çağrılabilir (statik).
- `duz_yaz`/`ast_duz_yaz` çocuk listesinde `çocuk[0] = hedef` invaryantı; `i == 0` kontrolü hedeften sonraya tip argümanı sokmak için yeterli.
- Her dosyada verilen ESKI metinler tam bir kez geçiyor (kesitlerden doğrulandı); parser.kem'de `sk_pending: 0` son alan olduğu için virgül eklendi.

## Düşünme (reasoning)

Let me carefully construct the 20 blocks based on the task.

Let me count: C has 3 files with 5 blocks total:
- src/ast.h: 1 block
- src/ifade.c: 2 blocks (one for TOK_CIFT_IKI_NOKTA, one for forward declaration)
- src/ast_yazdir.c: 2 blocks (a and b)

Self-host: 3 files × 5 blocks = 15
Total: 5 + 15 = 20.

Let me write each carefully.

### 1. src/ast.h

ESKI:
```
        struct {
            Dugum *hedef;          /* cagrilan ifade */
            Dugum **argumanlar;
            int sayi;
        } cagri;
```

### 2. src/ifade.c — TOK_CIFT_IKI_NOKTA

ESKI:
```
        case TOK_CIFT_IKI_NOKTA: {
            parser_ilerle(p);
            Token sag_tok = parser_bekle(p, TOK_TANIMLAYICI, "P123",
```

### 2b. src/ifade.c — forward declaration

ESKI: `/* === Sonek operatorler (cagri, erisim, indeks, yol) === */`

YENI = forward decl + empty line + that line.

### 3a. src/ast_yazdir.c — duz

ESKI:
```
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
```

### 3b. src/ast_yazdir.c — indent

ESKI:
```
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
```

### Self-host parser.kem — A. yapı Ayr

ESKI: `    sk_pending: tam32;   // >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)`
YENI = that line + new fields.

### Self-host parser.kem — B. ayr_olustur

ESKI:
```
        sk_pending: 0
    };
```

### Self-host parser.kem — C. parse_sonek

ESKI:
```
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
```

Wait, but the CIFT_IKI_NOKTA branch appears also in the section listing. Let me check that this exact 3 lines appear only once in parser.kem. The section shows it appears at lines 1028-1070. And also the offsets. Yes, once.

Hmm wait — but the task says "ESKI (üç dosyada da harfiyen bu, her dosyada tek kez)". But the context shows the same snippet appearing in parser.kem, checker.kem, codegen.kem at the parse_sonek function. In parser.kem the snippet is at line 1028-1070. Let me check - actually parser.kem's version uses `bekle(p, "SAG_PAREN")` while checker/codegen use `bekle_k(p, "SAG_PAREN", "P122")`. But the CIFT_IKI_NOKTA part itself is the same. And the parser.kem section shows:
```
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
        bekle(p, "TANIMLAYICI");
        ver dugum1(p, "YOL", sag, s, u, sol);
    }
```
Good, matches.

But wait — does the parse_sonek snippet appear ONLY once in parser.kem? The parser.kem section is 1028-1070, so yes, once.

But actually — hmm, I notice the parser.kem section might contain parse_sonek twice? No, sections are cut pieces. Let me trust it's once.

### Self-host parser.kem — C (helper) — parse_sonek function line

ESKI: `işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {`

But wait — this appears in section at line 1028 AND also line 1321 in checker.kem, 1518 in codegen.kem. Each file has its own copy. But in parser.kem, does `işlev parse_sonek(...)` appear more than once? Probably once. Yes.

Hmm, but actually I need to be careful: the section says "selfhost/parser.kem — KESİT (satır 1028-1070)". And it starts right at `işlev parse_sonek(...)`. Wait no, let me re-check:

```
## selfhost/parser.kem — KESİT (satır 1028-1070; düzenleme hedefi: `selfhost/parser.kem`, blok yolu `selfhost/parser.kem` olmalı)
```
}

// Sonek operatör (. [] () :: olarak). Pozisyon = sonek token'ı.
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
```

Hmm interesting — the section starts with `}` and blank line and comment, then `işlev parse_sonek(...)`. So context shows `// Sonek operatör...` comment above. The task says: "İki yardımcıyı her dosyada ... `parse_sonek` işlevinin HEMEN ÖNCESİNE ekle. ESKI olarak her dosyadaki `işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {` satırını kullan (tek kez geçer), YENI = aşağıdaki iki işlev + o satırın kendisi."

And: "(Önce `parse_sonek` satırının kendisinden önceki yorum satırını bozmamak için ESKI'yi yalnız `işlev parse_sonek(...)` satırı tut; yardımcılar yorumdan sonra, işlevden önce girer — sorun değil.)"

OK so I'll use just the `işlev parse_sonek(...)` line as ESKI. Good.

But wait — does this exact string `işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {` appear exactly once in parser.kem? Yes, the function definition.

### Self-host parser.kem — D. duz_yaz

ESKI:
```
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
```

Wait — but parser.kem's duz_yaz section shows:
```
işlev duz_yaz(p: &değişken Ayr, idx: tam32, derinlik: tam32) -> tam32 {
    yaz_tam(derinlik);  yb(9);
    yaz_str(dizi_al(p.a_ad, idx));  yb(9);
    yaz_kacis(dizi_al(p.a_deg, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sat, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sut, idx));  yb(10);
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    ...
```
Good.

Now the other two files (checker.kem and codegen.kem):

### checker.kem — A. yapı Ayr

ESKI:
```
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
```

Wait, but the checker.kem section shows:
```
    th_kod: Dizi<metin>;  // tip-hata kodları (traversal sırası)
    t043_node: Dizi<tam32>;  // D-630: T043 raporlanmış literal düğümleri (tekilleştirme)
    tasma_node: Dizi<tam32>;  // [D-632] 2^64+ (doymuş) TAM düğümleri — a_deg doymuş değeri taşır
    neg_lit: tam32;          // D-630: literal tekli eksinin operandı mı (alt sınır +1)
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
}
```

The task says use ESKI with these two lines:
```
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
```

Hmm, but in parser.kem section, `th_sat` and `th_sut` don't appear. In checker.kem, the section shows these with 4-space indentation. The task says "checker.kem'de iki satırın başında 4 boşluk var; codegen.kem'de de 4 boşluk."

OK so ESKI for both files:
```
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
```

But wait — does this appear exactly once in checker.kem? Let me check the checker.kem sections. The Ayr struct section (176-184) shows these two lines. And also... hmm, the ayr_olustur section uses different lines. So yes, once.

But hold on — the task says for checker.kem and codegen.kem the yapı Ayr last field is `th_sut`, but the codegen.kem section shows:
```
    t043_node: Dizi<tam32>;  // D-630: T043 raporlanmış literal düğümleri (tekilleştirme)
    tasma_node: Dizi<tam32>;  // [D-632] 2^64+ (doymuş) TAM düğümleri — a_deg doymuş değeri taşır
    neg_lit: tam32;          // D-630: literal tekli eksinin operandı mı (alt sınır +1)
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
}
```
Good, matches.

Wait, but checker.kem section header shows satır 176-184 but the lines above t043_node are different. Let me re-read:

checker.kem section:
```
    lin_basla: tam32;        // aktif işlevin lineer bağlama diliminin başı
    th_kod: Dizi<metin>;  // tip-hata kodları (traversal sırası)
    t043_node: Dizi<tam32>;  // D-630: T043 raporlanmış literal düğümleri (tekilleştirme)
    tasma_node: Dizi<tam32>;  // [D-632] 2^64+ (doymuş) TAM düğümleri — a_deg doymuş değeri taşır
    neg_lit: tam32;          // D-630: literal tekli eksinin operandı mı (alt sınır +1)
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
}
```

Notice `th_kod: Dizi<metin>;  // tip-hata...` has extra space before comment (2 spaces vs codegen which has one comment aligned). But the ESKI I need is just `    th_sat...` and `    th_sut...`. OK.

### checker.kem — B. ayr_olustur

ESKI: `        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0`

But wait — this line has different context. The task says "checker.kem ve codegen.kem: ayr_olustur'un SON satırı (her iki dosyada aynı metin, dosya başına tek kez)".

Looking at checker.kem section:
```
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
    };
}
```
And codegen.kem section:
```
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
    };
}
```
Good. Same text.

Hmm, but wait. The checker.kem ayr_olustur might be split differently. Looking at the section 2032-2066:

```
        fn_ad: [], fn_donus: [], fn_psay: [], ...
        yapi_ad: [], ...
        giris_dizin: "", kullan_gorulen: [],
        ly_ad: [], ...
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
    };
}
```

Hmm wait, checker.kem's version doesn't have `th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0` — actually it does! Let me re-read:

```
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
    };
}
```

Yes it's there.

But wait, does this `th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0` line appear elsewhere in checker.kem? The section for checker.kem at 2032-2066 shows it once. Probably appears once.

Hmm actually wait, in checker.kem - is there another instance? Probably not.

OK.

### checker.kem — C. parse_sonek

ESKI:
```
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
```
YENI: same but with turbofish block inserted between `ilerle(p);` and `değişken sag: metin = sim_lex(p);`.

Note: HATA_SATIRI for checker.kem = `parse_hata_kaydet(p, "P125");`.

### checker.kem — C helper. parse_sonek function line

ESKI: `işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {`

YENI = two helpers + the line.

### checker.kem — D. duz_yaz

ESKI:
```
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
```

Same for codegen.kem.

But codegen.kem's duz_yaz uses `dump_ad(dizi_al(p.a_ad, idx))` — different, but the ESKI section is the same (cb/cs/i loop). Good.

Now let me write all blocks.

Wait — one concern. In the parser.kem B block, the ESKI is `sk_pending: 0` followed by `    };`. But we need to make sure `sk_pending: 0\n    };` appears uniquely. The task says yes.

Actually, I realize I need to write out the exact text. Let me build each block carefully.

Let me reconsider the parser.kem A block. The task says:
- parser.kem: ESKI (tam olarak) `    sk_pending: tam32;   // >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)` satırının hemen ardına (satır KALIR) şunu ekle.

So ESKI = that line, YENI = that line + four new fields.

New fields:
```
    // [D1 turbofish] `f::<T1,T2>(..)` yan-kanalı: CAGRI düğümü → AÇIK tip
    // argüman düğümleri. tf_cagri[k] CAGRI idx; tipler tf_tip[tf_bas[k] ..
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
```

OK.

Now checker.kem and codegen.kem A block: ESKI is the two lines, YENI = two lines + new fields.

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
=======
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
    // [D1 turbofish] `f::<T1,T2>(..)` yan-kanalı: CAGRI düğümü → AÇIK tip
    // argüman düğümleri. tf_cagri[k] CAGRI idx; tipler tf_tip[tf_bas[k] ..
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
>>>>>>> YENI
```

Now checker.kem B block — ayr_olustur:

ESKI: `        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0`
YENI: 
```
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0,
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
```

Wait — but the ESKI line ends without trailing comma? The task says: `YENI: 'th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0,' and ardından yeni satır 'tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []'`.

Hmm, note it ends without trailing comma on the new last line. Good.

Now checker.kem C block:

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
=======
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        // [D1 turbofish] `::<` → AÇIK generic argümanlı çağrı (C ifade.c
        // aynası; belirsizlik kuralı orada). Liste kapanınca HEMEN `(`
        // gelmeli (P125). CAGRI `(` konumunda kurulur (özyineli parse_sonek).
        eğer sim_mi(p, "KUCUK") {
            değişken targs: Dizi<tam32> = parse_generic_args(p);
            eğer sim_mi(p, "SOL_PAREN") == yanlış {
                parse_hata_kaydet(p, "P125");
                ver sol;
            }
            değişken tc: tam32 = parse_sonek(p, sol);
            tf_kaydet(p, tc, targs);
            ver tc;
        }
        değişken sag: metin = sim_lex(p);
>>>>>>> YENI
```

Wait — this is repeated in 3 files. But the ESKI text `    eğer metin_esit(t, "CIFT_IKI_NOKTA") {` might appear multiple times? The task says "üç dosyada da harfiyen bu, her dosyada tek kez". OK.

Hmm — actually wait, does the exact three-line ESKI appear once in parser.kem? Let me check parser.kem's 1028-1070 section:

```
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
        bekle(p, "TANIMLAYICI");
        ver dugum1(p, "YOL", sag, s, u, sol);
    }
```

Yes. Once in parser.kem.

Actually — hmm, but the very same function parse_sonek appears in parser.kem once. But could `eğer metin_esit(t, "CIFT_IKI_NOKTA") {` appear elsewhere? Ambiguity concern: `eğer metin_esit(t, "CIFT_IKI_NOKTA") {` might be unique but the 3-line combo is safer. I'll use the 3-line ESKI as instructed.

Now parser.kem C block with HATA_SATIRI = `p.hata_say = p.hata_say + 1;`.

Now parser.kem helper block: ESKI is `işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {`. YENI = two helper functions + that line.

Wait, but ambiguity — this line appears in parser.kem, checker.kem, codegen.kem separately, but within a file once. Good.

Now D block. For each file:
```
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
```
YENI with turbofish insertion.

OK let me now write all 20 blocks carefully.

Actually, let me double-check the checker.kem duz_yaz section shows the exact text. Yes, section 2032-2066:
```
işlev duz_yaz(p: &değişken Ayr, idx: tam32, derinlik: tam32) -> tam32 {
    yaz_tam(derinlik);  yb(9);
    yaz_str(dizi_al(p.a_ad, idx));  yb(9);
    yaz_kacis(dizi_al(p.a_deg, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sat, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sut, idx));  yb(10);
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
    ver 0;
}
```
Yes, matches.

But wait, checker.kem might have this exact code block elsewhere? The task says every file has duz_yaz once. OK.

Alright, let me write everything now.

Also, careful: the `<` and `>` in KEMGU code. In the parser, `sim_mi(p, "KUCUK")` refers to `<` token. Fine.

Now write final answer with all blocks.

One more concern about checker.kem's ayr_olustur ESKI. The task says the ESKI is:
`        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0`

But does this exact line appear uniquely? Checker.kem's ayr_olustur section shows once. Could there be another? Probably not — ayr_olustur is one function. But wait — could `th_kod: [], th_sat: [], ...` appear in parser.kem too? Let me check: parser.kem's ayr_olustur ends with `hata_say: 0, yapi_izni: 1, sk_pending: 0`. No `th_kod`. Good.

OK. Writing.

Actually hold on. Let me re-read the checker.kem ayr_olustur section carefully:

```
        fn_ad: [], fn_donus: [], fn_psay: [], fn_pbase: [], fn_ptip: [], fn_rlin: [], fn_ptn: [], fn_plin: [], fn_pdizi: [],
        yapi_ad: [], yapi_abase: [], yapi_acount: [], alan_ad: [], alan_tip: [], alan_tn: [], t14_muaf: [], m4_node: [], m4_tn: [],
        giris_dizin: "", kullan_gorulen: [],
        ly_ad: [], lyf_yapi: [], lyf_alan: [], lin_ad: [], lin_sat: [], lin_sut: [], lin_tuk: [], dy_node: [], dy_yapi: [], dyf_node: [], dyf_alan: [], lin_maske: [], lin_fnk: [], lin_tek: [], lin_yet: [], gtas_ad: [], kt_ad: [], snap_tuk: [], snap_maske: [], snap_top: 0, imha_bag: 0, guv_bag: 0, yer_son: 0, tuk_kapali: 0, cast_sessiz: 0, cast_hata_var: 0, cip_bag: 0, lin_basla: 0,
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
    };
}
```

Yes.

And codegen.kem ayr_olustur section (14636-14641):
```
        fn_donus: [], fn_psay: [], fn_pbase: [], fn_ptip: [], fn_ptn: [], fn_plin: [], fn_pdizi: [],
        giris_dizin: "", kullan_gorulen: [],
        ly_ad: [], lyf_yapi: [], lyf_alan: [], lin_ad: [], lin_sat: [], lin_sut: [], lin_tuk: [], dy_node: [], dy_yapi: [], dyf_node: [], dyf_alan: [], lin_maske: [], lin_fnk: [], lin_tek: [], lin_yet: [], gtas_ad: [], kt_ad: [], snap_tuk: [], snap_maske: [], snap_top: 0, imha_bag: 0, guv_bag: 0, yer_son: 0, tuk_kapali: 0, cast_sessiz: 0, cast_hata_var: 0, cip_bag: 0, lin_basla: 0,
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
    };
}
```

Good, same ESKI line.

Now let me carefully write.

Also for parser.kem A block, need to note that the ESKI line ends in `// >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)` — with parentheses.

Let me write it exactly matching the section:

```
    sk_pending: tam32;   // >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)
```

Three spaces before `//`.

OK, writing final.

Also, I want to make sure about the checker.kem ast_yazdir (girintili) block — this is for checker.kem, not applicable. The girintili dump is only C ast_yazdir.c. OK.

Now writing.

Final answer with 20 blocks.
