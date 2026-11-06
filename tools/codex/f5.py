"""Her saf modülün seçili veri alanı için 0/1/üst/üst+1 tablosu üretir."""
from pathlib import Path
from yardim import testler,yaz
import struct,zlib,tarfile,io
B='kullan drivers::ortak::bayt::{ bayt_sifir, oku_u8, oku_le16, oku_le32, yaz_u8, yaz_le16, yaz_le32, yaz_le64, yaz_ascii, bayt_toplam8, bayt_sinirda, imza_esit };\n'
F='kullan tests::drivers::bulanik::veri::fikstur::{ bpb_verisi, pci_verisi, elf_verisi, madt_verisi, fadt_verisi, sdt_verisi };\n'
coverage=[]
def emit(key,h,values,domain,body,extra=None):
 cases={f'sinir_{i}':body(n) for i,n in enumerate(values)}
 if extra:cases.update(extra)
 path='tests/drivers/sinir/'+key.replace('/','_')+'_test.kem'
 testler(path,'// Kahin: python3 tools/codex/f5.py; '+domain+'; değerler '+str(values)+'.\n'+h,cases)
 coverage.append((key,domain,values,path,len(cases)))
def rs(call,valid,check='doğru'):
 return f'eşleş {call} {{ hata(e) => {{ ver {"yanlış" if valid else "doğru"}; }} tamam(v) => {{ ver {check if valid else "yanlış"}; }} }}'
def repeat(n,ch='a'):return ch*n
emit('ortak/bayt',B,[0,1,3,4],'3 baytlık tamponun erişim uzunluğu',lambda n:f'    değişken b: Dizi<dtam8> = bayt_sifir(3); ver bayt_sinirda(b, 0, {n}) == {"doğru" if n<=3 else "yanlış"};',{
 'toplama_tasmasi':'    değişken b: Dizi<dtam8> = bayt_sifir(3); ver değil bayt_sinirda(b, 2147483647, 2147483647);',
 'imza_tasmasi':'    değişken b: Dizi<dtam8> = bayt_sifir(3); ver değil imza_esit(b, 2147483647, "x");'})
emit('disk/crc32',B+'kullan drivers::disk::crc32;\n',[0,1,3,4],'3 baytlık CRC aralığının uzunluğu',lambda n:f'    değişken b: Dizi<dtam8> = bayt_sifir(3); yaz_ascii(b, 0, "abc"); ver crc32::crc32(b, 0, {n}) == {zlib.crc32(b"abc"[:n]) if n<=3 else 0};')
emit('disk/guid',B+'kullan drivers::disk::guid::{ guid_bicimle };\n',[0,1,16,17],'GUID sabit kayıt boyu 16',lambda n:'    '+rs(f'guid_bicimle(bayt_sifir({n}))',n==16,'metin_esit(v, "00000000-0000-0000-0000-000000000000")'))
# UTF-16 kodlayıcıda NUL dahil Unicode skaler sınırları bağımsız baytlarla sınanır.
def utfcase(n):
 cp=n if n<=0x10ffff else 0xfffd;raw=chr(cp).encode('utf-16le')
 s=f'    değişken xs: Dizi<tam32> = [{n}]; değişken b: Dizi<dtam8> = utf16le_kodla(xs);\n    ver dizi_boyut(b) == {len(raw)}'
 return s+''.join(f' ve oku_u8(b, {i}) == {v}' for i,v in enumerate(raw))+';'
emit('ortak/utf8',B+'kullan drivers::ortak::utf8::{ utf16le_kodla };\n',[0,1,0x10ffff,0x110000],'Unicode skaler kod noktası',utfcase)
H='kullan drivers::ortak::harf_veri::{ HarfEsleme, ust_eslemeler };\nkullan drivers::ortak::harf::{ buyuk_harf, kucuk_harf };\n'
def harfcase(n):
 if n==0:return '    ver metin_esit(buyuk_harf(""), "") ve metin_esit(kucuk_harf(""), "");'
 if n==1:return '    ver metin_esit(buyuk_harf("i"), "İ") ve metin_esit(kucuk_harf("I"), "ı");'
 if n==0x10ffff:return f'    ver metin_esit(buyuk_harf(kod_metin({n})), kod_metin({n})) ve metin_esit(kucuk_harf(kod_metin({n})), kod_metin({n}));'
 return '''    değişken s: metin = metin_birlestir(metin_kes(kod_metin(1114111),0,1), metin_kes(kod_metin(65536),1,1));
    s = metin_birlestir(s, metin_kes(kod_metin(65536),2,2));
    ver metin_esit(buyuk_harf(s), "����") ve metin_esit(kucuk_harf(s), "����");'''
emit('ortak/harf',H,[0,1,0x10ffff,0x110000],'boş/tek harf ve UTF-8 skaler üst sınırı',harfcase)
# Son eşleme bağımsız Python Unicode 15 tablosundan hesaplanır.
rows=[(cp,('İ' if cp==105 else chr(cp).upper())) for cp in range(0x110000) if not 0xd800<=cp<=0xdfff and ('İ' if cp==105 else chr(cp).upper())!=chr(cp)]
def datacase(n):
 if n<len(rows):return f'    değişken xs: Dizi<HarfEsleme> = ust_eslemeler(); değişken e: HarfEsleme = dizi_al(xs, {n}); ver e.kod == {rows[n][0]} ve metin_esit(e.metin, "{rows[n][1]}");'
 return f'    değişken xs: Dizi<HarfEsleme> = ust_eslemeler(); ver dizi_boyut(xs) == {n} ve metin_esit(buyuk_harf(kod_metin(1114111)), kod_metin(1114111));'
emit('ortak/harf_veri',H,[0,1,len(rows)-1,len(rows)],'üretilmiş üst eşleme dizisi indeksi',datacase)
Y='kullan drivers::fs::yol::{ yol_bol, yol_birlestir };\n'
def yolcase(n):
 body='değişken xs: Dizi<metin> = v; ver '+f'dizi_boyut(xs) == {0 if n==0 else 1} ve metin_esit(yol_birlestir(xs), "/{repeat(n)}");'
 return f'    eşleş yol_bol("/{repeat(n)}") {{ hata(e) => {{ ver {"doğru" if n>255 else "yanlış"}; }} tamam(v) => {{ '+(body if n<=255 else 'ver yanlış;')+' } }'
emit('fs/yol',Y,[0,1,255,256],'yol bileşeni UTF-8 bayt uzunluğu',yolcase)
emit('fs/yol_harf',H+'kullan drivers::fs::yol_harf::{ yol_esit };\n',[0,1,255,256],'normalleştirilen yol bileşeni bayt uzunluğu',lambda n:'    '+rs(f'yol_esit("/{repeat(n,"A")}", "/{repeat(n)}", doğru)',n<=255,'v'))
K='kullan drivers::ps2::klavye_tr::{ TusOlayi, KlavyeDurumu, klavye_yeni, klavye_bayt };\n'
K+='''işlev olay_kodu(o: TusOlayi) -> tam32 { eşleş o { TusOlayi::Yok => { ver 0; } TusOlayi::Karakter(k) => { ver k; } TusOlayi::Ozel(k,b) => { eğer b { ver 0-k; } ver 0-k-100; } } }
'''
emit('ps2/klavye_tr',K,[0,1,255,256],'PS/2 giriş baytı',lambda n:f'    değişken d: KlavyeDurumu = klavye_yeni(); ver olay_kodu(klavye_bayt(&değişken d, {n})) == {-4 if n==1 else 0};')
emit('girdi/klavye_us',K+'kullan drivers::girdi::klavye_tablo::{ TusEsleme };\nkullan drivers::girdi::klavye_us::{ klavye_us_yeni, klavye_us_bayt };\n',[0,1,255,256],'US giriş baytı',lambda n:f'    değişken d: KlavyeDurumu = klavye_us_yeni(); ver olay_kodu(klavye_us_bayt(&değişken d, {n})) == {-4 if n==1 else 0};')
def tablecase(n):return f'''    değişken xs: Dizi<TusEsleme> = []; değişken i: tam32 = 0;
    iken i < {n} {{ dizi_ekle(xs, TusEsleme {{ tarama: 30, alt: 65, ust: 66, harf: yanlış }}); i=i+1; }}
    değişken d: KlavyeDurumu = klavye_yeni(); ver olay_kodu(duzen_bayt(&değişken d, 30, xs)) == {65 if 1<=n<=128 else 0};'''
emit('girdi/klavye_tablo',K+'kullan drivers::girdi::klavye_tablo::{ TusEsleme, duzen_bayt };\n',[0,1,128,129],'karakter eşleme tablosu kapasitesi',tablecase)
emit('girdi/satir',K+'kullan drivers::girdi::satir::{ SatirDuzenleyici, satir_yeni, satir_karakter, satir_metni };\n',[0,1,256,257],'satırdaki Unicode kod noktası sayısı',lambda n:f'''    değişken d: SatirDuzenleyici = satir_yeni(); değişken i: tam32 = 0;
    iken i < {n} {{ satir_karakter(&değişken d, {ord('😀')}); i=i+1; }}
    ver dizi_boyut(d.kodlar) == {min(n,256)} ve metin_uzunluk(satir_metni(d)) == {4*min(n,256)};''')
M='kullan drivers::ps2::fare::{ FareDurumu, FareOlayi, fare_yeni, fare_bayt };\n'
def mousecase(n):return f'''    değişken d: FareDurumu = fare_yeni(yanlış); fare_bayt(&değişken d, 8); fare_bayt(&değişken d, {n});
    eşleş fare_bayt(&değişken d, 0) {{ hiç => {{ ver {"doğru" if n==256 else "yanlış"}; }} değer(v) => {{ ver {f'v.dx == {n} ve v.dy == 0' if n<=255 else 'yanlış'}; }} }}'''
emit('ps2/fare',M,[0,1,255,256],'fare X paket baytı',mousecase)
def cursorcase(n):return f'''    eşleş imlec_yeni(4096, 4096, {n}) {{ hata(e) => {{ ver {"doğru" if n>1024 else "yanlış"}; }} tamam(v) => {{
        değişken d: Imlec = v; değişken o: FareOlayi = FareOlayi {{ dx: 1, dy: -1, dz: 0, sol: yanlış, sag: yanlış, orta: yanlış }};
        imlec_olay(&değişken d, o); ver {f'd.x == {n} ve d.y == {n}' if n<=1024 else 'yanlış'};
    }} }}'''
emit('girdi/imlec',M+'kullan drivers::girdi::imlec::{ Imlec, DugmeOlayi, imlec_yeni, imlec_olay };\n',[0,1,1024,1025],'imleç hız çarpanı',cursorcase)
def ringcase(n):return f'''    eşleş halka_yeni({n}) {{ hata(e) => {{ ver {"doğru" if n>1048576 else "yanlış"}; }} tamam(v) => {{
        değişken d: Halka = v; değişken kabul: mantıksal = it(&değişken d, 7 olarak tam64);
        ver {f'dizi_boyut(d.veri) == {n} ve kabul == '+('doğru' if n else 'yanlış')+f' ve sayi(d) == {1 if n else 0}' if n<=1048576 else 'yanlış'};
    }} }}'''
emit('girdi/halka','kullan drivers::girdi::halka::{ Halka, halka_yeni, it, sayi };\n',[0,1,1048576,1048577],'FIFO eleman kapasitesi',ringcase)
C='kullan drivers::ekran::cerceve::{ Cerceve, cerceve_yeni, piksel_yaz, piksel_oku };\n'
def framecase(n):
 check=f'dizi_boyut(c.veri) == {n*4}'
 if n:check+=f' ve renk_mi(c, {n-1}, 7)'
 return f'''    eşleş cerceve_yeni({n}, 1, {n*4}) {{ hata(e) => {{ ver {"doğru" if n>16384 else "yanlış"}; }} tamam(v) => {{
        değişken c: Cerceve = v; piksel_yaz(c, {n-1}, 0, 7); ver {check if n<=16384 else 'yanlış'};
    }} }}'''
emit('ekran/cerceve',C+'işlev renk_mi(c: Cerceve, x: tam32, r: tam32) -> mantıksal { eşleş piksel_oku(c,x,0) { hiç => { ver yanlış; } değer(v) => { ver v==r; } } }\n',[0,1,16384,16385],'32bpp çerçeve genişliği',framecase)
emit('ekran/konsol','kullan drivers::ekran::konsol::{ Konsol, konsol_yeni };\n',[0,1,4096,4097],'konsol sütun sayısı',lambda n:f'''    eşleş konsol_yeni({n}, 1, 7, 0) {{ hata(e) => {{ ver {"yanlış" if 1<=n<=4096 else "doğru"}; }} tamam(v) => {{ değişken k: Konsol = v; ver {f'k.sutunlar == {n} ve dizi_boyut(k.kodlar) == {n}' if 1<=n<=4096 else 'yanlış'}; }} }}''')
def psfcase(n):
 step=(n+7)//8
 return f'''    değişken b: Dizi<dtam8> = bayt_sifir({32+step}); yaz_le32(b,0,{0x864ab572} olarak tam64); yaz_le32(b,8,32 olarak tam64); yaz_le32(b,16,1 olarak tam64); yaz_le32(b,20,{step} olarak tam64); yaz_le32(b,24,1 olarak tam64); yaz_le32(b,28,{n} olarak tam64);
    eşleş psf_coz(b) {{ hata(e) => {{ ver {"yanlış" if 1<=n<=4096 else "doğru"}; }} tamam(v) => {{ değişken f: PsfFont = v; ver {f'f.genislik == {n} ve f.glif_bayt == {step}' if 1<=n<=4096 else 'yanlış'}; }} }}'''
emit('ekran/psf',B+'kullan drivers::ekran::psf::{ PsfFont, psf_coz };\n',[0,1,4096,4097],'PSF2 glif genişliği',psfcase)
def bmpcase(n):
 step=((n*24+31)//32)*4
 return f'''    değişken b: Dizi<dtam8> = bayt_sifir({54+step}); yaz_ascii(b,0,"BM"); yaz_le32(b,2,{54+step} olarak tam64); yaz_le32(b,10,54 olarak tam64); yaz_le32(b,14,40 olarak tam64); yaz_le32(b,18,{n} olarak tam64); yaz_le32(b,22,1 olarak tam64); yaz_le16(b,26,1 olarak tam64); yaz_le16(b,28,24 olarak tam64);
    eşleş bmp_coz(b) {{ hata(e) => {{ ver {"yanlış" if 1<=n<=16384 else "doğru"}; }} tamam(v) => {{ değişken b: Bmp = v; ver {f'b.genislik == {n} ve b.adim == {step}' if 1<=n<=16384 else 'yanlış'}; }} }}'''
emit('ekran/bmp',B+C+'kullan drivers::ekran::bmp::{ Bmp, bmp_coz };\n',[0,1,16384,16385],'BMP genişliği',bmpcase)
def vtcase(n):
 seq=';'.join(['31']*n);color=0xaa0000 if 1<=n<=16 else 7
 return f'''    eşleş konsol_yeni(4,2,7,0) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken k: Konsol = v; değişken d: VtDurumu = vt_yeni(7,0);
        vt_metin(&değişken d, &değişken k, metin_birlestir(kod_metin(27), "[{seq}mX"));
        ver dizi_al(k.renkler,0) == {color} ve dizi_al(k.kodlar,0) == {ord('X')} ve k.x == 1;
    }} }}'''
emit('ekran/vt','kullan drivers::ekran::konsol::{ Konsol, konsol_yeni };\nkullan drivers::ekran::vt::{ VtDurumu, vt_yeni, vt_metin };\n',[0,1,16,17],'CSI parametre sayısı',vtcase)
def glyphcase(n):
 bitmap=[[128,128,64,0],[255,128,128,0],[224,0,64,0],[128,0,64,0]]
 expected=[(7 if bitmap[n][y*2+x//8]&(128>>(x%8)) else 0) if n<4 else 5 for y in range(2) for x in range(9)]
 s=f'''    eşleş psf_coz(sentetik_font()) {{ hata(e) => {{ ver yanlış; }} tamam(fv) => {{ değişken f: PsfFont = fv;
    eşleş cerceve_yeni(9,2,36) {{ hata(e) => {{ ver yanlış; }} tamam(cv) => {{ değişken c: Cerceve = cv;
        dikdortgen_doldur(c,0,0,9,2,5); glif_ciz(c,f,{n},0,0,7,0);
'''
 for i,r in enumerate(expected):s+=f'        eşleş piksel_oku(c,{i%9},{i//9}) {{ hiç => {{ ver yanlış; }} değer(v) => {{ eğer v != {r} {{ ver yanlış; }} }} }}\n'
 return s+'        ver doğru;\n    } } } }'
emit('ekran/yazi',C+'kullan drivers::ekran::cerceve::{ dikdortgen_doldur };\nkullan drivers::ekran::psf::{ PsfFont, psf_coz };\nkullan drivers::ekran::yazi::{ glif_ciz };\nkullan tests::drivers::ekran::veri::psf_sentetik::{ sentetik_font };\n',[0,1,3,4],'dört glifli fontun glif indeksi',glyphcase)
A='kullan drivers::acpi::tablo::{ SdtBaslik, AcpiHatasi };\n'
def acpicase(n):return f'''    değişken b: Dizi<dtam8> = sdt_verisi(doğru); yaz_le32(b,4,{36+n} olarak tam64); yaz_u8(b,9,0 olarak tam64);
    yaz_u8(b,9, ((256-bayt_toplam8(b,0,{min(52,36+n)})) & 255) olarak tam64);
    eşleş sdt_girdileri(b, doğru) {{ hata(e) => {{ ver {"yanlış" if n in (0,16) else "doğru"}; }} tamam(v) => {{ değişken xs: Dizi<tam64> = v; ver {f'dizi_boyut(xs) == {n//8}' if n in (0,16) else 'yanlış'}; }} }}'''
emit('acpi/tablo',B+F+A+'kullan drivers::acpi::tablo::{ sdt_girdileri };\n',[0,1,16,17],'52 baytlık XSDT içindeki kullanılabilir yük uzunluğu',acpicase)
def madtcase(n):return f'''    değişken b: Dizi<dtam8> = madt_verisi(); yaz_le32(b,4,52 olarak tam64); yaz_u8(b,45,{n} olarak tam64); yaz_u8(b,9,0 olarak tam64); yaz_u8(b,9,((256-bayt_toplam8(b,0,52)) & 255) olarak tam64);
    eşleş madt_coz(b) {{ hata(e) => {{ ver {"yanlış" if n==8 else "doğru"}; }} tamam(v) => {{ değişken m: Madt = v; ver {"dizi_boyut(m.girdiler) == 1" if n==8 else "yanlış"}; }} }}'''
emit('acpi/madt',B+F+A+'kullan drivers::acpi::madt::{ Madt, MadtGirdisi, madt_coz };\n',[0,1,8,9],'tek LAPIC girdisine ayrılmış 8 bayt',madtcase)
def fadtcase(n):return f'''    değişken b: Dizi<dtam8> = fadt_verisi(); yaz_le32(b,4,{n} olarak tam64); yaz_u8(b,9,0 olarak tam64);
    yaz_u8(b,9,((256-bayt_toplam8(b,0,{min(n,148)})) & 255) olarak tam64);
    {rs('fadt_coz(b)',n==148)}'''
emit('acpi/fadt',B+F+A+'kullan drivers::acpi::fadt::{ Fadt, Gas, fadt_coz };\n',[0,1,148,149],'FADT fikstürü kullanılabilir kayıt boyu',fadtcase)
def pcicase(n):return f'''    değişken kaynak: Dizi<dtam8> = pci_verisi(); değişken b: Dizi<dtam8> = []; değişken i: tam32 = 0;
    iken i < {n} {{ değişken v: tam32 = 0; eğer i < 256 {{ v = oku_u8(kaynak,i); }} dizi_ekle(b,v olarak dtam8); i=i+1; }}
    eşleş pci_coz(b) {{ hiç => {{ ver {"doğru" if n<256 else "yanlış"}; }} değer(v) => {{ değişken p: PciAygit = v; ver {f'p.satici == {0x1234}' if n>=256 else 'yanlış'}; }} }}'''
emit('pci/yapilandirma',B+F+'kullan drivers::pci::yapilandirma::{ PciAygit, PciBar, pci_coz };\n',[0,1,256,257],'sabit PCI kayıt boyu; sonda ek bayt yok sayılır',pcicase)
def elfcase(n):return f'''    değişken b: Dizi<dtam8> = elf_verisi(); yaz_le64(b,72,{n} olarak tam64); yaz_le64(b,96,0 olarak tam64);
    {rs('elf64_coz(b)',n<=512)}'''
emit('elf/elf64',B+F+'kullan drivers::elf::elf64::{ Elf64, ElfSegment, elf64_coz };\n',[0,1,512,513],'sıfır dosya boylu LOAD için dosya ofseti',elfcase)
R='kullan drivers::bellek::harita::{ BellekBolgesi, harita_duzenle };\n'
def regionscase(n):return f'''    değişken xs: Dizi<BellekBolgesi> = []; değişken i: tam32 = 0;
    iken i < {n} {{ dizi_ekle(xs, BellekBolgesi {{ taban: 0, uzunluk: 0, tur: 1 }}); i=i+1; }}
    eşleş harita_duzenle(xs) {{ hata(e) => {{ ver {"doğru" if n>4096 else "yanlış"}; }} tamam(v) => {{ ver {"dizi_boyut(v)==0" if n<=4096 else "yanlış"}; }} }}'''
emit('bellek/harita',R,[0,1,4096,4097],'girdi bölge sayısı; sıfır uzunluklu bölgeler elenir',regionscase)
def pagescase(n):return f'''    değişken xs: Dizi<BellekBolgesi> = []; dizi_ekle(xs, BellekBolgesi {{ taban: 1048576, uzunluk: {n*4096} olarak tam64, tur: 1 }});
    eşleş cerceve_yeni(xs) {{ hata(e) => {{ ver {"doğru" if n>1048576 else "yanlış"}; }} tamam(v) => {{ değişken d: CerceveAyirici = v; ver {f'd.toplam=={n} ve dizi_boyut(d.bitmap)=={(n+7)//8}' if n<=1048576 else 'yanlış'}; }} }}'''
emit('bellek/cerceve',R+'kullan drivers::bellek::cerceve::{ CerceveAyirici, cerceve_yeni };\n',[0,1,1048576,1048577],'kullanılabilir sayfa kapasitesi',pagescase)
def mbrcase(n):
 s='    değişken b: Dizi<dtam8> = bayt_sifir(528); yaz_u8(b,510,85 olarak tam64); yaz_u8(b,511,170 olarak tam64);\n'
 for i in range(n):
  off=446+i*16 if i<4 else 512
  s+=f'    yaz_u8(b,{off+4},12 olarak tam64); yaz_le32(b,{off+8},{2048+i*1024} olarak tam64); yaz_le32(b,{off+12},512 olarak tam64);\n'
 s+=f'''    eşleş mbr_coz(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken xs: Dizi<Bolum> = v;
        eğer dizi_boyut(xs) != {min(n,4)} {{ ver yanlış; }} değişken i: tam32 = 0;
        iken i < dizi_boyut(xs) {{ değişken p: Bolum = dizi_al(xs,i); eğer p.baslangic_lba != (2048+i*1024) olarak tam64 veya p.sektor_sayisi != 512 {{ ver yanlış; }} i=i+1; }} ver doğru;
    }} }}'''
 return s
emit('disk/bolum',B+'kullan drivers::disk::bolum::{ Bolum, BolumHatasi, mbr_coz };\n',[0,1,4,5],'dört MBR bölümü; beşinci kayıt sektör dışında yok sayılır',mbrcase)
FB=Path('tests/drivers/fs/fat32_onar_test.kem').read_text().split('işlev test_')[0]
def formatcase(n):
 total=70000*max(1,n)
 return f'''    eşleş fat32_bicimle::fat32_bicimle({total} olarak tam64, {n}, "") {{ hata(e) => {{ ver {"yanlış" if n in (1,128) else "doğru"}; }} tamam(v) => {{ değişken b: Dizi<dtam8> = v; ver {f'oku_u8(b,13)=={n}' if n in (1,128) else 'yanlış'}; }} }}'''
emit('fs/fat32_bicimle',FB,[0,1,128,129],'FAT32 küme başına sektör',formatcase)
emit('fs/fat32_bpb',B+F+'kullan drivers::fs::fat32_bpb::{ Fat32Bpb, BpbHatasi, bpb_coz };\n',[0,1,4096,4097],'BPB sektör bayt boyu',lambda n:f'    değişken b: Dizi<dtam8> = bpb_verisi(); yaz_le16(b,11,{n} olarak tam64); '+rs('bpb_coz(b)',n==4096))
def checksum(raw):
 s=0
 for b in raw:s=(((s&1)<<7)+(s>>1)+b)&255
 return s
def lfndata(n):
 short=bytearray(32);short[:11]=b'SHORT   TXT';short[11]=32;name='a'*min(255,n*13);raw=bytearray()
 units=[ord(c) for c in name]
 for seq in range(n,0,-1):
  rec=bytearray(32);rec[0]=seq|(64 if seq==n else 0);rec[11]=15;rec[13]=checksum(short[:11]);chunk=units[(seq-1)*13:seq*13]
  if len(chunk)<13:chunk+=[0]+[65535]*(12-len(chunk))
  for off,val in zip(list(range(1,11,2))+list(range(14,26,2))+list(range(28,32,2)),chunk):struct.pack_into('<H',rec,off,val)
  raw+=rec
 return raw+short,name if 1<=n<=20 else 'SHORT.TXT'
def dircase(n):
 raw,name=lfndata(n);s=f'    değişken b: Dizi<dtam8> = bayt_sifir({len(raw)});\n'
 for i,v in enumerate(raw):
  if v:s+=f'    yaz_u8(b,{i},{v} olarak tam64);\n'
 return s+f'''    eşleş dizin_coz(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken xs: Dizi<DizinGirdisi> = v;
        eğer dizi_boyut(xs)!=1 {{ ver yanlış; }} değişken g: DizinGirdisi = dizi_al(xs,0); ver metin_esit(g.ad,"{name}");
    }} }}'''
emit('fs/fat32_dizin',B+'kullan drivers::fs::fat32_dizin::{ DizinGirdisi, dizin_coz };\n',[0,1,20,21],'LFN parça sayısı; geçersiz uzun ad kısa ada düşer',dircase)
maxcluster=16777216//512-1110+1
def imagecase(n):return f'''    değişken b: Dizi<dtam8> = imaj_yeni(); eşleş imaj_bpb(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken bp: Fat32Bpb = v;
        {rs(f'imaj_konum(bp, {n} olarak tam64)',n==maxcluster,f'v == {16777216-512}')}
    }} }}'''
emit('fs/fat32_imaj',FB+'kullan drivers::fs::fat32_imaj::{ imaj_konum };\n',[0,1,maxcluster,maxcluster+1],'16MiB bellek önekine sığan son küme numarası',imagecase)
def shortcase(n):
 expected=('A'*n).ljust(11) if n<=8 else 'AAAAAA~1   '
 return f'    değişken xs: Dizi<metin> = []; '+rs(f'kisa_ad_uret("{repeat(n,"A")}",xs)',n>0,f'metin_esit(v,"{expected}")')
emit('fs/fat32_kisa_ad','kullan drivers::fs::fat32_kisa_ad::{ kisa_ad_uret };\n',[0,1,8,9],'8.3 taban adının doğrudan uzunluğu',shortcase)
def writecase(n):return f'''    değişken b: Dizi<dtam8> = imaj_yeni(); değişken veri: Dizi<dtam8> = bayt_sifir({n});
    eşleş imaja_dosya_yaz(b,"/A.TXT",veri) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ eğer v != {n} {{ ver yanlış; }} }} }}
    eşleş imaj_bpb(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken bp: Fat32Bpb = v;
    eşleş imaj_bul(b,bp,2 olarak tam64,"A.TXT") {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken k: FatKayit = v;
        eğer k.boyut != {n} {{ ver yanlış; }}
        {'ver k.ilk==0;' if n==0 else 'eşleş imaj_zincir(b,bp,k.ilk) { hata(e) => { ver yanlış; } tamam(xs) => { ver dizi_boyut(xs)=='+str((n+511)//512)+'; } }'}
    }} }} }} }}'''
emit('fs/fat32_yaz',FB+'kullan drivers::fs::fat32_imaj::{ imaj_bul, imaj_zincir };\n',[0,1,512,513],'tek küme veri kapasitesi ve sonraki kümeye geçiş',writecase)
def readcase(n):return f'''    değişken b: Dizi<dtam8> = imaj_yeni(); değişken veri: Dizi<dtam8> = bayt_sifir(512); yaz_u8(veri,0,77 olarak tam64);
    eşleş imaja_dosya_yaz(b,"/A.TXT",veri) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ }} }}
    yaz_le32(b,{1110*512+28},{n} olarak tam64);
    eşleş imajdan_dosya_oku(b,"/A.TXT") {{ hata(e) => {{ ver {"metin_esit(e, \"kisa zincir\")" if n>512 else "yanlış"}; }} tamam(v) => {{ değişken xs: Dizi<dtam8> = v; ver {f'dizi_boyut(xs)=={n}'+(' ve oku_u8(xs,0)==77' if n else '') if n<=512 else 'yanlış'}; }} }}'''
emit('fs/fat32_oku',FB+'kullan drivers::fs::fat32_oku::{ imajdan_dosya_oku };\n',[0,1,512,513],'tek kümeli dosyanın ilan edilen uzunluğu',readcase)
def repaircase(n):return f'''    değişken b: Dizi<dtam8> = imaj_yeni(); yaz_le32(b,1000,{n} olarak tam64); yaz_le32(b,4072,{n} olarak tam64);
    eşleş fsinfo_onar(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken s: Fat32Sayim = v; ver s.bos=={68890-1} ve s.sonraki==3 ve oku_le32(b,1000)=={68890-1} ve oku_le32(b,4072)=={68890-1}; }} }}'''
emit('fs/fat32_onar',FB,[0,1,68890,68891],'FSInfo boş küme sayacının fiziksel küme sayısına göre sınırı',repaircase)
def tarcase(n):
 info=tarfile.TarInfo('a');info.size=n;info.mtime=0;info.mode=0o644;head=info.tobuf(format=tarfile.USTAR_FORMAT)
 s='    değişken b: Dizi<dtam8> = bayt_sifir(2048);\n'
 for i,v in enumerate(head):
  if v:s+=f'    yaz_u8(b,{i},{v} olarak tam64);\n'
 s+=f'    değişken i: tam32 = 0; iken i < {min(n,512)} {{ yaz_u8(b,512+i,65 olarak tam64); i=i+1; }}\n'
 return s+rs('tar_listele(b)',n<=512,'dizi_boyut(v)==1')
emit('fs/ustar',B+'kullan drivers::fs::ustar::{ TarGirdisi, tar_listele };\n',[0,1,512,513],'2048 baytlık USTAR içinde iki son blokla kalan dosya kapasitesi',tarcase)
emit('sim/kabuk',FB+'kullan drivers::sim::kabuk::{ kabuk_calistir };\n',[0,1,256,257],'satır düzenleyici sınırında doğrudan echo; kabuk ayrı sınır koymaz',lambda n:f'    değişken b: Dizi<dtam8> = []; ver metin_esit(kabuk_calistir(b,"echo {repeat(n,"ğ")}"), metin_birlestir("{repeat(n,"ğ")}",kod_metin(10)));')
actual={str(p.relative_to('drivers').with_suffix('')) for p in Path('drivers').rglob('*.kem') if 'virtio' not in p.parts}
assert actual=={x[0] for x in coverage},actual^{x[0] for x in coverage}
report='# F5 sınır tabloları\n\nÜst sınır, her satırda belirtilen API alanının kapasitesi veya sabit fikstürün son geçerli konumudur. Sınırı olmayan APIlerde fazladan girdinin kabul edilmesi de açıkça sınanır. Önceden mevcut VirtIO modülleri bu saf-mantık dalgasına dahil değildir.\n\n| Modül | Sınır alanı | Değerler | Test |\n|---|---|---|---|\n'
for key,domain,values,path,count in sorted(coverage):report+=f'| `{key}` | {domain} | `{values}` | [{count} test](../../{path}) |\n'
report+='\n39 modül, 156 tablo satırı ve ortak bayt denetimi için 2 taşma regresyonu. Beklentiler Python tam sayıları, struct, zlib, tarfile ve Unicode 15 verisinden hesaplanır.\n'
yaz('otomasyon/chatgpt/codex_sinirlar.md',report)
p=Path('Makefile');s=p.read_text();s=s.replace('tests/drivers/bulanik/*_test.kem))','tests/drivers/bulanik/*_test.kem tests/drivers/sinir/*_test.kem))');p.write_text(s)
print(len(coverage),'modül,',sum(x[-1] for x in coverage),'sınır testi')
