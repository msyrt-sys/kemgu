# Tur 24 — turbofish_checker — `deepseek-v4.1-flash`

- Tarih: 2026-10-17 18:48:16 · süre 158.4 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 23715, "total_tokens": 33198, "completion_tokens": 9483, "prompt_tokens_details": {"cached_tokens": 10624, "created_cache_tokens": 13056}, "completion_tokens_details": {"reasoning_tokens": 244}}`
- Değişiklik blokları: 25 — duzenle:src/tip_kontrol.c, duzenle:src/tip_kontrol.c, duzenle:src/tip_kontrol.c, duzenle:src/tip_kontrol.c, duzenle:src/tip_kontrol.c, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/checker.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem, duzenle:selfhost/codegen.kem
- İstem: [tur_24_turbofish_checker_istem.md](tur_24_turbofish_checker_istem.md)

## Yanıt

```duzenle:src/tip_kontrol.c
<<<<<<< ESKI
TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *d) {
=======
/* [D1 turbofish] `f::<T1,T2>(..)` hedefinin generic islev AST'si.
 * TANIMLAYICI: sozcuksel arama (tip_belirle ile ayni golgeleme kurali —
 * ayni adli yerel degisken islevi golgeler). YOL: resolver baglamasi
 * (tip_belirle(hedef) SONRASI dolar). Generic olmayan / islev olmayan /
 * bulunamayan -> NULL. */
static const Dugum *turbofish_islev_bul(TipKontrol *tk, const Dugum *h) {
    const Sembol *s = NULL;
    if (!h) return NULL;
    if (h->tip == DUGUM_TANIMLAYICI) {
        s = sembol_bul(tk->scope, h->veri.tanimlayici.metin,
                       h->veri.tanimlayici.uzunluk);
    } else if (h->tip == DUGUM_YOL) {
        s = h->cozum_sembol;
    }
    if (!s || s->kategori != SEMBOL_ISLEV || !s->ast_dugumu ||
        s->ast_dugumu->tip != DUGUM_ISLEV) return NULL;
    return s->ast_dugumu;
}

/* [D1 turbofish] On-denetim — CAGRI islemenin EN BASINDA (her iki giris
 * noktasinda: tip_belirle ve tip_belirle_beklenen). Acik generic arguman
 * sayisi, hedef islevin generic parametre sayisina esit olmali; aksi T044
 * ve 1 doner (cagiran t_hata ile erken doner, argumanlar gezilmez).
 * Yan etkisiz basari yolu: iki giris noktasinda ard arda kosmasi tanıyı
 * IKILEMEZ. YOL hedefi burada atlanir (sembol henuz cozulmedi) ve
 * baglama asamasinda denetlenir. Yerlesik (dizi_olustur, tekkez_olustur,
 * kanal_olustur ...) hedef -> T044: yerlesikler acik generic arguman ALMAZ;
 * sessizce yok saymak "kabul et, anlamini degistir" olurdu. */
static int turbofish_on_kontrol(TipKontrol *tk, const Dugum *d) {
    int n = d->veri.cagri.tip_arg_sayi;
    if (n <= 0) return 0;
    const Dugum *h = d->veri.cagri.hedef;
    if (h && h->tip == DUGUM_YOL) return 0;
    const Dugum *f = turbofish_islev_bul(tk, h);
    int gsay = f ? f->veri.islev.tip_param_sayi : 0;
    if (n != gsay) {
        tip_hata(tk, d, "T044",
                 "turbofish tip argumani sayisi islevin generic parametre "
                 "sayisiyla uyumsuz");
        return 1;
    }
    return 0;
}

TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *d) {
>>>>>>> YENI
```

```duzenle:src/tip_kontrol.c
<<<<<<< ESKI
        case DUGUM_CAGRI: {
            /* [D-517] HAM BELLEK YERLESIKLERI `güvensiz` ISTER.
=======
        case DUGUM_CAGRI: {
            if (turbofish_on_kontrol(tk, d)) return t_hata(tk);  /* [D1 turbofish] */
            /* [D-517] HAM BELLEK YERLESIKLERI `güvensiz` ISTER.
>>>>>>> YENI
```

```duzenle:src/tip_kontrol.c
<<<<<<< ESKI
        case DUGUM_CAGRI: {
            /* C3: çeşit varyant yapıcısı (Cesit::V(args)) — beklenen-bağlam
=======
        case DUGUM_CAGRI: {
            if (turbofish_on_kontrol(tk, d)) return t_hata(tk);  /* [D1 turbofish] */
            /* C3: çeşit varyant yapıcısı (Cesit::V(args)) — beklenen-bağlam
>>>>>>> YENI
```

```duzenle:src/tip_kontrol.c
<<<<<<< ESKI
            GenBaglamalar gb;
            gb.sayi = 0;
            /* Once unify arg tipleri ile (substitue olmadan) — daha sonra
=======
            GenBaglamalar gb;
            gb.sayi = 0;
            /* [D1 turbofish] ACIK generic argumanlar cikarsamayi EZER: once
             * baglanir; gen_bagla "ilk gorulen kazanir" oldugu icin pas 1'deki
             * gen_unify onlari DEGISTIREMEZ. Pas 2'de parametre tipi bu
             * baglamayla ikame edilip BEKLENEN tip olur (literal o tipte
             * cikarsanir, uyusmayan arguman T001), donus tipi de ikame edilir.
             * YOL hedefi on-denetimden gecmisti (sembol o an bilinmiyordu):
             * sayi burada denetlenir. */
            const Dugum *tf_islev = NULL;
            if (d->veri.cagri.tip_arg_sayi > 0) {
                tf_islev = turbofish_islev_bul(tk, d->veri.cagri.hedef);
                if (!tf_islev || tf_islev->veri.islev.tip_param_sayi !=
                                 d->veri.cagri.tip_arg_sayi) {
                    tip_hata(tk, d, "T044",
                             "turbofish tip argumani sayisi islevin generic "
                             "parametre sayisiyla uyumsuz");
                    return t_hata(tk);
                }
                for (int ti = 0; ti < d->veri.cagri.tip_arg_sayi; ti++) {
                    const char *tp = tf_islev->veri.islev.tip_paramlar[ti];
                    TipBilgisi *acik = ast_tip_to_bilgi(tk,
                        d->veri.cagri.tip_arg[ti]);
                    gen_bagla(&gb, tp, (int)strlen(tp), acik);
                }
            }
            /* Once unify arg tipleri ile (substitue olmadan) — daha sonra
>>>>>>> YENI
```

```duzenle:src/tip_kontrol.c
<<<<<<< ESKI
                if (param_tip->kategori == TIP_GENERIC_PARAM) {
                    /* Concrete'i gb'de tutuyoruz; bind onceden yapildi */
                    continue;
                }
=======
                if (param_tip->kategori == TIP_GENERIC_PARAM &&
                    (!tf_islev || bek->kategori == TIP_HATA)) {
                    /* Concrete'i gb'de tutuyoruz; bind onceden yapildi.
                     * [D1 turbofish] ACIK baglamada bek arguman tipinden
                     * TURETILMEDI -> asagidaki T001 denetimi GEREKLI. */
                    continue;
                }
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
=======
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
    // [D1 turbofish] İŞLEV BAŞINA generic parametre tablosu (C islev.tip_paramlar
    // aynası): fg_ad[k] işlev adı, paramları gp_ad[fg_gb[k] .. +fg_gs[k]),
    // fg_dn[k] dönüş tip düğümü (-1 = yok). gp_ad (D-370) kapsam-duyarsız düz
    // listedir; SIRAYI yalnız bu tablo tutar.
    fg_ad: Dizi<metin>;
    fg_gb: Dizi<tam32>;
    fg_gs: Dizi<tam32>;
    fg_dn: Dizi<tam32>;
    // [D1 turbofish] tip_str İKAMESİ: ts_n > 0 iken TIP_BASIT adı ts_ad[i] ise
    // ts_tip[i] döner (mantıksal uzunluk ts_n; dizi küçülmez, yeniden yazılır).
    ts_ad: Dizi<metin>;
    ts_tip: Dizi<metin>;
    ts_n: tam32;
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
=======
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: [],
        fg_ad: [], fg_gb: [], fg_gs: [], fg_dn: [], ts_ad: [], ts_tip: [], ts_n: 0
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    atla_tip_paramlar(p);
    bekle_k(p, "SOL_PAREN", "P015");   // [D-650] C parser.c:343 imza "("
=======
    değişken fg_gp0: tam32 = dizi_boyut(p.gp_ad);   // [D1 turbofish]
    atla_tip_paramlar(p);
    değişken fg_gp1: tam32 = dizi_boyut(p.gp_ad);
    bekle_k(p, "SOL_PAREN", "P015");   // [D-650] C parser.c:343 imza "("
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    eğer sim_mi(p, "OK") { ilerle(p); dizi_ekle(kids, parse_tip(p)); }
=======
    değişken fg_dn: tam32 = 0 - 1;   // [D1 turbofish] dönüş tip düğümü
    eğer sim_mi(p, "OK") { ilerle(p); fg_dn = parse_tip(p); dizi_ekle(kids, fg_dn); }
    dizi_ekle(p.fg_ad, ad); dizi_ekle(p.fg_gb, fg_gp0);
    dizi_ekle(p.fg_gs, fg_gp1 - fg_gp0); dizi_ekle(p.fg_dn, fg_dn);
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
işlev tip_str(p: &değişken Ayr, idx: tam32) -> metin {
=======
// [D1 turbofish] tip_str ikamesi: generic param adı → açık tip dizgisi.
işlev ts_coz(p: &değişken Ayr, ad: metin) -> metin {
    değişken i: tam32 = 0;
    iken i < p.ts_n {
        eğer metin_esit(dizi_al(p.ts_ad, i), ad) { ver dizi_al(p.ts_tip, i); }
        i = i + 1;
    }
    ver ad;
}
// İşlev adı → fg kaydı (-1 = yok). İLK eşleşme (fn_* tablolarıyla aynı kural).
işlev fg_bul(p: &değişken Ayr, ad: metin) -> tam32 {
    değişken i: tam32 = 0;
    iken i < dizi_boyut(p.fg_ad) {
        eğer metin_esit(dizi_al(p.fg_ad, i), ad) { ver i; }
        i = i + 1;
    }
    ver 0 - 1;
}
// CAGRI → AÇIKÇA bağlanan generic işlevin fg kaydı. -1: turbofish yok, hedef
// TANIMLAYICI değil, yerel bağlama gölgeliyor, işlev yok ya da sayı uyuşmuyor.
işlev tf_fg(p: &değişken Ayr, cagri: tam32) -> tam32 {
    değişken k: tam32 = tf_bul(p, cagri);
    eğer k < 0 { ver 0 - 1; }
    değişken h: tam32 = dizi_al(p.cocuk, dizi_al(p.a_cb, cagri));
    eğer metin_esit(dizi_al(p.a_ad, h), "TANIMLAYICI") == yanlış { ver 0 - 1; }
    eğer yerel_baglama_mi(p, dizi_al(p.a_deg, h)) { ver 0 - 1; }
    değişken g: tam32 = fg_bul(p, dizi_al(p.a_deg, h));
    eğer g < 0 { ver 0 - 1; }
    eğer dizi_al(p.fg_gs, g) != dizi_al(p.tf_say, k) { ver 0 - 1; }
    ver g;
}
// node tipini, CAGRI'nın açık argümanlarıyla İKAME ederek dizgiye çevir.
// Bilinmeyen sonuç (iç içe generic, gövde-içi `T`) → "?" (denetim atlanır).
işlev tf_tip_coz(p: &değişken Ayr, cagri: tam32, g: tam32, node: tam32) -> metin {
    eğer node < 0 { ver "?"; }
    değişken k: tam32 = tf_bul(p, cagri);
    değişken n: tam32 = dizi_al(p.tf_say, k);
    // Önce açık tipleri İKAMESİZ çöz (p.ts_n == 0), sonra tabloyu kur.
    değişken tipler: Dizi<metin> = [];
    değişken j: tam32 = 0;
    iken j < n {
        dizi_ekle(tipler, tip_str(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)));
        j = j + 1;
    }
    j = 0;
    iken j < n {
        değişken gad: metin = dizi_al(p.gp_ad, dizi_al(p.fg_gb, g) + j);
        eğer j < dizi_boyut(p.ts_ad) {
            dizi_yaz(p.ts_ad, j, gad);
            dizi_yaz(p.ts_tip, j, dizi_al(tipler, j));
        } değilse {
            dizi_ekle(p.ts_ad, gad);
            dizi_ekle(p.ts_tip, dizi_al(tipler, j));
        }
        j = j + 1;
    }
    p.ts_n = n;
    değişken s: metin = tip_str(p, node);
    p.ts_n = 0;
    eğer bilinen_tip_mi(s) { ver s; }
    ver "?";
}
// [D1 turbofish] T044 ön-denetimi — C turbofish_on_kontrol aynası. CAGRI
// işlemenin EN BAŞINDA; doğru → T044 raporlandı, çağıran erken döner.
// YOL hedefi atlanır (V1: self-host YOL çağrılarını denetlemiyor).
işlev tf_on_kontrol(p: &değişken Ayr, idx: tam32, hedef: tam32) -> mantıksal {
    değişken k: tam32 = tf_bul(p, idx);
    eğer k < 0 { ver yanlış; }
    eğer metin_esit(dizi_al(p.a_ad, hedef), "YOL") { ver yanlış; }
    eğer tf_fg(p, idx) >= 0 { ver yanlış; }
    sekil_hata(p, idx, "T044");
    ver doğru;
}
işlev tip_str(p: &değişken Ayr, idx: tam32) -> metin {
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_BASIT") { ver dizi_al(p.a_deg, idx); }
=======
    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_BASIT") { ver ts_coz(p, dizi_al(p.a_deg, idx)); }
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
    eğer metin_esit(ad, "CAGRI") {
        değişken hb: tam32 = dizi_al(p.a_cb, idx);
        değişken hc: tam32 = dizi_al(p.a_cs, idx);
        değişken hedef: tam32 = dizi_al(p.cocuk, hb);
=======
    eğer metin_esit(ad, "CAGRI") {
        değişken hb: tam32 = dizi_al(p.a_cb, idx);
        değişken hc: tam32 = dizi_al(p.a_cs, idx);
        değişken hedef: tam32 = dizi_al(p.cocuk, hb);
        eğer tf_on_kontrol(p, idx, hedef) { ver 0; }   // [D1 turbofish] T044
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
                // Ön-geçişte tüketim KAPALI: C lineeri ikilemiyor (`al(tuket(t))` → OK).
                p.tuk_kapali = p.tuk_kapali + 1;
=======
                // [D1 turbofish] Açık tip düğümleri de denetlenir (C: bağlamada
                // ast_tip_to_bilgi → T011/DRF/...). C sırası: E013'ten SONRA, argüman
                // geçişlerinden ÖNCE.
                değişken tfg: tam32 = tf_fg(p, idx);
                eğer tfg >= 0 {
                    değişken tfk2: tam32 = tf_bul(p, idx);
                    değişken tj: tam32 = 0;
                    iken tj < dizi_al(p.tf_say, tfk2) {
                        kontrol_dugum(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, tfk2) + tj));
                        tj = tj + 1;
                    }
                }
                // Ön-geçişte tüketim KAPALI: C lineeri ikilemiyor (`al(tuket(t))` → OK).
                p.tuk_kapali = p.tuk_kapali + 1;
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
                    değişken pt: metin = fn_ptip_bul(p, fad, a - 1);
=======
                    değişken pt: metin = fn_ptip_bul(p, fad, a - 1);
                    // [D1 turbofish] açık bağlama: param tipi İKAME edilmiş tiptir
                    // (generic `T` param artık "?" değil → T001 denetlenir).
                    eğer tfg >= 0 { pt = tf_tip_coz(p, idx, tfg, fn_ptn_bul(p, fad, a - 1)); }
>>>>>>> YENI
```

```duzenle:selfhost/checker.kem
<<<<<<< ESKI
            ver fn_donus_bul(p, dizi_al(p.a_deg, h));   // "?" = kullanıcı işlevi değil
=======
            // [D1 turbofish] açık bağlamada dönüş tipi İKAME edilir (C gen_substitue).
            değişken tfg3: tam32 = tf_fg(p, idx);
            eğer tfg3 >= 0 {
                eğer dizi_al(p.fg_dn, tfg3) >= 0 { ver tf_tip_coz(p, idx, tfg3, dizi_al(p.fg_dn, tfg3)); }
            }
            ver fn_donus_bul(p, dizi_al(p.a_deg, h));   // "?" = kullanıcı işlevi değil
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
=======
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
    // [D1 turbofish] İŞLEV BAŞINA generic parametre tablosu (C islev.tip_paramlar
    // aynası): fg_ad[k] işlev adı, paramları gp_ad[fg_gb[k] .. +fg_gs[k]),
    // fg_dn[k] dönüş tip düğümü (-1 = yok). gp_ad (D-370) kapsam-duyarsız düz
    // listedir; SIRAYI yalnız bu tablo tutar.
    fg_ad: Dizi<metin>;
    fg_gb: Dizi<tam32>;
    fg_gs: Dizi<tam32>;
    fg_dn: Dizi<tam32>;
    // [D1 turbofish] tip_str İKAMESİ: ts_n > 0 iken TIP_BASIT adı ts_ad[i] ise
    // ts_tip[i] döner (mantıksal uzunluk ts_n; dizi küçülmez, yeniden yazılır).
    ts_ad: Dizi<metin>;
    ts_tip: Dizi<metin>;
    ts_n: tam32;
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
=======
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: [],
        fg_ad: [], fg_gb: [], fg_gs: [], fg_dn: [], ts_ad: [], ts_tip: [], ts_n: 0
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
    bekle(p, "TANIMLAYICI");
    atla_tip_paramlar(p);
    // D-401: İŞLEVİN generic param adlarını KAYDET. Öncesinde `atla_tip_paramlar`
=======
    bekle(p, "TANIMLAYICI");
    değişken fg_gp0: tam32 = dizi_boyut(p.gp_ad);   // [D1 turbofish]
    atla_tip_paramlar(p);
    değişken fg_gp1: tam32 = dizi_boyut(p.gp_ad);
    // D-401: İŞLEVİN generic param adlarını KAYDET. Öncesinde `atla_tip_paramlar`
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
    eğer sim_mi(p, "OK") { ilerle(p); dizi_ekle(kids, parse_tip(p)); }
=======
    değişken fg_dn: tam32 = 0 - 1;   // [D1 turbofish] dönüş tip düğümü
    eğer sim_mi(p, "OK") { ilerle(p); fg_dn = parse_tip(p); dizi_ekle(kids, fg_dn); }
    dizi_ekle(p.fg_ad, ad); dizi_ekle(p.fg_gb, fg_gp0);
    dizi_ekle(p.fg_gs, fg_gp1 - fg_gp0); dizi_ekle(p.fg_dn, fg_dn);
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
işlev tip_str(p: &değişken Ayr, idx: tam32) -> metin {
=======
// [D1 turbofish] tip_str ikamesi: generic param adı → açık tip dizgisi.
işlev ts_coz(p: &değişken Ayr, ad: metin) -> metin {
    değişken i: tam32 = 0;
    iken i < p.ts_n {
        eğer metin_esit(dizi_al(p.ts_ad, i), ad) { ver dizi_al(p.ts_tip, i); }
        i = i + 1;
    }
    ver ad;
}
// İşlev adı → fg kaydı (-1 = yok). İLK eşleşme (fn_* tablolarıyla aynı kural).
işlev fg_bul(p: &değişken Ayr, ad: metin) -> tam32 {
    değişken i: tam32 = 0;
    iken i < dizi_boyut(p.fg_ad) {
        eğer metin_esit(dizi_al(p.fg_ad, i), ad) { ver i; }
        i = i + 1;
    }
    ver 0 - 1;
}
// CAGRI → AÇIKÇA bağlanan generic işlevin fg kaydı. -1: turbofish yok, hedef
// TANIMLAYICI değil, yerel bağlama gölgeliyor, işlev yok ya da sayı uyuşmuyor.
işlev tf_fg(p: &değişken Ayr, cagri: tam32) -> tam32 {
    değişken k: tam32 = tf_bul(p, cagri);
    eğer k < 0 { ver 0 - 1; }
    değişken h: tam32 = dizi_al(p.cocuk, dizi_al(p.a_cb, cagri));
    eğer metin_esit(dizi_al(p.a_ad, h), "TANIMLAYICI") == yanlış { ver 0 - 1; }
    eğer yerel_baglama_mi(p, dizi_al(p.a_deg, h)) { ver 0 - 1; }
    değişken g: tam32 = fg_bul(p, dizi_al(p.a_deg, h));
    eğer g < 0 { ver 0 - 1; }
    eğer dizi_al(p.fg_gs, g) != dizi_al(p.tf_say, k) { ver 0 - 1; }
    ver g;
}
// node tipini, CAGRI'nın açık argümanlarıyla İKAME ederek dizgiye çevir.
// Bilinmeyen sonuç (iç içe generic, gövde-içi `T`) → "?" (denetim atlanır).
işlev tf_tip_coz(p: &değişken Ayr, cagri: tam32, g: tam32, node: tam32) -> metin {
    eğer node < 0 { ver "?"; }
    değişken k: tam32 = tf_bul(p, cagri);
    değişken n: tam32 = dizi_al(p.tf_say, k);
    // Önce açık tipleri İKAMESİZ çöz (p.ts_n == 0), sonra tabloyu kur.
    değişken tipler: Dizi<metin> = [];
    değişken j: tam32 = 0;
    iken j < n {
        dizi_ekle(tipler, tip_str(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)));
        j = j + 1;
    }
    j = 0;
    iken j < n {
        değişken gad: metin = dizi_al(p.gp_ad, dizi_al(p.fg_gb, g) + j);
        eğer j < dizi_boyut(p.ts_ad) {
            dizi_yaz(p.ts_ad, j, gad);
            dizi_yaz(p.ts_tip, j, dizi_al(tipler, j));
        } değilse {
            dizi_ekle(p.ts_ad, gad);
            dizi_ekle(p.ts_tip, dizi_al(tipler, j));
        }
        j = j + 1;
    }
    p.ts_n = n;
    değişken s: metin = tip_str(p, node);
    p.ts_n = 0;
    eğer bilinen_tip_mi(s) { ver s; }
    ver "?";
}
// [D1 turbofish] T044 ön-denetimi — C turbofish_on_kontrol aynası. CAGRI
// işlemenin EN BAŞINDA; doğru → T044 raporlandı, çağıran erken döner.
// YOL hedefi atlanır (V1: self-host YOL çağrılarını denetlemiyor).
işlev tf_on_kontrol(p: &değişken Ayr, idx: tam32, hedef: tam32) -> mantıksal {
    değişken k: tam32 = tf_bul(p, idx);
    eğer k < 0 { ver yanlış; }
    eğer metin_esit(dizi_al(p.a_ad, hedef), "YOL") { ver yanlış; }
    eğer tf_fg(p, idx) >= 0 { ver yanlış; }
    sekil_hata(p, idx, "T044");
    ver doğru;
}
işlev tip_str(p: &değişken Ayr, idx: tam32) -> metin {
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_BASIT") { ver dizi_al(p.a_deg, idx); }
=======
    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_BASIT") { ver ts_coz(p, dizi_al(p.a_deg, idx)); }
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
    eğer metin_esit(ad, "CAGRI") {
        değişken hb: tam32 = dizi_al(p.a_cb, idx);
        değişken hc: tam32 = dizi_al(p.a_cs, idx);
        değişken hedef: tam32 = dizi_al(p.cocuk, hb);
=======
    eğer metin_esit(ad, "CAGRI") {
        değişken hb: tam32 = dizi_al(p.a_cb, idx);
        değişken hc: tam32 = dizi_al(p.a_cs, idx);
        değişken hedef: tam32 = dizi_al(p.cocuk, hb);
        eğer tf_on_kontrol(p, idx, hedef) { ver 0; }   // [D1 turbofish] T044
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
                // Ön-geçişte tüketim KAPALI: C lineeri ikilemiyor (`al(tuket(t))` → OK).
                p.tuk_kapali = p.tuk_kapali + 1;
=======
                // [D1 turbofish] Açık tip düğümleri de denetlenir (C: bağlamada
                // ast_tip_to_bilgi → T011/DRF/...). C sırası: E013'ten SONRA, argüman
                // geçişlerinden ÖNCE.
                değişken tfg: tam32 = tf_fg(p, idx);
                eğer tfg >= 0 {
                    değişken tfk2: tam32 = tf_bul(p, idx);
                    değişken tj: tam32 = 0;
                    iken tj < dizi_al(p.tf_say, tfk2) {
                        kontrol_dugum(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, tfk2) + tj));
                        tj = tj + 1;
                    }
                }
                // Ön-geçişte tüketim KAPALI: C lineeri ikilemiyor (`al(tuket(t))` → OK).
                p.tuk_kapali = p.tuk_kapali + 1;
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
                    değişken pt: metin = fn_ptip_bul(p, fad, a - 1);
=======
                    değişken pt: metin = fn_ptip_bul(p, fad, a - 1);
                    // [D1 turbofish] açık bağlama: param tipi İKAME edilmiş tiptir
                    // (generic `T` param artık "?" değil → T001 denetlenir).
                    eğer tfg >= 0 { pt = tf_tip_coz(p, idx, tfg, fn_ptn_bul(p, fad, a - 1)); }
>>>>>>> YENI
```

```duzenle:selfhost/codegen.kem
<<<<<<< ESKI
            ver fn_donus_bul(p, dizi_al(p.a_deg, h));   // "?" = kullanıcı işlevi değil
=======
            // [D1 turbofish] açık bağlamada dönüş tipi İKAME edilir (C gen_substitue).
            değişken tfg3: tam32 = tf_fg(p, idx);
            eğer tfg3 >= 0 {
                eğer dizi_al(p.fg_dn, tfg3) >= 0 { ver tf_tip_coz(p, idx, tfg3, dizi_al(p.fg_dn, tfg3)); }
            }
            ver fn_donus_bul(p, dizi_al(p.a_deg, h));   // "?" = kullanıcı işlevi değil
>>>>>>> YENI
```

## Açıklama

Üç uygulamaya da AYNI D1 turbofish tip-kontrolü portlandı:

- **C (`src/tip_kontrol.c`)** — `turbofish_islev_bul` + `turbofish_on_kontrol` yardımcıları `tip_belirle`'nin hemen öncesine eklendi; `tip_belirle` ve `tip_belirle_beklenen`'in `DUGUM_CAGRI` işlemesi en başta T044 ön-denetimiyle dönüyor; bağlama aşamasında açık argümanlar `gb`'ye argüman unify'ından ÖNCE yazılıyor; pas 2'de açıkça bağlı generic paramın artık `continue` ile atlanmadığı tek koşulla ayrıştırıldı.
- **Self-host (`selfhost/checker.kem` + `selfhost/codegen.kem`)** — `Ayr` yapısına `fg_*` (işlev-generic tablosu) ve `ts_*` (tip_str ikame penceresi) alanları, `ayr_olustur` başlatmasına boş diziler, `parse_islev_genel`'de generic param dilimi yakalama + dönüş düğümü kaydı, `tip_str`'in TIP_BASIT kolunda `ts_coz` ikamesi ve öncesine `ts_coz/fg_bul/tf_fg/tf_tip_coz/tf_on_kontrol` yardımcıları, `kontrol_dugum` CAGRI en başında T044 ön-denetimi + açık tip düğümlerinin denetimi + per-arg `pt` ikamesi, `ifade_tip` CAGRI dönüş tipinde ikame.

VARSAYIMLAR
- `gen_bagla` çağrısının C imzası `gen_bagla(&gb, const char*, int, TipBilgisi*)` ve `ast_tip_to_bilgi(TipKontrol*, const Dugum*)` biçimindedir (görev metnindeki çağrı aynen kullanıldı).
- `hedef_tip->veri.islev.tip_paramlar` bir `const char*` dizisi, `tip_param_sayi` ilgili generic parametre sayısıdır.
- Self-host `Dizi` API'si `dizi_al/dizi_ekle/dizi_yaz/dizi_boyut` görevde belirtildiği gibi vardır; `dizi_yaz(d, i, x)` sınır-kontrollü yerinde yazmadır.
- Self-host'ta `dizi_al(p.dizi, i)` yazılı elemanlar için de okunabilir; `dizi_boyut` mantıksal uzunluğu döndürür (görevdeki S5 yorumuyla uyumlu).
- `sekil_hata(p, idx, kod)` T-kodunu CAGRI düğümünün konumuyla raporlar.
- `metin` tipinin `değişken gad: metin = dizi_al(...)` gibi atamalar için doğrudan kullanılabilir olduğu (tüm var olan kodda böyle).
- `yerel_baglama_mi(p, metin)` ve `tf_bul/bilinen_tip_mi` iki self-host dosyasında da mevcuttur (görev bunu doğruluyor).

## Düşünme (reasoning)

Let me carefully produce the edit blocks. The task specifies exact ESKI/YENI pairs. Let me write them all.

For C: 5 blocks.

For self-host checker.kem and codegen.kem: S1-S8 with S3 different per file and S5 two blocks (helper additions + tip_str line change).

Let me count self-host blocks:
- S1: yapı Ayr field addition
- S2: ayr_olustur init
- S3: parse_islev_genel (different per file)
- S4: parse_islev_genel return node
- S5a: tip_str helper additions (before tip_str function)
- S5b: tip_str TIP_BASIT line change
- S6: kontrol_dugum CAGRI head
- S7a: kontrol_dugum tuk_kapali line
- S7b: kontrol_dugum per-arg pt line
- S8: ifade_tip CAGRI

That's 10 per file. Total C 5, self-host 20, total 25. Good.

Let me write them all out carefully.
