from yardim import testler
from collections import deque
header='''// Beklentiler: python3 tools/codex/d8.py; bağımsız collections.deque FIFO modeli.
kullan drivers::girdi::halka::{ Halka, halka_yeni, it, al, bos_mu, sayi };
işlev esit(o: seçimlik<tam64>, n: tam64) -> mantıksal { eşleş o { hiç => { ver yanlış; } değer(v) => { ver v == n; } } }
işlev bos(o: seçimlik<tam64>) -> mantıksal { eşleş o { hiç => { ver doğru; } değer(v) => { ver yanlış; } } }
'''
def wrap(s,n=3):return f'    eşleş halka_yeni({n}) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken d: Halka = v;\n'+s+'\n    } }'
c={}
c['bos']=wrap('        ver bos_mu(d) ve sayi(d) == 0 ve d.kayip == 0 ve bos(al(&değişken d));')
c['fifo']=wrap('''        it(&değişken d, 4294967297 olarak tam64); it(&değişken d, -7 olarak tam64); it(&değişken d, 9223372036854775807 olarak tam64);
        ver esit(al(&değişken d), 4294967297 olarak tam64) ve esit(al(&değişken d), -7 olarak tam64) ve esit(al(&değişken d), 9223372036854775807 olarak tam64) ve bos_mu(d);''')
c['en_yeniyi_at']=wrap('''        it(&değişken d, 11 olarak tam64); it(&değişken d, 22 olarak tam64); it(&değişken d, 33 olarak tam64);
        eğer it(&değişken d, 44 olarak tam64) { ver yanlış; }
        ver d.kayip == 1 ve sayi(d) == 3 ve esit(al(&değişken d), 11 olarak tam64) ve esit(al(&değişken d), 22 olarak tam64) ve esit(al(&değişken d), 33 olarak tam64);''')
c['sifir']=wrap('''        ver değil it(&değişken d, 1 olarak tam64) ve bos(al(&değişken d)) ve bos_mu(d) ve d.kayip == 1;''',0)
c['bir']=wrap('''        değişken i: tam32 = 0;
        iken i < 100 { eğer değil it(&değişken d, i olarak tam64) { ver yanlış; } eğer değil esit(al(&değişken d), i olarak tam64) { ver yanlış; } i = i + 1; }
        ver bos_mu(d) ve d.kayip == 0;''',1)
c['kayip_doygun']=wrap('''        d.kayip = 9223372036854775807; it(&değişken d, 3 olarak tam64); ver d.kayip == 9223372036854775807;''',0)
for n in [-1,1048577]:c['gecersiz_'+str(abs(n))]=f'    eşleş halka_yeni({n}) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "kapasite"); }} }}'
c['ust_sinir']=wrap('        ver dizi_boyut(d.veri) == 1048576 ve sayi(d) == 0;',1048576)
q=deque();lost=0;lines=[];seed=13
for i in range(120):
 seed=(seed*1103515245+12345)&2147483647
 if seed%5<3:
  ok=len(q)<5
  if ok:q.append(i*4294967297)
  else:lost+=1
  lines.append(f'        eğer it(&değişken d, {i*4294967297} olarak tam64) != {"doğru" if ok else "yanlış"} {{ ver yanlış; }}')
 else:
  check=f'esit(al(&değişken d), {q.popleft()} olarak tam64)' if q else 'bos(al(&değişken d))'
  lines.append(f'        eğer değil {check} {{ ver yanlış; }}')
 lines.append(f'        eğer sayi(d) != {len(q)} veya d.kayip != {lost} {{ ver yanlış; }}')
c['model']=wrap('\n'.join(lines)+'\n        ver doğru;',5)
testler('tests/drivers/girdi/halka_test.kem',header,c);print(len(c),'test; model 120 işlem',lost,'kayıp')
