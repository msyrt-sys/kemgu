from yardim import testler,yaz
import io,tarfile
buf=io.BytesIO();long='uzun/'+'a'*110+'/not.txt';content='Merhaba ğ😀'.encode()
with tarfile.open(fileobj=buf,mode='w',format=tarfile.USTAR_FORMAT,encoding='utf-8') as tar:
 for name,data,directory in [('boot/',b'',True),('boot/ğ.txt',content,False),(long,b'PREFIX',False),('bos',b'',False)]:
  t=tarfile.TarInfo(name);t.mtime=0;t.mode=0o755 if directory else 0o644;t.type=tarfile.DIRTYPE if directory else tarfile.REGTYPE;t.size=len(data)
  tar.addfile(t,io.BytesIO(data))
data=buf.getvalue()
fixture='''// Python tarfile USTAR_FORMAT kahini: python3 tools/codex/c7.py.
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };
genel işlev tar_imaj() -> Dizi<dtam8> {
'''+f'    değişken b: Dizi<dtam8> = bayt_sifir({len(data)});\n'
for i,x in enumerate(data):
 if x:fixture+=f'    yaz_u8(b, {i}, {x});\n'
fixture+='    ver b;\n}\n';yaz('tests/drivers/fs/veri/ustar_veri.kem',fixture)
header='''// Beklentiler: python3 tools/codex/c7.py; tarfile üyeleri ve UTF-8 encode.
kullan drivers::fs::ustar::{ TarGirdisi, tar_listele, tar_bul };
kullan tests::drivers::fs::veri::ustar_veri::{ tar_imaj };
kullan drivers::ortak::bayt::{ bayt_sifir, oku_u8, yaz_u8, yaz_ascii };
işlev onar(b: Dizi<dtam8>) {
    değişken toplam: tam32 = 0; değişken i: tam32 = 0;
    iken i < 512 { eğer i >= 148 ve i < 156 { toplam = toplam + 32; } değilse { toplam = toplam + oku_u8(b, i); } i = i + 1; }
    i = 5; iken i >= 0 { yaz_u8(b, 148+i, (48+toplam%8) olarak tam64); toplam = toplam/8; i = i-1; }
    yaz_u8(b, 154, 0); yaz_u8(b, 155, 32);
}
'''
cases={}
cases['liste']=f'''    eşleş tar_listele(tar_imaj()) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken xs: Dizi<TarGirdisi> = v; eğer dizi_boyut(xs) != 4 {{ ver yanlış; }}
        değişken d: TarGirdisi = dizi_al(xs, 0); değişken f: TarGirdisi = dizi_al(xs, 1); değişken p: TarGirdisi = dizi_al(xs, 2);
        ver d.dizin_mi ve metin_esit(d.ad, "boot/") ve metin_esit(f.ad, "boot/ğ.txt") ve f.boyut == {len(content)} ve metin_esit(p.ad, "{long}");
    }} }}'''
s='''    eşleş tar_bul(tar_imaj(), "boot/ğ.txt") { hiç => { ver yanlış; } değer(v) => {
        değişken b: Dizi<dtam8> = v;
'''+f'        eğer dizi_boyut(b) != {len(content)} {{ ver yanlış; }}\n'
for i,x in enumerate(content):s+=f'        eğer oku_u8(b, {i}) != {x} {{ ver yanlış; }}\n'
cases['icerik']=s+'        ver doğru;\n    } }'
for name,ad in [('eksik','yok'),('dizin','boot/')]:cases[name]=f'    eşleş tar_bul(tar_imaj(), "{ad}") {{ hiç => {{ ver doğru; }} değer(v) => {{ ver yanlış; }} }}'
cases['bos_dosya']='    eşleş tar_bul(tar_imaj(), "bos") { hiç => { ver yanlış; } değer(v) => { ver dizi_boyut(v) == 0; } }'
for name,change,error in [('saglama','yaz_u8(b, 0, 65);','saglama'),('imza','yaz_u8(b, 257, 0);','imza'),('boy_sekiz','yaz_u8(b, 124, 56); onar(b);','sekizlik'),('boy_tasma','yaz_ascii(b, 124, "77777777777"); onar(b);','veri siniri'),('tur','yaz_u8(b, 156, 50); onar(b);','tur')]:cases[name]=f'    değişken b: Dizi<dtam8> = tar_imaj(); {change}\n    eşleş tar_listele(b) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "{error}"); }} }}'
for n in (0,511,512,1023):cases['son_'+str(n)]=f'    eşleş tar_listele(bayt_sifir({n})) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "son blok"); }} }}'
cases['bos_arsiv']='    eşleş tar_listele(bayt_sifir(1024)) { hata(e) => { ver yanlış; } tamam(v) => { ver dizi_boyut(v) == 0; } }'
testler('tests/drivers/fs/ustar_test.kem',header,cases)
print(len(cases),'test; tarfile boyu',len(data),'Unicode içerik',list(content))
