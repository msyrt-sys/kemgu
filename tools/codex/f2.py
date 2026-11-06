from yardim import testler,yaz
from pathlib import Path
header='''// Hesap: python3 tools/codex/f2.py; 68890 küme, kök ve sahipsiz tahsisleri çıkartılır.
kullan drivers::fs::fat32_bpb::{ Fat32Bpb };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi };
kullan drivers::fs::fat32_imaj::{ FatKayit, DizinResmi, imaj_bpb, imaj_fat_yaz };
kullan drivers::fs::fat32_bicimle;
kullan drivers::fs::fat32_yaz::{ imaja_dosya_yaz, imajda_dizin_olustur };
kullan drivers::fs::fat32_onar::{ Fat32Sayim, fsinfo_onar, sahipsiz_kumeler };
kullan drivers::ortak::bayt::{ bayt_sifir, oku_u8, oku_le32, yaz_u8, yaz_le16, yaz_le32 };
kullan drivers::disk::crc32;
işlev imaj_yeni() -> Dizi<dtam8> {
    eşleş fat32_bicimle::fat32_bicimle(70000 olarak tam64, 1, "") { tamam(v) => { ver v; } hata(e) => { değişken b: Dizi<dtam8> = []; ver b; } }
}
işlev fat_yaz(b: Dizi<dtam8>, k: tam64, v: tam64) {
    eşleş imaj_bpb(b) { hata(e) => { } tamam(p) => { değişken bp: Fat32Bpb = p; imaj_fat_yaz(b, bp, k, v); } }
}
işlev sahipsiz_mi(b: Dizi<dtam8>, beklenen: Dizi<tam64>) -> mantıksal {
    eşleş sahipsiz_kumeler(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken xs: Dizi<tam64> = v;
        eğer dizi_boyut(xs) != dizi_boyut(beklenen) { ver yanlış; } değişken i: tam32 = 0;
        iken i < dizi_boyut(xs) { eğer dizi_al(xs,i) != dizi_al(beklenen,i) { ver yanlış; } i = i + 1; } ver doğru;
    } }
}
'''
c={}
c['sayac_onar']=f'''    değişken b: Dizi<dtam8> = imaj_yeni(); fat_yaz(b, 7 olarak tam64, 9 olarak tam64); fat_yaz(b, 9 olarak tam64, 268435455 olarak tam64);
    yaz_le32(b, 1000, 0); yaz_le32(b, 1004, 999); yaz_le32(b, 4072, 123);
    eşleş fsinfo_onar(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken s: Fat32Sayim = v;
        ver s.bos == {68890-3} ve s.sonraki == 3 ve oku_le32(b, 1000) == {68890-3} ve oku_le32(b, 1004) == 3 ve oku_le32(b, 4072) == {68890-3} ve oku_le32(b, 4076) == 3;
    }} }}'''
c['bos']='''    değişken b: Dizi<dtam8> = imaj_yeni(); değişken xs: Dizi<tam64> = []; ver sahipsiz_mi(b, xs);'''
c['tahsis']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    eşleş imajda_dizin_olustur(b, "/D") { hata(e) => { ver yanlış; } tamam(v) => { } }
    eşleş imaja_dosya_yaz(b, "/D/A.TXT", bayt_sifir(1300)) { hata(e) => { ver yanlış; } tamam(v) => { } }
    değişken xs: Dizi<tam64> = []; ver sahipsiz_mi(b, xs);'''
c['sahipsiz']='''    değişken b: Dizi<dtam8> = imaj_yeni(); fat_yaz(b, 100 olarak tam64, 101 olarak tam64); fat_yaz(b, 101 olarak tam64, 268435455 olarak tam64); fat_yaz(b, 200 olarak tam64, 268435455 olarak tam64); fat_yaz(b, 300 olarak tam64, 268435447 olarak tam64);
    değişken xs: Dizi<tam64> = [100 olarak tam64, 101 olarak tam64, 200 olarak tam64]; ver sahipsiz_mi(b, xs);'''
c['sahipsiz_dongu']='''    değişken b: Dizi<dtam8> = imaj_yeni(); fat_yaz(b, 100 olarak tam64, 101 olarak tam64); fat_yaz(b, 101 olarak tam64, 100 olarak tam64);
    değişken xs: Dizi<tam64> = [100 olarak tam64, 101 olarak tam64]; ver sahipsiz_mi(b, xs);'''
c['capraz']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    eşleş imaja_dosya_yaz(b, "/A.TXT", bayt_sifir(1)) { hata(e) => { ver yanlış; } tamam(v) => { } }
    eşleş imaja_dosya_yaz(b, "/B.TXT", bayt_sifir(1)) { hata(e) => { ver yanlış; } tamam(v) => { } }
    yaz_le16(b, 568378, 3);
    eşleş sahipsiz_kumeler(b) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "capraz zincir"); } }'''
c['bozuk_kok']='''    değişken b: Dizi<dtam8> = imaj_yeni(); fat_yaz(b, 2 olarak tam64, 0 olarak tam64);
    eşleş sahipsiz_kumeler(b) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "bozuk zincir"); } }'''
c['fsinfo_red']='''    değişken b: Dizi<dtam8> = imaj_yeni(); yaz_u8(b, 512, 0);
    değişken once: tam64 = crc32::crc32(b, 0, dizi_boyut(b));
    eşleş fsinfo_onar(b) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "fsinfo") ve crc32::crc32(b, 0, dizi_boyut(b)) == once; } }'''
c['kisa']='''    eşleş sahipsiz_kumeler(bayt_sifir(0)) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş fsinfo_onar(bayt_sifir(511)) { tamam(v) => { ver yanlış; } hata(e) => { ver doğru; } }'''
testler('tests/drivers/fs/fat32_onar_test.kem',header,c)
print(len(c),'test; ikinci kısa kayıt ilk küme ofseti',1110*512+32+26)
# Dış kahin iki gerçek KEMGU imajını alır: onarılmış sayaçlar ve üç sahipsiz küme.
base=Path('tests/drivers/fs/fat32_kahin_test.kem').read_text()
helpers=base[base.index('işlev ondalik('):base.index('işlev test_dok(')]
k={}
k['sayac_kahin']='''    değişken b: Dizi<dtam8> = imaj_yeni(); yaz_le32(b, 1000, 123); yaz_le32(b, 4072, 123);
    eşleş fsinfo_onar(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken s: Fat32Sayim = v; eğer s.bos != 68889 veya s.sonraki != 3 { ver yanlış; } } }
    yazdir_metin("IMAJ temiz"); seyrek_dok(b); ver doğru;'''
k['sahipsiz_kahin']='''    değişken b: Dizi<dtam8> = imaj_yeni(); fat_yaz(b, 100 olarak tam64, 101 olarak tam64); fat_yaz(b, 101 olarak tam64, 268435455 olarak tam64); fat_yaz(b, 200 olarak tam64, 268435455 olarak tam64);
    değişken beklenen: Dizi<tam64> = [100 olarak tam64, 101 olarak tam64, 200 olarak tam64];
    eğer değil sahipsiz_mi(b, beklenen) { ver yanlış; }
    eşleş fsinfo_onar(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken s: Fat32Sayim = v; eğer s.bos != 68886 { ver yanlış; } } }
    yazdir_metin("IMAJ sahipsiz"); seyrek_dok(b);
    eşleş sahipsiz_kumeler(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken xs: Dizi<tam64> = v; değişken i: tam32 = 0;
        iken i < dizi_boyut(xs) { yazdir_metin(metin_birlestir("SAHIPSIZ ", ondalik(dizi_al(xs,i) olarak tam32))); i = i+1; }
    } } ver doğru;'''
testler('tests/drivers/fs/fat32_onar_kahin_test.kem',header+helpers,k)
