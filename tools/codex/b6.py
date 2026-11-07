from yardim import testler
header='''// Hesap: python3 tools/codex/b6.py; ceil(1300/512)=3, serbest küme=68889.
kullan drivers::fs::fat32_bicimle;
// C007: bu tabanda geçişli imzalar T011 verir; açık tip ithalatları korunur.
kullan drivers::fs::fat32_bpb::{ Fat32Bpb };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi };
kullan drivers::fs::fat32_imaj::{ DizinResmi, FatKayit };
// Asıl imaj API adı açık modül yoluyla çağrılır; host API ile karışmaz.
kullan drivers::fs::fat32_yaz;
kullan drivers::ortak::bayt::{ bayt_sifir, oku_u8, oku_le16, oku_le32, yaz_u8, yaz_le32, imza_esit };
kullan drivers::disk::crc32;
işlev imaj_yeni() -> Dizi<dtam8> {
    eşleş fat32_bicimle::fat32_bicimle(70000, 1, "") { tamam(v) => { ver v; } hata(e) => { değişken bos: Dizi<dtam8> = []; ver bos; } }
}
işlev veri_yeni(n: tam32) -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = bayt_sifir(n); değişken i: tam32 = 0;
    iken i < n { yaz_u8(b, i, ((i*17+3)&255) olarak tam64); i = i + 1; } ver b;
}
işlev yazildi(r: sonuç<tam64, metin>, n: tam64) -> mantıksal {
    eşleş r { hata(e) => { yazdir_metin(e); ver yanlış; } tamam(v) => { ver v == n; } }
}
işlev red(r: sonuç<tam64, metin>, s: metin) -> mantıksal {
    eşleş r { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, s); } }
}
işlev aynalar(b: Dizi<dtam8>) -> mantıksal {
    değişken i: tam32 = 0;
    iken i < 275968 { eğer oku_u8(b, 16384+i) != oku_u8(b, 292352+i) { ver yanlış; } i = i + 1; }
    ver doğru;
}
'''
assert (1300+511)//512==3
assert (32+539)*512==292352 and 539*512==275968
cases={}
cases['uc_kume']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    değişken veri: Dizi<dtam8> = veri_yeni(1300);
    eğer değil yazildi(fat32_yaz::dosya_yaz(b, "/FOO.BIN", veri), 1300) { ver yanlış; }
    eğer oku_le32(b, 16396) != 4 veya oku_le32(b, 16400) != 5 veya oku_le32(b, 16404) != 268435455 { ver yanlış; }
    eğer oku_le32(b, 1000) != 68886 veya oku_le32(b, 1004) != 6 veya oku_le32(b, 4072) != 68886 { ver yanlış; }
    eğer değil imza_esit(b, 568320, "FOO     BIN") veya oku_le32(b, 568348) != 1300 { ver yanlış; }
    değişken i: tam32 = 0;
    iken i < 1300 { eğer oku_u8(b, 568832+i) != oku_u8(veri, i) { ver yanlış; } i = i + 1; }
    ver aynalar(b) ve oku_u8(b, 570132) == 0;'''
cases['dizin_ve_dosya']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    eğer değil yazildi(fat32_yaz::dizin_olustur(b, "/belgeler"), 0) { ver yanlış; }
    eğer değil imza_esit(b, 568832, ".          ") veya değil imza_esit(b, 568864, "..         ") { ver yanlış; }
    eğer oku_le16(b, 568858) != 3 veya oku_le16(b, 568890) != 0 { ver yanlış; }
    eğer değil yazildi(fat32_yaz::dosya_yaz(b, "/belgeler/Merhaba😀.txt", veri_yeni(1)), 1) { ver yanlış; }
    eğer değil yazildi(fat32_yaz::dizin_olustur(b, "/belgeler/alt"), 0) { ver yanlış; }
    // Kümeler: kök=2, belgeler=3, dosya=4, alt=5; altın .. girdisi 3.
    ver oku_le16(b, 569914) == 3 ve oku_le32(b, 1000) == 68886 ve aynalar(b);'''
cases['sil_ve_yeniden']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    eğer değil yazildi(fat32_yaz::dosya_yaz(b, "/uzun dosya.txt", veri_yeni(1300)), 1300) { ver yanlış; }
    eşleş fat32_yaz::dosya_sil(b, "/uzun dosya.txt") { hata(e) => { ver yanlış; } tamam(v) => { eğer değil v { ver yanlış; } } }
    eğer oku_le32(b, 16396) != 0 veya oku_le32(b, 16400) != 0 veya oku_le32(b, 16404) != 0 { ver yanlış; }
    eğer oku_u8(b, 568320) != 229 veya oku_u8(b, 568352) != 229 veya oku_u8(b, 568384) != 229 { ver yanlış; }
    eğer oku_le32(b, 1000) != 68889 veya oku_le32(b, 1004) != 3 { ver yanlış; }
    eğer değil yazildi(fat32_yaz::dosya_yaz(b, "/YENI.TXT", veri_yeni(1)), 1) { ver yanlış; }
    ver oku_le16(b, 568346) == 3 ve aynalar(b);'''
cases['bos_dosya']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    eğer değil yazildi(fat32_yaz::dosya_yaz(b, "/BOS", veri_yeni(0)), 0) { ver yanlış; }
    eğer oku_le16(b, 568346) != 0 veya oku_le32(b, 1000) != 68889 { ver yanlış; }
    eşleş fat32_yaz::dosya_sil(b, "/BOS") { hata(e) => { ver yanlış; } tamam(v) => { ver v ve oku_le32(b, 1000) == 68889; } }'''
cases['dizin_buyume']='''    değişken b: Dizi<dtam8> = imaj_yeni(); değişken i: tam32 = 0;
    iken i < 20 {
        değişken ad: metin = metin_birlestir("/", kod_metin(65+i));
        eğer değil yazildi(fat32_yaz::dosya_yaz(b, ad, veri_yeni(0)), 0) { ver yanlış; } i = i + 1;
    }
    ver oku_le32(b, 16392) == 3 ve oku_le32(b, 16396) == 268435455 ve oku_le32(b, 1000) == 68888 ve
        imza_esit(b, 568832, "Q          ") ve aynalar(b);'''
for name,path,error in [('eksik','/yok/a','bulunamadi'),('kok','/','kok'),('goreli','a','goreli'),('yasak','/a?b','ad')]:
 cases[name]=f'''    değişken b: Dizi<dtam8> = imaj_yeni();
    değişken once: tam64 = crc32::crc32(b, 0, dizi_boyut(b));
    eğer değil red(fat32_yaz::dosya_yaz(b, "{path}", veri_yeni(1)), "{error}") {{ ver yanlış; }}
    ver crc32::crc32(b, 0, dizi_boyut(b)) == once;'''
cases['var']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    eğer değil yazildi(fat32_yaz::dosya_yaz(b, "/FOO", veri_yeni(1)), 1) { ver yanlış; }
    değişken once: tam64 = crc32::crc32(b, 0, dizi_boyut(b));
    ver red(fat32_yaz::dosya_yaz(b, "/foo", veri_yeni(2)), "var") ve crc32::crc32(b, 0, dizi_boyut(b)) == once;'''
cases['disk_dolu']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    değişken i: tam32 = 3;
    iken i < 68892 { yaz_le32(b, 16384+i*4, 268435455); yaz_le32(b, 292352+i*4, 268435455); i = i + 1; }
    değişken once: tam64 = crc32::crc32(b, 0, dizi_boyut(b));
    ver red(fat32_yaz::dosya_yaz(b, "/DOLU", veri_yeni(1)), "disk dolu") ve crc32::crc32(b, 0, dizi_boyut(b)) == once;'''
cases['bozuk_fsinfo']='''    değişken b: Dizi<dtam8> = imaj_yeni(); yaz_u8(b, 512, 0);
    değişken once: tam64 = crc32::crc32(b, 0, dizi_boyut(b));
    ver red(fat32_yaz::dosya_yaz(b, "/X", veri_yeni(1)), "fsinfo") ve crc32::crc32(b, 0, dizi_boyut(b)) == once;'''
cases['bozuk_zincir']='''    değişken b: Dizi<dtam8> = imaj_yeni(); yaz_le32(b, 16392, 2); yaz_le32(b, 292360, 2);
    ver red(fat32_yaz::dosya_yaz(b, "/X", veri_yeni(1)), "dongu");'''
cases['kisa_imaj']='    ver red(fat32_yaz::dosya_yaz(bayt_sifir(511), "/X", veri_yeni(0)), "bpb");'
cases['yetersiz_bosluk'] = cases['disk_dolu'].replace('i: tam32 = 3', 'i: tam32 = 4').replace('veri_yeni(1)), "disk dolu"', 'veri_yeni(1024)), "disk dolu"')
cases['sil_hatalari'] = '''    değişken b: Dizi<dtam8> = imaj_yeni();
    eğer değil yazildi(fat32_yaz::dizin_olustur(b, "/DIZIN"), 0) { ver yanlış; }
    değişken once: tam64 = crc32::crc32(b, 0, dizi_boyut(b));
    eşleş fat32_yaz::dosya_sil(b, "/DIZIN") { tamam(v) => { ver yanlış; } hata(e) => { eğer değil metin_esit(e, "dizin") { ver yanlış; } } }
    eşleş fat32_yaz::dosya_sil(b, "/YOK") { tamam(v) => { ver yanlış; } hata(e) => { eğer değil metin_esit(e, "bulunamadi") { ver yanlış; } } }
    ver crc32::crc32(b, 0, dizi_boyut(b)) == once;'''
testler('tests/drivers/fs/fat32_yaz_test.kem',header,cases)
print(len(cases),'test')
