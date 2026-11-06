from yardim import yaz,testler
import struct
from pathlib import Path
# Bağımsız BMP kurucu: üstten renk ızgarasını diskteki satır düzenine çevirir.
grid=[[0xff0000,0x00ff00,0x0000ff],[0x123456,0xffffff,0]]
def bmp(bpp=24,top=False,masks=None):
 w,h=3,2;step=((w*bpp+31)//32)*4;off=54+(12 if masks else 0)
 head=struct.pack('<2sIHHI',b'BM',off+step*h,0,0,off)+struct.pack('<IiiHHIIiiII',40,w,-h if top else h,1,bpp,3 if masks else 0,step*h,0,0,0,0)
 if masks:head+=struct.pack('<III',*masks)
 rows=grid if top else grid[::-1];raw=bytearray()
 for row in rows:
  for rgb in row:
   v=rgb
   if masks:
    v=0
    for component,mask in zip([(rgb>>16)&255,(rgb>>8)&255,rgb&255],masks):
     shift=(mask&-mask).bit_length()-1;maximum=mask>>shift
     v|=((component*maximum)//255)<<shift
   raw+=v.to_bytes(bpp//8,'little')
  raw+=b'\xcc'*(step-w*(bpp//8))
 return head+raw
fixtures={'alt24':bmp(),'ust24':bmp(top=True),'alt32':bmp(32),'maske32':bmp(32,True,(0x3ff00000,0x000ffc00,0x000003ff))}
fs='// Python struct BMP kahini: python3 tools/codex/d9.py\n'
for name,data in fixtures.items():fs+=f'genel işlev {name}() -> Dizi<dtam8> {{ değişken b: Dizi<dtam8> = ['+', '.join(str(v)+' olarak dtam8' for v in data)+']; ver b; }\n'
yaz('tests/drivers/ekran/veri/bmp_fikstur.kem',fs)
header='''// Kahin: python3 tools/codex/d9.py; BMP struct ve bağımsız RGB ızgarası.
kullan drivers::ekran::cerceve::{ Cerceve, cerceve_yeni, piksel_oku };
kullan drivers::ekran::bmp::{ Bmp, bmp_coz, bmp_ciz };
kullan tests::drivers::ekran::veri::bmp_fikstur::{ alt24, ust24, alt32, maske32 };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le16, yaz_le32 };
işlev reddet(b: Dizi<dtam8>) -> mantıksal { eşleş bmp_coz(b) { hata(e) => { ver doğru; } tamam(v) => { ver yanlış; } } }
işlev renk(c: Cerceve, x: tam32, y: tam32, beklenen: tam32) -> mantıksal { eşleş piksel_oku(c,x,y) { hiç => { ver yanlış; } değer(v) => { ver v == beklenen; } } }
'''
c={}
for name in fixtures:
 colors=grid
 if name=='maske32':colors=[[sum((((rgb>>shift)&255)*1023//255)*255//1023<<shift for shift in (16,8,0)) for rgb in row] for row in grid]
 s=f'''    eşleş cerceve_yeni(3, 2, 16) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken c: Cerceve = v;
        eşleş bmp_ciz(c, {name}(), 0, 0) {{ hata(e) => {{ ver yanlış; }} tamam(n) => {{ eğer n != 6 {{ ver yanlış; }} }} }}
        ver '''+' ve '.join(f'renk(c, {x}, {y}, {colors[y][x]})' for y in range(2) for x in range(3))+';\n    } }'
 c[name]=s
c['kirp']='''    eşleş cerceve_yeni(2, 1, 8) { hata(e) => { ver yanlış; } tamam(v) => { değişken c: Cerceve = v;
        eşleş bmp_ciz(c, alt24(), -1, -1) { hata(e) => { ver yanlış; } tamam(n) => { ver n == 2 ve renk(c, 0, 0, 16777215) ve renk(c, 1, 0, 0); } }
    } }'''
c['disari']='''    eşleş cerceve_yeni(2, 1, 8) { hata(e) => { ver yanlış; } tamam(v) => { değişken c: Cerceve = v;
        eşleş bmp_ciz(c, alt24(), -2147483648, 2147483647) { hata(e) => { ver yanlış; } tamam(n) => { ver n == 0 ve renk(c, 0, 0, 0); } }
    } }'''
for name,offs,size,value in [('imza',0,1,0),('dib_kisa',14,4,39),('dib_tasma',14,4,2**32-1),('ofset',10,4,53),('genislik_sifir',18,4,0),('genislik_negatif',18,4,2**32-1),('yukseklik_sifir',22,4,0),('yukseklik_min',22,4,2**31),('duzlem',26,2,2),('bpp',28,2,16),('sikistirma',30,4,1),('dosya_kisa',2,4,54),('dosya_uzun',2,4,999)]:
 fn={1:'yaz_u8',2:'yaz_le16',4:'yaz_le32'}[size]
 c[name]=f'    değişken b: Dizi<dtam8> = alt24(); {fn}(b, {offs}, {value} olarak tam64); ver reddet(b);'
for name,r,g in [('sifir_maske',0,0xff00),('ortusen',0xff,0xff),('aralikli',0x550000,0xff00)]:c[name]=f'    değişken b: Dizi<dtam8> = maske32(); yaz_le32(b, 54, {r} olarak tam64); yaz_le32(b, 58, {g} olarak tam64); ver reddet(b);'
c['kesik']='''    değişken b: Dizi<dtam8> = alt24(); değişken k: Dizi<dtam8> = []; değişken i: tam32 = 0;
    iken i < dizi_boyut(b) - 1 { dizi_ekle(k, (dizi_al(b, i) olarak tam32) olarak dtam8); i = i + 1; }
    ver reddet(k) ve reddet(bayt_sifir(0)) ve reddet(bayt_sifir(53));'''
testler('tests/drivers/ekran/bmp_test.kem',header,c);print(len(c),'test; fixture boyları', {k:len(v) for k,v in fixtures.items()})
