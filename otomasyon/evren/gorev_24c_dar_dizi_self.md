# Tur 24c — self-host codegen: dar elemanlı dizilerde yazma genişletmesi (C D-707 aynası)

YALNIZ `selfhost/codegen.kem`'e ```duzenle blokları yaz (5 blok). Başka dosyaya DOKUNMA. KEMGU dizgi
literalinde kaçış yoktur. Kesitler kod yerlerini gösterir.

## Kusur (ölçüldü)
Çalışma zamanı dar elemanları (`i8`, `i16`) 32 bitlik yuvada tutar: `kdl_dizi_ekle_tam(ptr, ptr, i32)`,
`kdl_dizi_yaz_tam(ptr, i32, i32)`. `dizi_arg_tip(et)` dar eleman için zaten "i32" basıyor AMA değer
register'ı `i8`/`i16` kalıyor → `call void @kdl_dizi_ekle_tam(ptr %r, ptr %d, i32 %15)` ve `%15` i8
→ LLVM REDDİ ("defined with type 'i8' but expected 'i32'"). Dar elemanlı HER dizi derlenemiyor.
Düzeltme: çağrıdan ÖNCE değeri i32'ye genişlet — değer işaretsizse (dtamN) zext, değilse sext.
Mevcut `int_uydur(p, op, hedef, isz)` bunu yapar (p.son_tip = kaynak tip olmalı; immediate'a dokunmaz).
Mevcut `ifade_isz(p, idx) -> tam32` bir ifadenin işaretsiz olup olmadığını verir.

## Blok 1 — yardımcı
ESKİ: `// CG8: değer tipi → çağrı argüman LLVM tipi (i64/ptr/i32).` satırı (tek). YENİ: aşağıdaki işlev +
o satır:
```
// [D-707] Dar eleman (i8/i16) değeri çalışma zamanının i32 yuvasına genişlet:
// işaretsiz (dtamN) → zext, işaretli → sext. Diğer tiplerde değer AYNEN döner.
işlev dizi_dar_genislet(p: &değişken Ayr, op: metin, et: metin, isz: tam32) -> metin {
    eğer metin_esit(et, "i8") veya metin_esit(et, "i16") {
        p.son_tip = et;
        ver int_uydur(p, op, "i32", isz);
    }
    ver op;
}
```

## Blok 2 — `dizi_ekle` yerleşiği (kesit; değer `e = ifade_uret(p, dizi_al(p.cocuk, cb + 2))`)
ESKİ (tam olarak; bu bölgede tek — `p.rho_ref` geçen satırın hemen üstü):
```
                yaz_str("  call void @kdl_dizi_ekle_"); yaz_str(dizi_ekle_sonek(et)); yaz_str("(ptr ");
                yaz_str(p.rho_ref); yaz_str(", ptr ");   // V2-F4.2a: ρ
```
YENİ: önce `                e = dizi_dar_genislet(p, e, et, ifade_isz(p, dizi_al(p.cocuk, cb + 2)));   // [D-707]`
sonra ESKİ'nin iki satırı aynen.

## Blok 3 — `dizi_yaz` yerleşiği (değer `vy = ifade_uret(p, dizi_al(p.cocuk, cb + 3))`, tip `vty`)
ESKİ (tam olarak):
```
                yaz_str("  call void @kdl_dizi_yaz_"); yaz_str(dizi_ekle_sonek(vty)); yaz_str("(ptr ");
                yaz_str(dy); yaz_str(", i32 "); yaz_str(iy); yaz_str(", "); yaz_str(dizi_arg_tip(vty));
```
YENİ: önce `                vy = dizi_dar_genislet(p, vy, vty, ifade_isz(p, dizi_al(p.cocuk, cb + 3)));   // [D-707]`
sonra ESKİ'nin iki satırı aynen.

## Blok 4 — literal dizi (değer `e = ifade_uret(p, dizi_al(p.cocuk, cb + i))`, tip `et`)
ESKİ (tam olarak):
```
                yaz_str("  call void @kdl_dizi_ekle_"); yaz_str(dizi_ekle_sonek(et)); yaz_str("(ptr ");
                yaz_str(drho); yaz_str(", ptr ");   // V2-F4.2b: aynı yönlendirilmiş ρ
```
YENİ: önce `                e = dizi_dar_genislet(p, e, et, ifade_isz(p, dizi_al(p.cocuk, cb + i)));   // [D-707]`
sonra ESKİ'nin iki satırı aynen.

## Blok 5 — indeks ataması `xs[i] = v` (değer `vy2 = ifade_uret(p, dizi_al(p.cocuk, cb + 1))`, tip `vty2`)
ESKİ (tam olarak):
```
            yaz_str("  call void @kdl_dizi_yaz_"); yaz_str(dizi_ekle_sonek(vty2)); yaz_str("(ptr ");
            yaz_str(taban); yaz_str(", i32 "); yaz_str(iy2); yaz_str(", "); yaz_str(dizi_arg_tip(vty2));
```
YENİ: önce `            vy2 = dizi_dar_genislet(p, vy2, vty2, ifade_isz(p, dizi_al(p.cocuk, cb + 1)));   // [D-707]`
sonra ESKİ'nin iki satırı aynen.
