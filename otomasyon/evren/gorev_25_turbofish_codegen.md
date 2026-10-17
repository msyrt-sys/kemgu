# Tur 25 — turbofish CODEGEN: açık argüman monomorfizasyonu EZER

Parser (tur 23) ve checker (tur 24) hazır:
- C: CAGRI düğümü `d` için `d->veri.cagri.tip_arg[ti]` (tip düğümleri),
  `d->veri.cagri.tip_arg_sayi` (yoksa 0).
- Self-host: `tf_bul(p, idx)` → k (yoksa -1); tip düğümleri
  `dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)`, j < `dizi_al(p.tf_say, k)`.

Hedef: `kare::<tam64>(92682)` → `@kare$i64` specialization'ı, argüman `i64`
olarak yayılır (bugün çıkarsama literalden `$i32` seçer ve 92682² taşar).
Açık argüman, argümanlardan ÇIKARSAMAYI EZER. Yalnız iki dosya:
`src/llvm.c` ve `selfhost/codegen.kem`. Başka dosyaya DOKUNMA. Her değişiklik
bir ```duzenle bloğu; ESKI metni harfiyen kopyala (tek kez geçer — ölçüldü).
KEMGU dizgi literallerinde KAÇIŞ YOK.

## C — `src/llvm.c` (4 blok)

### C1. `generic_param_beklenen` — imza + açık argüman dalı
ESKI (tam olarak, altı satır):
```
static const char *generic_param_beklenen(LlvmGen *g, const Dugum *islevd,
                                          int i) {
    if (i >= islevd->veri.islev.param_sayi) return NULL;
    const Dugum *pp = islevd->veri.islev.parametreler[i];
    const Dugum *ppt = pp ? pp->veri.parametre.tip : NULL;
    if (!ppt) return NULL;
```
YENI:
```
static const char *generic_param_beklenen(LlvmGen *g, const Dugum *d,
                                          const Dugum *islevd, int i) {
    if (i >= islevd->veri.islev.param_sayi) return NULL;
    const Dugum *pp = islevd->veri.islev.parametreler[i];
    const Dugum *ppt = pp ? pp->veri.parametre.tip : NULL;
    if (!ppt) return NULL;
    /* [D1 turbofish] ACIK generic argumanli cagri (`f::<tam64>(..)`): generic
     * parametre de SOMUTTUR -> beklenen IR, parametre tipinin acik argumanlarla
     * IKAME edilmis halidir. Literal `92682` boylece i64 dogar (bugun cikarsama
     * onu i32 sayar ve `kare(92682)` tasar). Acik argumanlarin IR'i DIS ikame
     * baglaminda (cagiran specialization'in T'si) hesaplanir — hepsi ONCE
     * hesaplanir, sonra itilir (callee'nin `T`si cagiranin `T`sini golgelemesin). */
    int tf_n = (d && d->tip == DUGUM_CAGRI) ? d->veri.cagri.tip_arg_sayi : 0;
    int tps0 = islevd->veri.islev.tip_param_sayi;
    if (tf_n > 0 && tf_n == tps0) {
        TipSubst *ts = (TipSubst *)arena_ayir_sifir(g->arena,
            sizeof(TipSubst) * (size_t)tps0);
        if (ts) {
            for (int ti = 0; ti < tps0; ti++) {
                ts[ti].ad = islevd->veri.islev.tip_paramlar[ti];
                ts[ti].ad_uz = (int)strlen(ts[ti].ad);
                ts[ti].ir = ast_tip_to_ir(g, d->veri.cagri.tip_arg[ti]);
                ts[ti].metin_mi = tip_metin_mi_subst(g, d->veri.cagri.tip_arg[ti]);
            }
            TipSubst *eski = g->substler;
            for (int ti = 0; ti < tps0; ti++) {
                ts[ti].sonraki = g->substler;
                g->substler = &ts[ti];
            }
            const char *r = ast_tip_to_ir(g, ppt);
            g->substler = eski;
            return r;
        }
    }
```

### C2 ve C3. İki çağrı yeri — `d` geçir
ESKI (tam olarak): `                                ? generic_param_beklenen(g, mik->ast, i)`
YENI: `                                ? generic_param_beklenen(g, d, mik->ast, i)`

ESKI (tam olarak): `                        bekle = generic_param_beklenen(g, ik->ast, i);`
YENI: `                        bekle = generic_param_beklenen(g, d, ik->ast, i);`

### C4. `generic_islev_cagri_uret` — açık argüman tip_args'ı EZER
ESKI (tam olarak, tek satır):
`        tip_args[ti] = inferred ? inferred : "i32";`
YENI:
```
        tip_args[ti] = inferred ? inferred : "i32";
        /* [D1 turbofish] ACIK argumanlar cikarsamayi EZER (checker'daki
         * gen_bagla "ilk gorulen kazanir" ile ayni karar). IR, DIS ikame
         * baglaminda (g->substler) hesaplanir. Metin-lik de acik tipten gelir
         * (D-449: generic govdede `==` icerik mi isaretci mi). */
        if (d->veri.cagri.tip_arg_sayi == tps) {
            tip_args[ti] = ast_tip_to_ir(g, d->veri.cagri.tip_arg[ti]);
            if (tip_args_metin) {
                tip_args_metin[ti] = tip_metin_mi_subst(g,
                    d->veri.cagri.tip_arg[ti]);
            }
        }
```

## Self-host — `selfhost/codegen.kem` (5 blok)

Mevcut ve kullanılacak işlevler (dosyada VAR): `tf_bul`, `fn_dugum_bul`, `nokta_son`,
`ll_tip`, `subst_kapat`, `param_ir_tip`, `dizi_*`, `metin_esit`. `p.tp_yad`/`p.tp_ad`
generic param kayıt defteri (işlev adı → param adları, bildirim SIRASIYLA);
`p.mono_sp`/`p.mono_si`/`p.mono_smet` aktif ikame yığını (`subst_bul` sondan arar;
`subst_kapat(p, bas)` bas'tan itibaren adları "" yapar).

### S1. Yardımcılar — `mono_islev_kayit` tanımından hemen ÖNCE
ESKI (tam olarak, tek satır):
`işlev mono_islev_kayit(p: &değişken Ayr, base: metin, argt: Dizi<metin>, argm: Dizi<metin>, arge: Dizi<metin>) -> metin {`
YENI: aşağıdaki işlevler + ESKI satırın YENİ hâli (S2'de verildi — bu blokta ESKI
satırı S2'deki YENİ imzayla değiştir; yani S1 ve S2 TEK blok olarak yazılır):
```
// [D1 turbofish] CAGRI'nın AÇIK tip argümanlarının IR'ları (aktif ikame altında —
// çağıran bir specialization'ın gövdesindeysek onun T'si çözülür). Yoksa [].
işlev tf_acik_ir(p: &değişken Ayr, idx: tam32) -> Dizi<metin> {
    değişken r: Dizi<metin> = [];
    değişken k: tam32 = tf_bul(p, idx);
    eğer k < 0 { ver r; }
    değişken j: tam32 = 0;
    iken j < dizi_al(p.tf_say, k) {
        dizi_ekle(r, ll_tip(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)));
        j = j + 1;
    }
    ver r;
}
// [D-449] açık tip `metin` mi? ("1" / ""). Generic param adıysa aktif ikamenin
// metin-liğine bakar (mono_smet).
işlev tf_tip_metin(p: &değişken Ayr, n: tam32) -> metin {
    eğer metin_esit(dizi_al(p.a_ad, n), "TIP_BASIT") == yanlış { ver ""; }
    değişken ad: metin = dizi_al(p.a_deg, n);
    eğer metin_esit(ad, "metin") { ver "1"; }
    değişken i: tam32 = dizi_boyut(p.mono_sp) - 1;
    iken i >= 0 {
        eğer metin_esit(dizi_al(p.mono_sp, i), ad) {
            eğer i < dizi_boyut(p.mono_smet) {
                eğer metin_esit(dizi_al(p.mono_smet, i), "1") { ver "1"; }
            }
            ver "";
        }
        i = i - 1;
    }
    ver "";
}
işlev tf_acik_met(p: &değişken Ayr, idx: tam32) -> Dizi<metin> {
    değişken r: Dizi<metin> = [];
    değişken k: tam32 = tf_bul(p, idx);
    eğer k < 0 { ver r; }
    değişken j: tam32 = 0;
    iken j < dizi_al(p.tf_say, k) {
        dizi_ekle(r, tf_tip_metin(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)));
        j = j + 1;
    }
    ver r;
}
// İşlev düğümünün j'inci PARAMETRE'sinin tip düğümü (-1 = yok).
işlev fn_param_tip_dugum(p: &değişken Ayr, fn: tam32, j: tam32) -> tam32 {
    değişken cb: tam32 = dizi_al(p.a_cb, fn);
    değişken cs: tam32 = dizi_al(p.a_cs, fn);
    değişken i: tam32 = 0;  değişken pi: tam32 = 0;
    iken i < cs {
        değişken c: tam32 = dizi_al(p.cocuk, cb + i);
        eğer metin_esit(dizi_al(p.a_ad, c), "PARAMETRE") {
            eğer pi == j {
                eğer dizi_al(p.a_cs, c) > 0 { ver dizi_al(p.cocuk, dizi_al(p.a_cb, c)); }
                ver 0 - 1;
            }
            pi = pi + 1;
        }
        i = i + 1;
    }
    ver 0 - 1;
}
// [D1 turbofish] Açık argümanlı çağrıda j'inci parametrenin IR tipi: parametre
// tip düğümü, AÇIK IR'lar ikame edilmiş halde ll_tip'ten geçer (C
// generic_param_beklenen aynası). Turbofish yok / sayı uyuşmuyor → param_ir_tip.
işlev tf_param_ir(p: &değişken Ayr, idx: tam32, fname: metin, j: tam32) -> metin {
    değişken acik: Dizi<metin> = tf_acik_ir(p, idx);   // DIŞ ikame altında, ÖNCE
    eğer dizi_boyut(acik) == 0 { ver param_ir_tip(p, fname, j); }
    değişken fn: tam32 = fn_dugum_bul(p, fname);
    eğer fn < 0 { ver param_ir_tip(p, fname, j); }
    değişken ptn: tam32 = fn_param_tip_dugum(p, fn, j);
    eğer ptn < 0 { ver param_ir_tip(p, fname, j); }
    değişken bare: metin = nokta_son(fname);
    değişken bas: tam32 = dizi_boyut(p.mono_sp);
    değişken gi: tam32 = 0;
    değişken i: tam32 = 0;
    iken i < dizi_boyut(p.tp_yad) {
        eğer metin_esit(dizi_al(p.tp_yad, i), bare) {
            eğer gi < dizi_boyut(acik) {
                dizi_ekle(p.mono_sp, dizi_al(p.tp_ad, i));
                dizi_ekle(p.mono_si, dizi_al(acik, gi));
                dizi_ekle(p.mono_smet, "");
            }
            gi = gi + 1;
        }
        i = i + 1;
    }
    eğer gi != dizi_boyut(acik) { subst_kapat(p, bas); ver param_ir_tip(p, fname, j); }
    değişken r: metin = ll_tip(p, ptn);
    subst_kapat(p, bas);
    ver r;
}
```

### S2. `mono_islev_kayit` — imza (S1 bloğunun son satırı)
YENİ imza satırı (S1 bloğunun YENI'sinin SON satırı budur):
`işlev mono_islev_kayit(p: &değişken Ayr, base: metin, argt: Dizi<metin>, argm: Dizi<metin>, arge: Dizi<metin>, acik: Dizi<metin>, acikm: Dizi<metin>) -> metin {`

### S3. `mono_islev_kayit` gövdesi — açık IR çıkarsamayı EZER
İki ayrı blok:

(a) ESKI (tam olarak, iki satır):
```
    değişken mang: metin = base;
    değişken i: tam32 = 0;
```
YENI:
```
    değişken mang: metin = base;
    değişken gj: tam32 = 0;   // [D1 turbofish] bu işlevin kaçıncı generic paramı
    değişken i: tam32 = 0;
```

(b) ESKI (tam olarak, tek satır):
`            eğer metin_uzunluk(sp) > 0 { sp = metin_birlestir(sp, " "); si = metin_birlestir(si, " "); sm = metin_birlestir(sm, " "); }`
YENI:
```
            // [D1 turbofish] AÇIK argüman çıkarsamayı EZER (C generic_islev_cagri_uret
            // ve checker gen_bagla ile aynı karar). tp_yad bildirim sırasını tutar.
            eğer gj < dizi_boyut(acik) {
                ir = dizi_al(acik, gj);
                mmet = "";
                eğer gj < dizi_boyut(acikm) { mmet = dizi_al(acikm, gj); }
            }
            gj = gj + 1;
            eğer metin_uzunluk(sp) > 0 { sp = metin_birlestir(sp, " "); si = metin_birlestir(si, " "); sm = metin_birlestir(sm, " "); }
```

### S4. Çağrı yeri — mono kaydına açık IR'lar
ESKI (tam olarak, tek satır):
`                değişken spec: metin = mono_islev_kayit(p, fname, argt, argm, arge);`
YENI:
`                değişken spec: metin = mono_islev_kayit(p, fname, argt, argm, arge, tf_acik_ir(p, idx), tf_acik_met(p, idx));`

### S5. Argüman döngüsü — parametre IR'ı açık argümanlarla
CAGRI ifade üretimindeki argüman döngüsünde `param_ir_tip(p, fname, i - 1)` İKİ kez
geçer (beklenen_ll kurulumu ve D-393 i64 genişletmesi). İkisini de değiştir:

(a) ESKI (tam olarak, tek satır):
`                değişken apt: metin = param_ir_tip(p, fname, i - 1);`
YENI:
`                değişken apt: metin = tf_param_ir(p, idx, fname, i - 1);   // [D1 turbofish]`

(b) ESKI (tam olarak, tek satır):
`                değişken pt: metin = param_ir_tip(p, fname, i - 1);`
YENI:
`                değişken pt: metin = tf_param_ir(p, idx, fname, i - 1);   // [D1 turbofish]`

Toplam blok: C 4 + self-host 6 (S1+S2 tek blok, S3a, S3b, S4, S5a, S5b) = 10.
