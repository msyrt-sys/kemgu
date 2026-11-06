from yardim import testler
from pathlib import Path
header='''// Beklentiler: python3 tools/codex/d6.py; Python Unicode dilimleri ve ord.
kullan drivers::ps2::klavye_tr::{ TusOlayi, KlavyeDurumu, klavye_yeni, klavye_bayt };
kullan drivers::girdi::satir::{ SatirDuzenleyici, satir_yeni, satir_olay, satir_metni };
kullan drivers::ortak::utf8::{ utf8_coz };
işlev metin_ekle(d: &değişken SatirDuzenleyici, s: metin) {
    değişken ks: Dizi<tam32> = utf8_coz(s); değişken i: tam32 = 0;
    iken i < dizi_boyut(ks) { satir_olay(d, TusOlayi::Karakter(dizi_al(ks, i))); i = i + 1; }
}
işlev tus(d: &değişken SatirDuzenleyici, k: tam32) { satir_olay(d, TusOlayi::Ozel(k, doğru)); }
'''
c={}
c['unicode_ekle']='''    değişken d: SatirDuzenleyici = satir_yeni(); metin_ekle(&değişken d, "a😀"); tus(&değişken d, 7); metin_ekle(&değişken d, "ğ");
    ver metin_esit(satir_metni(d), "ağ😀") ve d.imlec == 2 ve dizi_boyut(d.kodlar) == 3;'''
c['sil']='''    değişken d: SatirDuzenleyici = satir_yeni(); metin_ekle(&değişken d, "ağ😀z"); tus(&değişken d, 7); tus(&değişken d, 2); tus(&değişken d, 9);
    ver metin_esit(satir_metni(d), "ağ") ve d.imlec == 2;'''
c['sinir_tus']='''    değişken d: SatirDuzenleyici = satir_yeni(); tus(&değişken d, 7); tus(&değişken d, 8); tus(&değişken d, 2); tus(&değişken d, 9); tus(&değişken d, 5); tus(&değişken d, 6);
    değişken kd: KlavyeDurumu = klavye_yeni(); satir_olay(&değişken d, klavye_bayt(&değişken kd, 0)); ver d.imlec == 0 ve metin_esit(satir_metni(d), "");'''
c['birakma']='''    değişken d: SatirDuzenleyici = satir_yeni(); metin_ekle(&değişken d, "a"); satir_olay(&değişken d, TusOlayi::Ozel(2, yanlış));
    ver metin_esit(satir_metni(d), "a");'''
c['enter']='''    değişken d: SatirDuzenleyici = satir_yeni(); metin_ekle(&değişken d, "ğ😀");
    eşleş satir_olay(&değişken d, TusOlayi::Ozel(1, doğru)) { hiç => { ver yanlış; } değer(s) => {
        ver metin_esit(s, "ğ😀") ve d.imlec == 0 ve dizi_boyut(d.kodlar) == 0 ve dizi_boyut(d.gecmis) == 1;
    } }'''
c['bos_enter']='''    değişken d: SatirDuzenleyici = satir_yeni();
    eşleş satir_olay(&değişken d, TusOlayi::Ozel(1, doğru)) { hiç => { ver yanlış; } değer(s) => { ver metin_esit(s, "") ve dizi_boyut(d.gecmis) == 0; } }'''
c['gecmis']='''    değişken d: SatirDuzenleyici = satir_yeni(); metin_ekle(&değişken d, "bir"); tus(&değişken d, 1); metin_ekle(&değişken d, "iki"); tus(&değişken d, 1); metin_ekle(&değişken d, "taslak");
    tus(&değişken d, 5); eğer değil metin_esit(satir_metni(d), "iki") { ver yanlış; }
    tus(&değişken d, 5); tus(&değişken d, 5); eğer değil metin_esit(satir_metni(d), "bir") { ver yanlış; }
    tus(&değişken d, 6); tus(&değişken d, 6); tus(&değişken d, 6); ver metin_esit(satir_metni(d), "taslak") ve d.imlec == 6;'''
c['tekrar']='''    değişken d: SatirDuzenleyici = satir_yeni(); metin_ekle(&değişken d, "a"); tus(&değişken d, 1); metin_ekle(&değişken d, "a"); tus(&değişken d, 1); metin_ekle(&değişken d, "b"); tus(&değişken d, 1); metin_ekle(&değişken d, "a"); tus(&değişken d, 1);
    ver dizi_boyut(d.gecmis) == 3;'''
c['gecmis_16']=f'''    değişken d: SatirDuzenleyici = satir_yeni(); değişken i: tam32 = 0;
    iken i < 17 {{ satir_olay(&değişken d, TusOlayi::Karakter({ord('a')} + i)); tus(&değişken d, 1); i = i + 1; }}
    ver dizi_boyut(d.gecmis) == 16 ve metin_esit(dizi_al(d.gecmis, 0), "{chr(ord('a')+1)}") ve metin_esit(dizi_al(d.gecmis, 15), "{chr(ord('a')+16)}");'''
c['kapasite']=f'''    değişken d: SatirDuzenleyici = satir_yeni(); değişken i: tam32 = 0;
    iken i < 257 {{ satir_olay(&değişken d, TusOlayi::Karakter({ord('😀')})); i = i + 1; }}
    ver dizi_boyut(d.kodlar) == 256 ve d.imlec == 256 ve metin_uzunluk(satir_metni(d)) == {len(('😀'*256).encode())};'''
c['gecersiz']='''    değişken d: SatirDuzenleyici = satir_yeni(); değişken ks: Dizi<tam32> = [-1, 0, 9, 31, 127, 55296, 1114112]; değişken i: tam32 = 0;
    iken i < dizi_boyut(ks) { satir_olay(&değişken d, TusOlayi::Karakter(dizi_al(ks, i))); i = i + 1; } ver dizi_boyut(d.kodlar) == 0;'''
testler('tests/drivers/girdi/satir_test.kem',header,c)
p=Path('Makefile');s=p.read_text();s=s.replace('tests/drivers/ekran/*_test.kem))','tests/drivers/ekran/*_test.kem tests/drivers/girdi/*_test.kem))');p.write_text(s)
print(len(c),'test')
