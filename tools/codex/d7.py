from yardim import testler
header='''// Beklentiler: python3 tools/codex/d7.py; min/max ile ekran kırpma ve bit kenarları.
kullan drivers::ps2::fare::{ FareOlayi };
kullan drivers::girdi::imlec::{ DugmeOlayi, Imlec, imlec_yeni, imlec_olay };
işlev olay(x: tam32, y: tam32, z: tam32, tus: tam32) -> FareOlayi {
    ver FareOlayi { dx: x, dy: y, dz: z, sol: (tus & 1) != 0, sag: (tus & 2) != 0, orta: (tus & 4) != 0 };
}
işlev kenar(o: DugmeOlayi, k: tam32, bas: mantıksal) -> mantıksal {
    eşleş o { DugmeOlayi::Basildi(v) => { ver bas ve v == k; } DugmeOlayi::Birakildi(v) => { ver değil bas ve v == k; } }
}
'''
def wrap(s,hiz=2,w=100,h=80):return f'    eşleş imlec_yeni({w}, {h}, {hiz}) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken d: Imlec = v;\n'+s+'\n    } }'
c={}
c['hareket']=wrap(f'''        imlec_olay(&değişken d, olay(7, -9, 1, 0)); imlec_olay(&değişken d, olay(-2, 3, -3, 0));
        ver d.x == {(7-2)*2} ve d.y == {-(-9+3)*2} ve d.tekerlek == {1-3};''')
c['kirp']=wrap(f'''        imlec_olay(&değişken d, olay(2147483647, -2147483648, 0, 0));
        eğer d.x != {min(99,2147483647*2)} veya d.y != {min(79,2147483648*2)} {{ ver yanlış; }}
        imlec_olay(&değişken d, olay(-2147483648, 2147483647, 0, 0)); ver d.x == 0 ve d.y == 0;''')
c['kenarlar']=wrap('''        değişken a: Dizi<DugmeOlayi> = imlec_olay(&değişken d, olay(0, 0, 0, 7));
        eğer dizi_boyut(a) != 3 { ver yanlış; }
        eğer değil kenar(dizi_al(a, 0), 1, doğru) veya değil kenar(dizi_al(a, 1), 2, doğru) veya değil kenar(dizi_al(a, 2), 3, doğru) { ver yanlış; }
        eğer dizi_boyut(imlec_olay(&değişken d, olay(0, 0, 0, 7))) != 0 { ver yanlış; }
        değişken b: Dizi<DugmeOlayi> = imlec_olay(&değişken d, olay(0, 0, 0, 2));
        ver dizi_boyut(b) == 2 ve kenar(dizi_al(b, 0), 1, yanlış) ve kenar(dizi_al(b, 1), 3, yanlış) ve d.dugmeler == 2;''')
c['sifir_hiz']=wrap('''        imlec_olay(&değişken d, olay(3, -4, 1, 1)); ver d.x == 0 ve d.y == 0 ve d.tekerlek == 1 ve d.dugmeler == 1;''',0)
c['bir_piksel']=wrap('''        imlec_olay(&değişken d, olay(2147483647, -2147483648, 0, 0)); ver d.x == 0 ve d.y == 0;''',1024,1,1)
c['tekerlek_tasma']=wrap('''        d.tekerlek = 2147483647; imlec_olay(&değişken d, olay(0, 0, 1, 0));
        eğer d.tekerlek != 2147483647 { ver yanlış; } d.tekerlek = (-2147483648) olarak tam32;
        imlec_olay(&değişken d, olay(0, 0, -1, 0)); ver d.tekerlek == -2147483648;''')
for i,(w,h,hiz) in enumerate([(0,1,1),(1,0,1),(1,1,-1),(1,1,1025)]):c['gecersiz_'+str(i)]=f'    eşleş imlec_yeni({w}, {h}, {hiz}) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "boyut veya hiz"); }} }}'
testler('tests/drivers/girdi/imlec_test.kem',header,c);print(len(c),'test')
