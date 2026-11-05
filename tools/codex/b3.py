from yardim import testler
header='''// Vektörler: python3 tools/codex/b3.py; byte sınırı len(s.encode()).
kullan drivers::fs::yol::{ yol_bol, yol_birlestir };
'''
cases={}
for i,(raw,expected) in enumerate([('/','/'),('///','/'),('/a//b/./c/../','/a/b'),('/../../a','/a'),('/a/b/../../c','/c'),('/a/../','/'),('/ğ/😀','/ğ/😀'),('/.abc/...','/.abc/...'),('/a/'+'x'*255,'/a/'+'x'*255),('/'+'ü'*127+'a','/'+'ü'*127+'a')]):
 # Bağımsız Python yığın normalizasyonu.
 stack=[]
 for x in raw.split('/'):
  if x in ('','.'):continue
  if x=='..':stack=stack[:-1]
  else:stack.append(x)
 assert '/'+ '/'.join(stack)==expected and all(len(x.encode())<=255 for x in stack)
 cases['yol_'+str(i)]=f'''    eşleş yol_bol("{raw}") {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken parcalar: Dizi<metin> = v;
        ver dizi_boyut(parcalar) == {len(stack)} ve metin_esit(yol_birlestir(parcalar), "{expected}");
    }} }}'''
for i,(raw,error) in enumerate([('','goreli'),('a/b','goreli'),('./a','goreli'),('/'+'x'*256,'uzun bilesen'),('/'+'ü'*128,'uzun bilesen')]):
 cases['hata_'+str(i)]=f'    eşleş yol_bol("{raw}") {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "{error}"); }} }}'
cases['birlestir']='''    değişken p: Dizi<metin> = ["a", "b"];
    değişken bos: Dizi<metin> = [];
    ver metin_esit(yol_birlestir(p), "/a/b") ve metin_esit(yol_birlestir(bos), "/");'''
testler('tests/drivers/fs/yol_test.kem',header,cases)
print(len(cases),'test; UTF8 sınırı',len(('ü'*128).encode()))
