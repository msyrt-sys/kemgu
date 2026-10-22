# Tur 24a — C derleyici: dar elemanlı diziler (i8/i16) — yardımcılar + dizi_ekle/dizi_yaz/dizi_al

YALNIZ `src/llvm.c`'ye ```duzenle blokları yaz (4 blok). Başka dosyaya DOKUNMA. C11, `-Wall -Wextra
-Wpedantic` sıfır uyarı. Kesitler kod yerlerini gösterir.

## Kusur (ölçüldü)
Çalışma zamanı dar tamsayı elemanları (`i8`, `i16`) 32 bitlik yuvada tutar (`kdl_dizi_ekle_tam(i32)`,
`kdl_dizi_al_tam -> i32`). Bugün değer yuvaya `int_donustur` ile yazılıyor; o işaretsiz (`dtamN`)
elemanı da `sext` ediyor → `Dizi<dtam8>`'e 200 eklenip `dizi_al(d,0) olarak tam64` -56 çıkıyor.
Doğrusu: eleman tipinin İŞARETİNE göre genişlet (dtamN → zext, tamN → sext); okurken `i32` çağır,
sonra eleman genişliğine `trunc` et ve sonucun `isaretsiz` bayrağını eleman tipinden taşı.

## Blok 1 — yardımcılar
ESKİ: `static const char *kdl_al_donus_ir(const char *et) {` satırı (kesitte; tek kez geçer).
YENİ: aşağıdaki üç işlev + o satır (aynen; `int_donustur_im`, `int_donustur`, `yeni_reg` dosyada
daha önce tanımlı):
```c
/* [D-707] Dar tamsayi eleman (i8/i16) mi? Calisma zamani bunlari i32 yuvada
 * tasir; yazarken eleman ISARETINE gore genisletilmeli, okurken daraltilmali. */
static int dizi_dar_mi(const char *et) {
    return et && (strcmp(et, "i8") == 0 || strcmp(et, "i16") == 0);
}
/* [D-707] Degeri once eleman tipine, sonra i32 yuvaya getir: isaretsiz
 * (dtamN) eleman zext, isaretli eleman sext. Donus: i32 register. */
static int dizi_dar_yaz(LlvmGen *g, int reg, const char *src_tip,
                        const char *et, int isaretsiz) {
    int r = int_donustur_im(g, reg, src_tip, et, isaretsiz);
    return int_donustur_im(g, r, et, "i32", isaretsiz);
}
/* [D-707] i32 yuvadan oku (`call i32`), eleman genisligine daralt (trunc).
 * Eskiden bazi yollar `call i8 @kdl_dizi_al_tam` yayiyordu: declare i32 ile
 * uyusmaz, LLVM sessizce kabul eder (D-295) — yalniz ABI tesadufuyle calisir. */
static int dizi_dar_oku(LlvmGen *g, const char *fn, int dizi_reg,
                        int idx_reg, const char *et) {
    int raw = yeni_reg(g);
    fprintf(g->out, "  %%%d = call i32 @%s(ptr %%%d, i32 %%%d)\n",
            raw, fn, dizi_reg, idx_reg);
    return int_donustur(g, raw, "i32", et);
}
```

## Blok 2 — `dizi_ekle` yerleşiği (kesit ~5930)
ESKİ (tam olarak bu 4 satır):
```
                else { fn = "kdl_dizi_ekle_tam"; cast_tip = "i32"; }
                ev = (n > 1) ? int_donustur(g, args[1].reg,
                                                 args[1].tip, cast_tip)
                                 : 0;
```
YENİ:
```
                else { fn = "kdl_dizi_ekle_tam"; cast_tip = "i32"; }
                if (dizi_dar_mi(et)) {   /* [D-707] eleman isaretine gore genislet */
                    ev = (n > 1) ? dizi_dar_yaz(g, args[1].reg, args[1].tip, et,
                             ast_tip_isaretsiz_mi(dizi_deger_eleman_ast)) : 0;
                } else {
                ev = (n > 1) ? int_donustur(g, args[1].reg,
                                                 args[1].tip, cast_tip)
                                 : 0;
                }
```

## Blok 3 — `dizi_yaz` yerleşiği (kesit ~6017)
ESKİ (tam olarak):
```
                else { fn = "kdl_dizi_yaz_tam"; cast_tip = "i32"; }
                ev = (n > 2) ? int_donustur(g, args[2].reg,
                                                 args[2].tip, cast_tip) : 0;
```
YENİ: aynı ilk satır, sonra `if (dizi_dar_mi(et)) { ev = (n > 2) ? dizi_dar_yaz(g, args[2].reg,
args[2].tip, et, ast_tip_isaretsiz_mi(dizi_deger_eleman_ast)) : 0; } else {` + eski `ev = ...` iki
satırı + `}` (Blok 2 ile aynı biçim, `[D-707]` yorumuyla).

## Blok 4 — `dizi_al` yerleşiği (kesit ~5978)
ESKİ (tam olarak bu satır):
```
                if (strcmp(et, "i64") == 0) fn = "kdl_dizi_al_tam64";
                else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_al_ptr";
                else { fn = "kdl_dizi_al_tam"; et = "i32"; }
```
(DİKKAT: bu üç satır dosyada `dizi_al` yerleşiğinde geçer; aynı metin başka yerde varsa ESKİ'ye bir
önceki satırı — `IfadeSonuc sf = { fr, et, 0, 0};` ... `}` kapanışı — ekleyerek tekilleştir.)
YENİ: önce şu dal, sonra eski üç satır:
```
                if (dizi_dar_mi(et)) {   /* [D-707] i32 oku + daralt, isaret eleman tipinden */
                    int dr = dizi_dar_oku(g, "kdl_dizi_al_tam", args[0].reg,
                                          idx_i32, et);
                    IfadeSonuc sd = { dr, et,
                                      ast_tip_isaretsiz_mi(dizi_deger_eleman_ast), 0};
                    return sd;
                }
```
