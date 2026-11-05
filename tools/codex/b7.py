from yardim import testler
import runpy
h=runpy.run_path('tools/codex/b6.py')['header']
h+='''kullan drivers::fs::fat32_oku::{ imajdan_dosya_oku, imaj_dizin_listele };
işlev okundu(r: sonuç<Dizi<dtam8>, metin>, veri: Dizi<dtam8>) -> mantıksal {
    eşleş r { hata(e) => { yazdir_metin(e); ver yanlış; } tamam(v) => {
        değişken b: Dizi<dtam8> = v;
        eğer dizi_boyut(b) != dizi_boyut(veri) { ver yanlış; }
        değişken i: tam32 = 0;
        iken i < dizi_boyut(b) { eğer oku_u8(b, i) != oku_u8(veri, i) { ver yanlış; } i = i + 1; }
        ver doğru;
    } }
}
işlev okunamadi(r: sonuç<Dizi<dtam8>, metin>, s: metin) -> mantıksal {
    eşleş r { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, s); } }
}
'''
cases={}
for n in (0,1,511,512,513,1537):
 cases['gidis_donus_'+str(n)]=f'''    değişken b: Dizi<dtam8> = imaj_yeni(); değişken veri: Dizi<dtam8> = veri_yeni({n});
    eğer değil yazildi(fat32_yaz::imaja_dosya_yaz(b, "/TEST.BIN", veri), {n}) {{ ver yanlış; }}
    ver okundu(imajdan_dosya_oku(b, "/test.bin"), veri);'''
cases['alt_dizin']='''    değişken b: Dizi<dtam8> = imaj_yeni(); değişken veri: Dizi<dtam8> = veri_yeni(1600);
    eğer değil yazildi(fat32_yaz::imajda_dizin_olustur(b, "/belgeler"), 0) { ver yanlış; }
    eğer değil yazildi(fat32_yaz::imaja_dosya_yaz(b, "/belgeler/ğ😀.txt", veri), 1600) { ver yanlış; }
    eğer değil okundu(imajdan_dosya_oku(b, "/belgeler/ğ😀.txt"), veri) { ver yanlış; }
    eğer değil okunamadi(imajdan_dosya_oku(b, "/BELGELER/ğ😀.txt"), "bulunamadi") { ver yanlış; }
    eşleş imaj_dizin_listele(b, "/belgeler") { hata(e) => { ver yanlış; } tamam(v) => {
        değişken xs: Dizi<DizinGirdisi> = v;
        eğer dizi_boyut(xs) != 3 { ver yanlış; }
        değişken a: DizinGirdisi = dizi_al(xs, 0); değişken c: DizinGirdisi = dizi_al(xs, 2);
        ver metin_esit(a.ad, ".") ve metin_esit(c.ad, "ğ😀.txt") ve c.boyut == 1600;
    } }'''
cases['sil_sonra_oku']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    eğer değil yazildi(fat32_yaz::imaja_dosya_yaz(b, "/merhaba.txt", veri_yeni(3)), 3) { ver yanlış; }
    eşleş fat32_yaz::imajdan_dosya_sil(b, "/merhaba.txt") { hata(e) => { ver yanlış; } tamam(v) => { } }
    eğer değil okunamadi(imajdan_dosya_oku(b, "/merhaba.txt"), "bulunamadi") { ver yanlış; }
    eşleş imaj_dizin_listele(b, "/") { hata(e) => { ver yanlış; } tamam(v) => { ver dizi_boyut(v) == 0; } }'''
cases['dosya_dizin_ayrimi']='''    değişken b: Dizi<dtam8> = imaj_yeni();
    eğer değil yazildi(fat32_yaz::imaja_dosya_yaz(b, "/A", veri_yeni(1)), 1) { ver yanlış; }
    eğer değil okunamadi(imajdan_dosya_oku(b, "/"), "dizin") { ver yanlış; }
    eşleş imaj_dizin_listele(b, "/A") { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "dizin degil"); } }'''
for name,change,error in [('dongu','yaz_le32(b, 16396, 3);','dongu'),('kisa','yaz_le32(b, 16396, 268435455);','kisa zincir'),('bos','yaz_le32(b, 16396, 0);','bozuk zincir'),('buyuk','yaz_le32(b, 568348, 4294967295);','dosya buyuk')]:
 cases['bozuk_'+name]=f'''    değişken b: Dizi<dtam8> = imaj_yeni();
    eğer değil yazildi(fat32_yaz::imaja_dosya_yaz(b, "/A", veri_yeni(513)), 513) {{ ver yanlış; }}
    {change}
    ver okunamadi(imajdan_dosya_oku(b, "/A"), "{error}");'''
cases['disk_dolu']='''    değişken b: Dizi<dtam8> = imaj_yeni(); değişken i: tam32 = 3;
    iken i < 68892 { yaz_le32(b, 16384+i*4, 268435455); yaz_le32(b, 292352+i*4, 268435455); i = i + 1; }
    eğer değil red(fat32_yaz::imaja_dosya_yaz(b, "/X", veri_yeni(1)), "disk dolu") { ver yanlış; }
    ver okunamadi(imajdan_dosya_oku(b, "/X"), "bulunamadi");'''
cases['kisa_imaj']='    ver okunamadi(imajdan_dosya_oku(bayt_sifir(0), "/a"), "bpb");'
testler('tests/drivers/fs/fat32_oku_test.kem',h,cases)
print(len(cases),'test')
