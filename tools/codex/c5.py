from yardim import testler
header='''// Hesap: python3 tools/codex/c5.py; sayfa=4096, ilk 1MiB=256 sayfa.
kullan drivers::bellek::harita::{ BellekBolgesi };
kullan drivers::bellek::cerceve::{ CerceveAyirici, cerceve_yeni, ayir, ayir_ardisik, serbest, ayrilmis_isaretle };
işlev harita_verisi() -> Dizi<BellekBolgesi> {
    değişken xs: Dizi<BellekBolgesi> = [];
    dizi_ekle(xs, BellekBolgesi { taban: 0, uzunluk: 1064960, tur: 1 });
    dizi_ekle(xs, BellekBolgesi { taban: 1056768, uzunluk: 4096, tur: 2 });
    dizi_ekle(xs, BellekBolgesi { taban: 2097152, uzunluk: 12288, tur: 1 }); ver xs;
}
işlev adres_mi(r: seçimlik<tam64>, a: tam64) -> mantıksal { eşleş r { hiç => { ver yanlış; } değer(v) => { ver v == a; } } }
işlev yok(r: seçimlik<tam64>) -> mantıksal { eşleş r { hiç => { ver doğru; } değer(v) => { ver yanlış; } } }
işlev red(r: sonuç<mantıksal, metin>, s: metin) -> mantıksal { eşleş r { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, s); } } }
'''
assert 0x104000==1064960 and 0x102000==1056768
addresses=[0x100000,0x101000,0x103000,0x200000,0x201000,0x202000]
cases={}
def wrap(s):return '''    eşleş cerceve_yeni(harita_verisi()) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken d: CerceveAyirici = v;
'''+s+'\n    } }'
cases['sirali']=wrap('        eğer d.toplam != 6 { ver yanlış; }\n'+''.join(f'        eğer değil adres_mi(ayir(&değişken d), {a}) {{ ver yanlış; }}\n' for a in addresses)+'        ver yok(ayir(&değişken d));')
cases['ardisik']=wrap('''        eğer değil yok(ayir_ardisik(&değişken d, 4)) { ver yanlış; }
        eğer değil adres_mi(ayir_ardisik(&değişken d, 3), 2097152) { ver yanlış; }
        eğer değil adres_mi(ayir_ardisik(&değişken d, 2), 1048576) { ver yanlış; }
        ver adres_mi(ayir(&değişken d), 1060864) ve yok(ayir(&değişken d));''')
cases['serbest']=wrap('''        eğer değil adres_mi(ayir(&değişken d), 1048576) { ver yanlış; }
        eşleş serbest(&değişken d, 1048576 olarak tam64) { hata(e) => { ver yanlış; } tamam(v) => { eğer değil v { ver yanlış; } } }
        eğer değil red(serbest(&değişken d, 1048576 olarak tam64), "cift serbest") { ver yanlış; }
        ver adres_mi(ayir(&değişken d), 1048576);''')
for name,address,error in [('hizasiz',0x100001,'hizasiz'),('ilk_mib',0,'haritada yok'),('delik',0x102000,'haritada yok'),('disinda',0x300000,'haritada yok'),('ayrilmamis',0x100000,'cift serbest')]:cases[name]=wrap(f'        ver red(serbest(&değişken d, {address} olarak tam64), "{error}");')
cases['isaretle']=wrap('''        eşleş ayrilmis_isaretle(&değişken d, 1048577 olarak tam64, 4096 olarak tam64) { hata(e) => { ver yanlış; } tamam(v) => { } }
        eğer değil red(serbest(&değişken d, 1048576 olarak tam64), "ayrilmis bolge") { ver yanlış; }
        ver adres_mi(ayir(&değişken d), 1060864);''')
cases['ardisik_gecersiz']=wrap('        ver yok(ayir_ardisik(&değişken d, 0)) ve yok(ayir_ardisik(&değişken d, -1)) ve yok(ayir_ardisik(&değişken d, 7)) ve adres_mi(ayir(&değişken d), 1048576);')
cases['isaret_tasma']=wrap('        ver red(ayrilmis_isaretle(&değişken d, 9223372036854775807 olarak tam64, 1 olarak tam64), "adres") ve adres_mi(ayir(&değişken d), 1048576);')
cases['bos']='''    değişken xs: Dizi<BellekBolgesi> = [];
    eşleş cerceve_yeni(xs) { hata(e) => { ver yanlış; } tamam(v) => { değişken d: CerceveAyirici = v; ver d.toplam == 0 ve yok(ayir(&değişken d)); } }'''
cases['hiza']='''    değişken xs: Dizi<BellekBolgesi> = [];
    dizi_ekle(xs, BellekBolgesi { taban: 1048577, uzunluk: 8191, tur: 1 });
    eşleş cerceve_yeni(xs) { hata(e) => { ver yanlış; } tamam(v) => { değişken d: CerceveAyirici = v; ver adres_mi(ayir(&değişken d), 1052672) ve yok(ayir(&değişken d)); } }'''
cases['boy_siniri']=f'''    değişken xs: Dizi<BellekBolgesi> = [];
    dizi_ekle(xs, BellekBolgesi {{ taban: 1048576, uzunluk: {(1048576+1)*4096}, tur: 1 }});
    eşleş cerceve_yeni(xs) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "cok sayfa"); }} }}'''
testler('tests/drivers/bellek/cerceve_test.kem',header,cases)
print(len(cases),'test',addresses)
