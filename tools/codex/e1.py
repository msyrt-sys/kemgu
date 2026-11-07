from yardim import testler
from pathlib import Path
import re,gzip
# Klavye beklentisi uygulamadan değil A1 spesifikasyonundaki tablodan.
rows=[]
for line in Path('otomasyon/chatgpt/isler/ps2_klavye.md').read_text().splitlines():
 m=re.match(r'\| (\d+) \| `(.*?)` = (\d+) \| `(.*?)` = (\d+) \| (evet|hayır) \|',line)
 if m:
  sc,a,ka,b,kb,_=m.groups();rows.append((int(sc),a,b))
def scancodes(s):
 out=[]
 for ch in s:
  sc,shift=next((sc,sh) for sc,a,b in rows for sh,v in [(False,a),(True,b)] if v==ch)
  if shift:out.append(42)
  out.extend([sc,sc+128])
  if shift:out.append(170)
 return out+[28,156]
commands=['ls /belgeler','cat /belgeler/merhaba.txt'];sc=sum(map(scancodes,commands),[])
text='Merhaba Dünya — ğüşıöç';fg=0xabc123;bg=0x102030
font=gzip.decompress(Path('/usr/share/consolefonts/Lat38-VGA8.psf.gz').read_bytes());height=font[3];idx=ord('m');bitmap=font[4+idx*height:4+(idx+1)*height]
# PSF1 Unicode eşlemesinden m indeksini bağımsız doğrula.
off=4+256*height;found=None
for g in range(256):
 while True:
  cp=int.from_bytes(font[off:off+2],'little');off+=2
  if cp==65535:break
  if cp==ord('m'):found=g
assert found==idx
header='''// Kahin: python3 tools/codex/e1.py; A1 tablo baytları, Python UTF-8 ve gerçek PSF bitleri.
kullan drivers::ps2::klavye_tr::{ TusOlayi, KlavyeDurumu, klavye_yeni, klavye_bayt };
kullan drivers::girdi::satir::{ SatirDuzenleyici, satir_yeni, satir_olay };
kullan drivers::fs::fat32_bpb::{ Fat32Bpb };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi };
kullan drivers::fs::fat32_imaj::{ DizinResmi, FatKayit };
kullan drivers::fs::fat32_bicimle;
kullan drivers::fs::fat32_yaz;
kullan drivers::sim::kabuk::{ kabuk_calistir };
kullan drivers::ekran::konsol::{ Konsol, konsol_yeni, konsol_metin, konsol_satir };
kullan drivers::ekran::cerceve::{ Cerceve, cerceve_yeni, piksel_oku };
kullan drivers::ekran::psf::{ PsfFont, psf_coz };
kullan drivers::ekran::yazi::{ kod_ciz };
kullan tests::drivers::ekran::veri::psf_gercek::{ gercek_font };
'''
header+='''işlev imaj_yeni() -> Dizi<dtam8> {
    eşleş fat32_bicimle::fat32_bicimle(70000 olarak tam64, 1, "") { tamam(v) => { ver v; } hata(e) => { değişken b: Dizi<dtam8> = []; ver b; } }
}
işlev bitir(s: metin) -> metin { ver metin_birlestir(s, kod_metin(10)); }
'''
c={}
s=f'''    değişken disk: Dizi<dtam8> = imaj_yeni();
    eşleş fat32_yaz::dizin_olustur(disk, "/belgeler") {{ hata(e) => {{ ver yanlış; }} tamam(n) => {{ }} }}
    değişken veri: Dizi<dtam8> = ['''+', '.join(str(b)+' olarak dtam8' for b in text.encode())+'''];
    eşleş fat32_yaz::dosya_yaz(disk, "/belgeler/merhaba.txt", veri) { hata(e) => { ver yanlış; } tamam(n) => { } }
'''
s+=f'''    eşleş konsol_yeni(40, 6, {fg}, {bg}) {{ hata(e) => {{ ver yanlış; }} tamam(kv) => {{ değişken k: Konsol = kv;
    değişken kl: KlavyeDurumu = klavye_yeni(); değişken sat: SatirDuzenleyici = satir_yeni();
    değişken baytlar: Dizi<tam32> = [{', '.join(map(str,sc))}];
    değişken komutlar: Dizi<metin> = ["{commands[0]}", "{commands[1]}"]; değişken adet: tam32 = 0; değişken i: tam32 = 0;
    iken i < dizi_boyut(baytlar) {{
        eşleş satir_olay(&değişken sat, klavye_bayt(&değişken kl, dizi_al(baytlar, i))) {{
            hiç => {{ }} değer(s) => {{
                eğer adet >= 2 {{ ver yanlış; }} eğer değil metin_esit(s, dizi_al(komutlar, adet)) {{ ver yanlış; }}
                konsol_metin(&değişken k, kabuk_calistir(disk, s)); adet = adet + 1;
            }}
        }} i = i + 1;
    }}
    eğer adet != 2 veya dizi_boyut(sat.kodlar) != 0 {{ ver yanlış; }}
    eğer değil metin_esit(konsol_satir(k, 0), "{'merhaba.txt'.ljust(40)}") {{ ver yanlış; }}
    eğer değil metin_esit(konsol_satir(k, 1), "{text.ljust(40)}") {{ ver yanlış; }}
    eğer değil metin_esit(konsol_satir(k, 2), "{' '*40}") {{ ver yanlış; }}
    eşleş psf_coz(gercek_font()) {{ hata(e) => {{ ver yanlış; }} tamam(fv) => {{ değişken f: PsfFont = fv;
    eşleş cerceve_yeni(320, 48, 1280) {{ hata(e) => {{ ver yanlış; }} tamam(cv) => {{ değişken c: Cerceve = cv;
    değişken yy: tam32 = 0;
    iken yy < k.satirlar {{ değişken xx: tam32 = 0; iken xx < k.sutunlar {{
        değişken yer: tam32 = yy * k.sutunlar + xx;
        kod_ciz(c, f, xx * f.genislik, yy * f.yukseklik, dizi_al(k.kodlar, yer), dizi_al(k.renkler, yer), dizi_al(k.arkalar, yer)); xx = xx + 1;
    }} yy = yy + 1; }}
'''
for y,b in enumerate(bitmap):
 for x in range(8):s+=f'    eşleş piksel_oku(c, {x}, {y}) {{ hiç => {{ ver yanlış; }} değer(v) => {{ eğer v != {fg if b&(128>>x) else bg} {{ ver yanlış; }} }} }}\n'
s+='    ver doğru;\n    } } } } } }'
c['masaustu']=s
for name,cmd,expected in [('echo','  echo  ğ😀','ğ😀'),('bilinmeyen','sil /x','bilinmeyen komut: sil'),('cat_arg','cat','kullanim: cat /yol')]:c[name]=f'    değişken b: Dizi<dtam8> = []; ver metin_esit(kabuk_calistir(b, "{cmd}"), bitir("{expected}"));'
c['bos']='    değişken b: Dizi<dtam8> = []; ver metin_esit(kabuk_calistir(b, "   "), "");'
c['eksik']='    değişken b: Dizi<dtam8> = imaj_yeni(); ver metin_esit(kabuk_calistir(b, "cat /yok"), bitir("hata: yok"));'
c['kok_liste']='    değişken b: Dizi<dtam8> = imaj_yeni(); ver metin_esit(kabuk_calistir(b, "ls"), "");'
testler('tests/drivers/sim/masaustu_sim_test.kem',header,c)
p=Path('Makefile');s=p.read_text();s=s.replace('tests/drivers/girdi/*_test.kem))','tests/drivers/girdi/*_test.kem tests/drivers/sim/*_test.kem))');p.write_text(s)
print(len(c),'test;',len(sc),'scancode baytı;',len(text.encode()),'dosya baytı; m glifi',list(bitmap))
