# Tagged-Union (sonuç/seçimlik/çeşit) Self-Host Codegen Direktifi

> D3 Öncelik-1'in **son ve en büyük** rung'u. Skaler `eşleş` (literal+catch-all)
> zaten indi (commit `cg_esles_tamsayi`). Bu direktif tagged-union *değer inşası*
> (yapıcılar) + *destructuring* (eşleş payload bind) işini, birinci elden
> tersine-mühendislikle çıkarılmış tam entegrasyon yüzeyiyle tarif eder.
>
> **Tasarım kararı (kritik kolaylaştırıcı):** codegen.kem'de C'nin `beklenen` AST
> plumbing'i YOK. Bunun yerine **LLVM-tip-string yaklaşımı** kullan: beklenen tip
> zaten LLVM struct string olarak elde edilebilir (`ll_tip` / `p.cur_ret`).
> Yapıcı tag/payload kararını struct string'inden türet. AST-node plumbing GEREKMEZ.

## 0. Layout (C llvm.c:721-744 ile birebir)
- `seçimlik<T>` → `{i8, <T>}` — tag alan 0, payload alan 1. `değer`=tag 0, `hiç`=tag 1 (payload yok).
- `sonuç<T,H>` → `{i8, <T>, <H>}` — `tamam`=tag 0 alan 1, `hata`=tag 1 alan 2.
- (`çeşit` ADT → `{i8 disc, <payload union approx>}` — bu direktifte SONRA; önce sonuç/seçimlik.)
- codegen_diff exit-eşdeğerlik karşılaştırır (byte-IR değil) → tag değerleri C ile
  AYNI olmak zorunda DEĞİL, sadece self-tutarlı (yapıcı tag X yazar, eşleş tag X okur).
  Yine de C ile aynı (0/1) tut — netlik + ileride byte-diff istenirse hazır.

## 1. Entegrasyon noktaları (8 — hepsi codegen.kem)

### (1) `ll_tip` — TIP_SECIMLIK / TIP_SONUC → struct string
`ll_tip` başında, `TIP_REFERANS/POINTER/DIZI → ptr` satırından SONRA, `TIP_BASIT
== yanlış → i32` satırından ÖNCE ekle (yoksa TIP_KULLANICI değil diye i32'ye düşer):
```
eğer metin_esit(ta, "TIP_SECIMLIK") {        // {i8, T}
    değişken tt: metin = ll_tip(p, dizi_al(p.cocuk, dizi_al(p.a_cb, tip_idx)));
    ver metin_birlestir(metin_birlestir("{i8, ", tt), "}");
}
eğer metin_esit(ta, "TIP_SONUC") {           // {i8, T, H}
    değişken sc: tam32 = dizi_al(p.a_cb, tip_idx);
    değişken tt: metin = ll_tip(p, dizi_al(p.cocuk, sc));
    değişken hh: metin = ll_tip(p, dizi_al(p.cocuk, sc + 1));
    // "{i8, " + tt + ", " + hh + "}"
    ver metin_birlestir(metin_birlestir(metin_birlestir(metin_birlestir(
        "{i8, ", tt), ", "), hh), "}");
}
```
AST: TIP_SECIMLIK = parse_tek_arg → child[0]=T (codegen.kem:747). TIP_SONUC =
dugum2(dt,ht) → child[0]=T child[1]=H (codegen.kem:756).

### (2) `agg_alan(s, n)` yardımcısı — struct string'in n. alan tipi
`{i8, i32, ptr}` → alan 0="i8", 1="i32", 2="ptr". `{` ile `}` arası `, ` ile böl.
bayt-tarama ile yaz (codegen.kem string builtin'leri: bayt/metin_kes/metin_uzunluk).
Yapıcı payload tipini + eşleş extract tipini buradan al.

### (3) `Ayr` struct + init: `beklenen_ll: metin` (varsayılan "")
Satır ~3808 initializer'a `beklenen_ll: ""` ekle (eksikse parse/tip hatası).

### (4) DEGISKEN — yapıcı bağlamı kur
codegen.kem:2585 civarı: `vtip = ll_tip(annotation)` HESAPLANDIKTAN SONRA, değer
eval'inden (`v = ifade_uret(... cb+cs-1)`) ÖNCE:
```
değişken eski_bll: metin = p.beklenen_ll;
eğer annot == 1 ve bayt(vtip, 0) == 123 { p.beklenen_ll = vtip; }   // '{' = 123
değişken v: metin = ifade_uret(p, dizi_al(p.cocuk, cb + cs - 1));
p.beklenen_ll = eski_bll;   // RESET (sızdırma)
```

### (5) VER — `ver tamam(x)` bağlamı
codegen.kem VER handler (deyim_uret ~2471): `p.cur_ret` aktif fn dönüş LLVM tipi.
Değer eval'inden önce: `eğer bayt(p.cur_ret, 0) == 123 { p.beklenen_ll = p.cur_ret; }`
sonra reset. (cur_ret zaten ll_tip ile set ediliyor — (1) sonrası `{i8,...}` olur.)

### (6) CAGRI — `değer`/`tamam`/`hata` yapıcıları
CAGRI handler başında (codegen.kem:2147), fn_var_mi/builtin'den ÖNCE:
```
eğer (metin_esit(fad,"değer") veya metin_esit(fad,"tamam") veya metin_esit(fad,"hata"))
     ve bayt(p.beklenen_ll, 0) == 123 {
    değişken agg: metin = p.beklenen_ll;
    değişken tag: tam32 = 0;  değişken pf: tam32 = 1;
    eğer metin_esit(fad, "hata") { tag = 1; pf = 2; }
    // alloca agg; store i8 tag, alan 0; eval arg → store, alan pf; load agg
    değişken ar: metin = reg_str(yeni_reg(p));
    yaz_str("  "); yaz_str(ar); yaz_str(" = alloca "); yaz_str(agg); yb(10);
    değişken g0: metin = reg_str(yeni_reg(p));   // GEP alan 0
    yaz_str("  "); yaz_str(g0); yaz_str(" = getelementptr "); yaz_str(agg);
    yaz_str(", ptr "); yaz_str(ar); yaz_str(", i32 0, i32 0"); yb(10);
    yaz_str("  store i8 "); yaz_str(tam64_str(tag olarak tam64)); yaz_str(", ptr ");
    yaz_str(g0); yb(10);
    // payload: arg = ilk çocuk (CAGRI child[1..]; child[0]=hedef? — CAGRI layout'a bak)
    değişken pt: metin = agg_alan(agg, pf);
    değişken eski: metin = p.beklenen_ll; p.beklenen_ll = "";   // iç içe yapıcı reset
    değişken pv: metin = ifade_uret(p, <arg-node>);
    p.beklenen_ll = eski;
    // (int genişlik uyumu gerekirse int_donustur; payload ptr/struct ise düz store)
    değişken gp: metin = reg_str(yeni_reg(p));
    yaz_str("  "); yaz_str(gp); yaz_str(" = getelementptr "); yaz_str(agg);
    yaz_str(", ptr "); yaz_str(ar); yaz_str(", i32 0, i32 "); yaz_str(tam64_str(pf olarak tam64)); yb(10);
    yaz_str("  store "); yaz_str(pt); yaz_str(" "); yaz_str(pv); yaz_str(", ptr "); yaz_str(gp); yb(10);
    değişken lr: metin = reg_str(yeni_reg(p));
    yaz_str("  "); yaz_str(lr); yaz_str(" = load "); yaz_str(agg); yaz_str(", ptr "); yaz_str(ar); yb(10);
    p.son_tip = agg;
    ver lr;
}
```
NOT: CAGRI çocuk düzeni — `değer(x)`'in arg'ı: CAGRI node child layout'unu doğrula
(hedef + argümanlar). codegen.kem CAGRI handler'ında arg erişim deseni mevcut; onu kullan.

### (7) TANIMLAYICI — `hiç` yapıcısı
`hiç` HIC token → parse_birincil'de TANIMLAYICI-benzeri (codegen.kem:961-962).
TANIMLAYICI emission'da (codegen.kem ~2098), cg_var_bul'dan ÖNCE:
```
eğer metin_esit(dizi_al(p.a_deg, idx), "hiç") ve bayt(p.beklenen_ll, 0) == 123 {
    // alloca agg; store i8 1, alan 0; (payload yazma — undef); load
    ... (tag=1, payload yok) ...
    p.son_tip = p.beklenen_ll; ver lr;
}
```
(HIC ayrı node olarak da gelebilir — `metin_esit(ad,"HIC")` kontrolü de ekle.)

### (8) ESLES — tagged-union destructuring (struct scrutinee)
Mevcut ESLES handler'da `skaler == yanlış { ver 0; }` erken-çıkışını DEĞİŞTİR:
struct scrutinee (`bayt(sty,0) == 123`) için tagged-union dalı:
```
// tag = extractvalue agg sval, 0  (bir kez)
değişken tagr: metin = reg_str(yeni_reg(p));
yaz_str("  "); yaz_str(tagr); yaz_str(" = extractvalue "); yaz_str(sty);
yaz_str(" "); yaz_str(sval); yaz_str(", 0"); yb(10);
// her kol:
//   desen DESEN_YAPICI (değer(v)/tamam(v)/hata(e)) → tag (değer/tamam=0, hata=1),
//        payload alan (1 veya 2). icmp eq i8 tagr, <tag> → Lbody/Lnext.
//        Lbody'de: pv = extractvalue sty sval, <pf>; bind: alloca <pt>, store pv,
//        cg_var_ekle(alt-desen adı, alloca, pt, "", ""); deyim_uret(govde); br Lend.
//   desen DESEN_TANIMLAYICI "hiç" → tag 1, payload yok, catch benzeri tag-check.
//   DESEN_JOKER → catch-all.
```
Bağlama (binding): DESEN_YAPICI child[0] = alt-desen (DESEN_TANIMLAYICI, bağlanacak
ad). `dizi_al(p.a_deg, alt_desen)` = ad. alloca+store+cg_var_ekle ile bağla; arm
gövdesinden sonra append-only cg_var doğal olarak sonraki kollarda gölgelenir
(BLOK scope push/pop yok — D3 notu; eşleş kolları sıralı, ad çakışması korunur).

## 2. Test korpusu (test/cg_korpus/, codegen_diff her ikisini de koşar)
```
// cg_secimlik.kem
işlev kutula(x: tam32) -> seçimlik<tam32> { ver değer(x); }
işlev coz(o: seçimlik<tam32>) -> tam32 {
    eşleş o { değer(v) => { ver v; } hiç => { ver 0; } }
    ver 0;
}
işlev main() -> tam32 {
    değişken a: seçimlik<tam32> = kutula(42);
    değişken b: seçimlik<tam32> = hiç;
    ver coz(a) + coz(b);   // 42 + 0 = 42
}
```
Sonra `cg_sonuc.kem` (tamam/hata + sonuç<tam32,tam32>). NOT: `sonuç<T,metin>`
(E=ptr) self-host'ta da çalışmalı (D1 C tarafında çözüldü; self-host layout
{i8,i32,ptr} agg_alan ile doğru payload tipi verir).

## 3. Kapılar
1. C-kemgu sanity: `--check` OK + `--llvm | clang | run` → 42 (oracle).
2. **codegen_diff**: cg_secimlik/cg_sonuc her iki backend'de exit eşdeğer.
3. **bootstrap fixpoint birebir** (codegen.kem kendi kaynağında sonuç/seçimlik/eşleş
   *kullanmıyor* → ekleme additive, stage1==stage2 korunur — DOĞRULA).
4. Struct-by-value param/dönüş (`seçimlik<T>` param/return) zaten destekli (D3 ✓);
   `{i8,i32}` agg param olarak geçer — codegen_diff ile teyit.

## 4. Riskler
- **Bağlama scope'u:** arm gövdesinde bind edilen ad append-only cg_var'a eklenir;
  BLOK scope push/pop olmadığından sonraki kollarda görünür kalır — sıralı kollarda
  zararsız (her kol kendi adını kullanır) ama farklı kollarda AYNI ad farklı tiple
  bağlanırsa son kazanır. Test'te kol-başı benzersiz ad kullan; gerekirse arm-scope
  push/pop ekle (cg_base benzeri).
- **beklenen_ll sızması:** her yapıcı/iç-ifade sonrası RESET şart (yapıcı arg'ı kendi
  beklenen'ini ezmeli — örn. `tamam(değer(x))` iç içe). Push/pop deseni (eski sakla).
- **payload int genişlik:** arg tipi alan tipiyle uyuşmazsa int_donustur (sext/trunc).
- **çeşit (ADT):** bu direktif sonuç/seçimlik. çeşit disc+payload union ayrı ek pas
  (cesit_struct_ir + cesit_yapici_uret C:572 deseni).

## 5. Sıra
seçimlik (cg_secimlik geçene dek) → sonuç (cg_sonuc) → her adımda codegen_diff +
fixpoint. çeşit ADT ayrı pas. Tek-kanal seri (codegen.kem); her mantıksal birim
ayrı Türkçe commit.
