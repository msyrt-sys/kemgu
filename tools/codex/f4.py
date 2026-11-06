"""Yerel Python Unicode verisinden kod noktası dönüşümleri; Türkçe I kuralları açık."""
from pathlib import Path
import unicodedata
from yardim import yaz,testler
assert unicodedata.unidata_version == "15.0.0", "Bu fikstür Python Unicode 15.0.0 verisiyle yeniden üretilir"
def up(s):return ''.join('İ' if c=='i' else c.upper() for c in s)
def down(s):return ''.join('ı' if c=='I' else 'i' if c=='İ' else c.lower() for c in s)
upper=[];lower=[]
for cp in range(0x110000):
 if 0xd800<=cp<=0xdfff:continue
 c=chr(cp);u=up(c);l=down(c)
 if u!=c:upper.append((cp,u))
 if l!=c:lower.append((cp,l))
s=f'// Üretim: python3 tools/codex/f4.py; Python Unicode {unicodedata.unidata_version}, Türkçe i/I istisnaları.\n'
s+='genel yapı HarfEsleme { kod: tam32; metin: metin; }\n'
for fn,rows in [('ust_eslemeler',upper),('alt_eslemeler',lower)]:
 s+=f'genel işlev {fn}() -> Dizi<HarfEsleme> {{\n    değişken xs: Dizi<HarfEsleme> = [];\n'
 for cp,out in rows:
  assert '"' not in out and '\\' not in out and '\n' not in out
  s+=f'    dizi_ekle(xs, HarfEsleme {{ kod: {cp}, metin: "{out}" }});\n'
 s+='    ver xs;\n}\n'
yaz('drivers/ortak/harf_veri.kem',s)
header=f'''// Kahin: python3 tools/codex/f4.py; Python Unicode {unicodedata.unidata_version}, Türkçe özel çiftleri.
kullan drivers::ortak::harf_veri::{{ HarfEsleme }};
kullan drivers::ortak::harf::{{ buyuk_harf, kucuk_harf }};
kullan drivers::fs::yol_harf::{{ yol_esit }};
'''
c={}
for i,text in enumerate(['iİıI','çğıöşü ÇĞIÖŞÜ','istanbul IĞDIR İZMİR','abcXYZ','Straße ﬃ','Σσς','ЯБВ абв','𐐨𐐀','😀 123!?','']):
 c['donus_'+str(i)]=f'    ver metin_esit(buyuk_harf("{text}"), "{up(text)}") ve metin_esit(kucuk_harf("{text}"), "{down(text)}");'
# Her eşlemeyi tek büyük metinde kontrol et; geçerli Unicode değişmezleri de araya eklenir.
for name,rows,fn,convert in [('tum_ust',upper,'buyuk_harf',up),('tum_alt',lower,'kucuk_harf',down)]:
 raw='|'.join(chr(cp) for cp,_ in rows);expected=convert(raw)
 c[name]=f'    ver metin_esit({fn}("{raw}"), "{expected}");'
c['bozuk_utf8']='''    değişken s: metin = metin_kes("ğ", 0, 1); ver metin_esit(buyuk_harf(s), "�") ve metin_esit(kucuk_harf(s), "�");'''
for i,(a,b,mode,expected) in enumerate([('/İZMİR/./a','//izmir/a',True,True),('/IĞDIR','/ığdır',True,True),('/I','/i',True,False),('/İ','/ı',True,False),('/Ğ','/ğ',False,False),('/x/../a','/a',False,True),('/ß','/ss',True,False),('/é','/é',True,False)]):
 c['yol_'+str(i)]=f'    eşleş yol_esit("{a}", "{b}", {"doğru" if mode else "yanlış"}) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ ver v == {"doğru" if expected else "yanlış"}; }} }}'
c['yol_red']='''    eşleş yol_esit("goreli", "/goreli", doğru) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "goreli"); } }'''
testler('tests/drivers/ortak/harf_test.kem',header,c)
print('Unicode',unicodedata.unidata_version,len(upper),'üst,',len(lower),'alt eşleme;',len(c),'test')
