# Tur 24b — C derleyici: dar elemanlı diziler — indeksleme, iki literal yolu, `için`

YALNIZ `src/llvm.c`'ye ```duzenle blokları yaz (4 blok). Başka dosyaya DOKUNMA. C11, sıfır uyarı.
Tur 24a'da şu yardımcılar EKLENDİ (dosyada var, yeniden tanımlama YOK):
`static int dizi_dar_mi(const char *et)` (i8/i16 mi),
`static int dizi_dar_yaz(LlvmGen *g, int reg, const char *src_tip, const char *et, int isaretsiz)` (→ i32),
`static int dizi_dar_oku(LlvmGen *g, const char *fn, int dizi_reg, int idx_reg, const char *et)` (call i32 + trunc).
`ast_tip_isaretsiz_mi(const Dugum *)` dtamN için 1 döner. Her dalın başına `/* [D-707] ... */` yorumu.

## Blok 1 — indeksleme `xs[i]` okuma (kesit ~3790)
ESKİ (tam olarak; bu iki satır + sonraki 6 satır bu bölgede tek):
```
                    if (strcmp(et, "i64") == 0) fn = "kdl_dizi_al_tam64";
                    else if (strcmp(et, "ptr") == 0) fn = "kdl_dizi_al_ptr";
                    int rr = yeni_reg(g);
                    fprintf(g->out,
                        "  %%%d = call %s @%s(ptr %%%d, i32 %%%d)\n",
                        rr, et, fn, v_load, idx_r);
```
YENİ: önce
```
                    if (dizi_dar_mi(et)) {
                        int dr = dizi_dar_oku(g, "kdl_dizi_al_tam", v_load, idx_r, et);
                        IfadeSonuc sd = { dr, et, ast_tip_isaretsiz_mi(vi->eleman_tip_ast), 0};
                        return sd;
                    }
```
sonra ESKİ'nin altı satırı aynen.

## Blok 2 — annotasyonlu literal dizi (kesit ~6505)
ESKİ (tam olarak):
```
                    } else {
                        vr = int_donustur(g, v.reg, v.tip, eleman_tip);
                        fn = "kdl_dizi_ekle_tam";
```
YENİ:
```
                    } else if (dizi_dar_mi(eleman_tip)) {
                        vr = dizi_dar_yaz(g, v.reg, v.tip, eleman_tip,
                                          ast_tip_isaretsiz_mi(eleman_tip_ast_d));
                        e_tip2 = "i32";
                        fn = "kdl_dizi_ekle_tam";
                    } else {
                        vr = int_donustur(g, v.reg, v.tip, eleman_tip);
                        fn = "kdl_dizi_ekle_tam";
```

## Blok 3 — `dizi_literal_heap_emit` literal yolu (kesit ~1745)
ESKİ (tam olarak):
```
        } else {
            vr = int_donustur(g, v.reg, v.tip, elem_ir);
            fn = "kdl_dizi_ekle_tam";
```
YENİ:
```
        } else if (dizi_dar_mi(elem_ir)) {
            vr = dizi_dar_yaz(g, v.reg, v.tip, elem_ir, ast_tip_isaretsiz_mi(elem_d));
            ekle_tip = "i32";
            fn = "kdl_dizi_ekle_tam";
        } else {
            vr = int_donustur(g, v.reg, v.tip, elem_ir);
            fn = "kdl_dizi_ekle_tam";
```
(`elem_d` bu işlevde eleman tip düğümüdür — kesitte `g->beklenen_tip = elem_d;` satırına bak.)

## Blok 4 — `için x: xs` döngüsü (kesit ~7085)
ESKİ (tam olarak):
```
            } else {
                el_reg = yeni_reg(g);
                fprintf(g->out, "  %%%d = call %s @%s(ptr %%%d, i32 %%%d)\n",
                        el_reg, et, fn_al, kdl_ptr, i_load);
            }
```
YENİ:
```
            } else if (dizi_dar_mi(et)) {
                el_reg = dizi_dar_oku(g, fn_al, kdl_ptr, i_load, et);
            } else {
                el_reg = yeni_reg(g);
                fprintf(g->out, "  %%%d = call %s @%s(ptr %%%d, i32 %%%d)\n",
                        el_reg, et, fn_al, kdl_ptr, i_load);
            }
```
NOT: `için` değişkeni `x` dar tipte saklanır; işaretsizliği ayrıca taşınmaz — bu tur için gerekli
değil (x'in kendi tipi `dtam8` olarak bildirilir; genişletme onun tipinden yapılır).
