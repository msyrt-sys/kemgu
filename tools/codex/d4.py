from yardim import testler
header='''// Beklentiler: python3 tools/codex/d4.py; ord ve 8-(x%8) sekme hesabı.
kullan drivers::ekran::konsol::{ Konsol, konsol_yeni, konsol_kod, konsol_metin, konsol_imlec, konsol_alan_sil, kirli_satirlar, konsol_satir };
'''
def wrap(s,w=4,h=3):return f'''    eşleş konsol_yeni({w}, {h}, 7, 0) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken k: Konsol = v;
'''+s+'\n    } }'
cases={}
cases['sarma']=wrap('''        konsol_metin(&değişken k, "ABCDE");
        ver metin_esit(konsol_satir(k, 0), "ABCD") ve metin_esit(konsol_satir(k, 1), "E   ") ve k.x == 1 ve k.y == 1;''')
cases['kaydir']=wrap('''        konsol_metin(&değişken k, "ABCDEFGHIJKL");
        ver metin_esit(konsol_satir(k, 0), "EFGH") ve metin_esit(konsol_satir(k, 1), "IJKL") ve metin_esit(konsol_satir(k, 2), "    ") ve k.x == 0 ve k.y == 2;''')
cases['cr_lf']=wrap('''        konsol_metin(&değişken k, "AB"); konsol_kod(&değişken k, 13); konsol_metin(&değişken k, "X"); konsol_kod(&değişken k, 10);
        konsol_metin(&değişken k, "Y");
        ver metin_esit(konsol_satir(k, 0), "XB  ") ve metin_esit(konsol_satir(k, 1), "Y   ");''')
cases['geri_sil']=wrap('''        konsol_kod(&değişken k, 8); konsol_metin(&değişken k, "ABCD"); konsol_kod(&değişken k, 8);
        ver k.x == 3 ve k.y == 0 ve metin_esit(konsol_satir(k, 0), "ABC ");''')
cases['sekme']=wrap('''        konsol_metin(&değişken k, "A"); konsol_kod(&değişken k, 9);
        eğer k.x != 8 veya k.y != 0 { ver yanlış; }
        konsol_metin(&değişken k, "B"); ver metin_esit(konsol_satir(k, 0), "A       B   ");''',12,2)
cases['unicode']=wrap(f'''        konsol_metin(&değişken k, "ğ😀");
        ver dizi_al(k.kodlar, 0) == {ord('ğ')} ve dizi_al(k.kodlar, 1) == {ord('😀')} ve k.x == 2;''')
cases['kirli']=wrap('''        değişken ilk: Dizi<tam32> = kirli_satirlar(&değişken k);
        eğer dizi_boyut(ilk) != 3 veya dizi_boyut(kirli_satirlar(&değişken k)) != 0 { ver yanlış; }
        konsol_metin(&değişken k, "A");
        değişken xs: Dizi<tam32> = kirli_satirlar(&değişken k);
        ver dizi_boyut(xs) == 1 ve dizi_al(xs, 0) == 0 ve dizi_boyut(kirli_satirlar(&değişken k)) == 0;''')
cases['renk_kaydir']=wrap('''        k.on = 1193046; k.arka = 11259375; konsol_metin(&değişken k, "A"); konsol_kod(&değişken k, 10);
        k.on = 7; k.arka = 0; konsol_metin(&değişken k, "B"); konsol_kod(&değişken k, 10);
        konsol_metin(&değişken k, "C"); konsol_kod(&değişken k, 10);
        ver dizi_al(k.kodlar, 0) == 66 ve dizi_al(k.renkler, 0) == 7 ve dizi_al(k.arkalar, 0) == 0;''')
cases['sil_ve_imlec']=wrap('''        konsol_metin(&değişken k, "ABCDE"); konsol_alan_sil(&değişken k, -1, 3);
        konsol_imlec(&değişken k, 999, -1);
        ver metin_esit(konsol_satir(k, 0), "  CD") ve k.x == 3 ve k.y == 0;''')
cases['gecersiz_kod']=wrap('''        konsol_kod(&değişken k, 0); konsol_kod(&değişken k, 127); konsol_kod(&değişken k, 55296);
        ver k.x == 1 ve dizi_al(k.kodlar, 0) == 65533;''')
for i,(w,h) in enumerate([(0,1),(1,0),(-1,1),(4096,4096)]):cases['boyut_'+str(i)]=f'    eşleş konsol_yeni({w}, {h}, 7, 0) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "boyut"); }} }}'
testler('tests/drivers/ekran/konsol_test.kem',header,cases)
print(len(cases),'test')
