from yardim import yaz,testler
from pathlib import Path
import re
# US ANSI set-1 fiziksel satırları; kod noktaları yalnız Python ord ile üretilir.
rows=[]
for codes,lo,hi in [
 (range(2,14),'1234567890-=','!@#$%^&*()_+'),
 (range(16,28),'qwertyuiop[]','QWERTYUIOP{}'),
 (range(30,42),"asdfghjkl;'`",'ASDFGHJKL:"~'),
 (range(44,54),'zxcvbnm,./','ZXCVBNM<>?'),
 ([43,57,86],'\\ \\','| |')]:
 assert len(codes)==len(lo)==len(hi)
 rows += [(sc,ord(a),ord(b),a.isalpha()) for sc,a,b in zip(codes,lo,hi)]
assert len(rows)==49
s='''// US ANSI set-1; tablo sabitleri python3 tools/codex/f3.py ile ord üzerinden hesaplanır.
kullan drivers::ps2::klavye_tr::{ KlavyeDurumu, TusOlayi, klavye_yeni };
kullan drivers::girdi::klavye_tablo::{ TusEsleme, duzen_bayt };
genel işlev us_tablo() -> Dizi<TusEsleme> {
    değişken xs: Dizi<TusEsleme> = [];
'''
for sc,a,b,letter in rows:s+=f'    dizi_ekle(xs, TusEsleme {{ tarama: {sc}, alt: {a}, ust: {b}, harf: {"doğru" if letter else "yanlış"} }});\n'
s+='''    ver xs;
}
genel işlev klavye_us_yeni() -> KlavyeDurumu { ver klavye_yeni(); }
genel işlev klavye_us_bayt(d: &değişken KlavyeDurumu, b: tam32) -> TusOlayi { ver duzen_bayt(d, b, us_tablo()); }
'''
yaz('drivers/girdi/klavye_us.kem',s)
header='''// Beklenti: python3 tools/codex/f3.py; US satır tablosu ve A1 Türkçe spesifikasyonu.
kullan drivers::ps2::klavye_tr::{ KlavyeDurumu, TusOlayi, klavye_yeni, klavye_bayt };
kullan drivers::girdi::klavye_tablo::{ TusEsleme, duzen_bayt };
kullan drivers::girdi::klavye_us::{ klavye_us_yeni, klavye_us_bayt };
işlev olay_kodu(o: TusOlayi) -> tam32 {
    eşleş o { TusOlayi::Yok => { ver 0; } TusOlayi::Karakter(k) => { ver k; }
        TusOlayi::Ozel(k, bas) => { eğer bas { ver 0 - k; } ver 0 - k - 100; } }
}
'''
c={}
for sc,a,b,letter in rows:
 c['us_'+str(sc)]=f'''    değişken d: KlavyeDurumu = klavye_us_yeni();
    eğer olay_kodu(klavye_us_bayt(&değişken d, {sc})) != {a} veya olay_kodu(klavye_us_bayt(&değişken d, {sc+128})) != 0 {{ ver yanlış; }}
    klavye_us_bayt(&değişken d, 42);
    eğer olay_kodu(klavye_us_bayt(&değişken d, {sc})) != {b} {{ ver yanlış; }}
    klavye_us_bayt(&değişken d, 58);
    eğer olay_kodu(klavye_us_bayt(&değişken d, {sc})) != {a if letter else b} {{ ver yanlış; }}
    klavye_us_bayt(&değişken d, 170);
    ver olay_kodu(klavye_us_bayt(&değişken d, {sc})) == {b if letter else a};'''
c['ozel']='''    değişken d: KlavyeDurumu = klavye_us_yeni(); klavye_us_bayt(&değişken d, 224);
    eğer olay_kodu(klavye_us_bayt(&değişken d, 75)) != -7 { ver yanlış; }
    klavye_us_bayt(&değişken d, 224); eğer olay_kodu(klavye_us_bayt(&değişken d, 203)) != -107 { ver yanlış; }
    ver olay_kodu(klavye_us_bayt(&değişken d, 14)) == -2 ve olay_kodu(klavye_us_bayt(&değişken d, 28)) == -1;'''
c['gecersiz']='''    değişken d: KlavyeDurumu = klavye_us_yeni(); klavye_us_bayt(&değişken d, 224);
    ver olay_kodu(klavye_us_bayt(&değişken d, 256)) == 0 ve değil d.e0 ve olay_kodu(klavye_us_bayt(&değişken d, -1)) == 0 ve olay_kodu(klavye_us_bayt(&değişken d, 0)) == 0;'''
c['parametre']=f'''    değişken d: KlavyeDurumu = klavye_yeni(); değişken xs: Dizi<TusEsleme> = [];
    dizi_ekle(xs, TusEsleme {{ tarama: 30, alt: {ord('Ω')}, ust: {ord('😀')}, harf: yanlış }});
    eğer olay_kodu(duzen_bayt(&değişken d, 30, xs)) != {ord('Ω')} {{ ver yanlış; }}
    duzen_bayt(&değişken d, 42, xs); ver olay_kodu(duzen_bayt(&değişken d, 30, xs)) == {ord('😀')};'''
c['tablo_sinir']='''    değişken d: KlavyeDurumu = klavye_yeni(); değişken xs: Dizi<TusEsleme> = [];
    eğer olay_kodu(duzen_bayt(&değişken d, 30, xs)) != 0 { ver yanlış; }
    dizi_ekle(xs, TusEsleme { tarama: 30, alt: 55296, ust: 1114112, harf: yanlış });
    eğer olay_kodu(duzen_bayt(&değişken d, 30, xs)) != 0 { ver yanlış; }
    değişken i: tam32 = 1; iken i < 129 { dizi_ekle(xs, TusEsleme { tarama: 30, alt: 65, ust: 66, harf: yanlış }); i=i+1; }
    ver olay_kodu(duzen_bayt(&değişken d, 30, xs)) == 0;'''
tr=[]
for line in Path('otomasyon/chatgpt/isler/ps2_klavye.md').read_text().splitlines():
 m=re.match(r'\| (\d+) \| `(.*?)` = (\d+) \| `(.*?)` = (\d+) \| (evet|hayır) \|',line)
 if m:
  sc,a,ka,b,kb,letter=m.groups();assert ord(a)==int(ka) and ord(b)==int(kb);tr.append((int(sc),int(ka),int(kb),letter=='evet'))
s='    değişken xs: Dizi<TusEsleme> = [];\n'
for sc,a,b,letter in tr:s+=f'    dizi_ekle(xs, TusEsleme {{ tarama: {sc}, alt: {a}, ust: {b}, harf: {"doğru" if letter else "yanlış"} }});\n'
s+='''    değişken a: KlavyeDurumu = klavye_yeni(); değişken b: KlavyeDurumu = klavye_yeni();
    değişken tur: tam32 = 0;
    iken tur < 4 {
        a.sol_shift = (tur & 1) != 0; b.sol_shift = a.sol_shift; a.caps = (tur & 2) != 0; b.caps = a.caps;
        değişken i: tam32 = 0;
        iken i < dizi_boyut(xs) { değişken t: TusEsleme = dizi_al(xs, i);
            eğer olay_kodu(klavye_bayt(&değişken a, t.tarama)) != olay_kodu(duzen_bayt(&değişken b, t.tarama, xs)) { ver yanlış; } i=i+1;
        } tur=tur+1;
    } ver doğru;'''
c['a1_esdeger']=s
testler('tests/drivers/girdi/klavye_us_test.kem',header,c)
print(len(c),'test; A1 dosyası değiştirilmedi, 49x4 Türkçe eşdeğerlik')
