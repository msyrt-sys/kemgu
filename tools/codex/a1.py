from yardim import yaz, testler
import re
from pathlib import Path
spec=Path('otomasyon/chatgpt/isler/ps2_klavye.md').read_text(encoding='utf-8')
rows=[]
for line in spec.splitlines():
    m=re.match(r'\| (\d+) \| `(.*?)` = (\d+) \| `(.*?)` = (\d+) \| (evet|hayır) \|',line)
    if m:
        sc,a,ka,b,kb,letter=m.groups()
        assert ord(a)==int(ka) and ord(b)==int(kb)
        rows.append((int(sc),ord(a),ord(b),letter=='evet'))
assert len(rows)==49
s='''// Türkçe Q, set 1: yalnız saf bayt çözümü.
genel yapı KlavyeDurumu {
    sol_shift: mantıksal; sag_shift: mantıksal; ctrl: mantıksal; alt: mantıksal;
    altgr: mantıksal; caps: mantıksal; e0: mantıksal;
}
genel çeşit TusOlayi { Yok, Karakter(tam32), Ozel(tam32, mantıksal) }
'''
for i,name in enumerate(['ENTER','GERI','TAB','ESC','YUKARI','ASAGI','SOL','SAG','SIL'],1):
    s+=f'genel sabit OZEL_{name}: tam32 = {i};\n'
s+='''
genel işlev klavye_yeni() -> KlavyeDurumu {
    ver KlavyeDurumu { sol_shift: yanlış, sag_shift: yanlış, ctrl: yanlış,
        alt: yanlış, altgr: yanlış, caps: yanlış, e0: yanlış };
}

genel işlev klavye_bayt(d: &değişken KlavyeDurumu, b: tam32) -> TusOlayi {
    eğer b < 0 veya b > 255 { d.e0 = yanlış; ver TusOlayi::Yok; }
    eğer b == 224 { d.e0 = doğru; ver TusOlayi::Yok; }
    değişken uzatilmis: mantıksal = d.e0;
    d.e0 = yanlış;
    değişken basildi: mantıksal = b < 128;
    değişken k: tam32 = b & 127;
    eğer k == 29 { d.ctrl = basildi; ver TusOlayi::Yok; }
    eğer k == 56 {
        eğer uzatilmis { d.altgr = basildi; } değilse { d.alt = basildi; }
        ver TusOlayi::Yok;
    }
    değişken ozel: tam32 = 0;
    eğer uzatilmis {
        eğer k == 72 { ozel = OZEL_YUKARI; }
        eğer k == 80 { ozel = OZEL_ASAGI; }
        eğer k == 75 { ozel = OZEL_SOL; }
        eğer k == 77 { ozel = OZEL_SAG; }
        eğer k == 83 { ozel = OZEL_SIL; }
        eğer k == 28 { ozel = OZEL_ENTER; }
        eğer ozel != 0 { ver TusOlayi::Ozel(ozel, basildi); }
        ver TusOlayi::Yok;
    }
    eğer k == 42 { d.sol_shift = basildi; ver TusOlayi::Yok; }
    eğer k == 54 { d.sag_shift = basildi; ver TusOlayi::Yok; }
    eğer k == 58 {
        eğer basildi { d.caps = değil d.caps; }
        ver TusOlayi::Yok;
    }
    eğer k == 28 { ozel = OZEL_ENTER; }
    eğer k == 14 { ozel = OZEL_GERI; }
    eğer k == 15 { ozel = OZEL_TAB; }
    eğer k == 1 { ozel = OZEL_ESC; }
    eğer ozel != 0 { ver TusOlayi::Ozel(ozel, basildi); }
    eğer değil basildi { ver TusOlayi::Yok; }
    değişken shift: mantıksal = d.sol_shift veya d.sag_shift;
    değişken buyuk: mantıksal = shift != d.caps;
'''
for sc,a,b,letter in rows:
    s+=f'    eğer k == {sc} {{ eğer {"buyuk" if letter else "shift"} {{ ver TusOlayi::Karakter({b}); }} ver TusOlayi::Karakter({a}); }}\n'
s+='    ver TusOlayi::Yok;\n}\n'
yaz('drivers/ps2/klavye_tr.kem',s)
header='''// Sabitler: python3 tools/codex/a1.py — ord(karakter), enumerate ve make + 128.
kullan drivers::ps2::klavye_tr::{ KlavyeDurumu, TusOlayi, klavye_yeni, klavye_bayt };
işlev karakter(o: TusOlayi, k: tam32) -> mantıksal {
    eşleş o { TusOlayi::Karakter(v) => { ver v == k; } _ => { ver yanlış; } }
}
işlev yok(o: TusOlayi) -> mantıksal {
    eşleş o { TusOlayi::Yok => { ver doğru; } _ => { ver yanlış; } }
}
işlev ozel(o: TusOlayi, k: tam32, bas: mantıksal) -> mantıksal {
    eşleş o { TusOlayi::Ozel(v, b) => { ver v == k ve b == bas; } _ => { ver yanlış; } }
}
'''
cases={}
for sc,a,b,letter in rows:
    body='    değişken d: KlavyeDurumu = klavye_yeni();\n'
    body+=f'    eğer değil karakter(klavye_bayt(&değişken d, {sc}), {a}) {{ ver yanlış; }}\n'
    body+=f'    eğer değil yok(klavye_bayt(&değişken d, {sc+128})) {{ ver yanlış; }}\n'
    for caps,shift in [(False,True),(True,True),(True,False),(False,False)]:
        expected=b if ((shift != caps) if letter else shift) else a
        body+=f'    d.caps = {"doğru" if caps else "yanlış"}; d.sol_shift = {"doğru" if shift else "yanlış"};\n'
        body+=f'    eğer değil karakter(klavye_bayt(&değişken d, {sc}), {expected}) {{ ver yanlış; }}\n'
    cases[f'tablo_{sc}']=body+'    ver doğru;'
cases['shift']='''    değişken d: KlavyeDurumu = klavye_yeni();
    eğer değil yok(klavye_bayt(&değişken d, 42)) { ver yanlış; }
    eğer değil karakter(klavye_bayt(&değişken d, 30), 65) { ver yanlış; }
    klavye_bayt(&değişken d, 54);
    klavye_bayt(&değişken d, 170);
    eğer değil karakter(klavye_bayt(&değişken d, 30), 65) { ver yanlış; }
    klavye_bayt(&değişken d, 182);
    ver karakter(klavye_bayt(&değişken d, 30), 97);'''
cases['caps']='''    değişken d: KlavyeDurumu = klavye_yeni();
    klavye_bayt(&değişken d, 58); klavye_bayt(&değişken d, 186);
    eğer değil d.caps { ver yanlış; }
    eğer değil karakter(klavye_bayt(&değişken d, 40), 304) { ver yanlış; }
    klavye_bayt(&değişken d, 42);
    eğer değil karakter(klavye_bayt(&değişken d, 30), 97) { ver yanlış; }
    klavye_bayt(&değişken d, 58);
    ver değil d.caps;'''
for sc,key,extended in [(28,1,False),(14,2,False),(15,3,False),(1,4,False),(72,5,True),(80,6,True),(75,7,True),(77,8,True),(83,9,True)]:
    b='    değişken d: KlavyeDurumu = klavye_yeni();\n'
    for pressed in (True,False):
        if extended: b+='    eğer değil yok(klavye_bayt(&değişken d, 224)) { ver yanlış; }\n'
        b+=f'    eğer değil ozel(klavye_bayt(&değişken d, {sc if pressed else sc+128}), {key}, {"doğru" if pressed else "yanlış"}) {{ ver yanlış; }}\n'
        b+='    eğer d.e0 { ver yanlış; }\n'
    cases[f'ozel_{key}']=b+'    ver karakter(klavye_bayt(&değişken d, 30), 97);'
cases['degistiriciler']='''    değişken d: KlavyeDurumu = klavye_yeni();
    klavye_bayt(&değişken d, 29); klavye_bayt(&değişken d, 56);
    klavye_bayt(&değişken d, 224); klavye_bayt(&değişken d, 56);
    eğer değil d.ctrl veya değil d.alt veya değil d.altgr { ver yanlış; }
    eğer değil karakter(klavye_bayt(&değişken d, 30), 97) { ver yanlış; }
    klavye_bayt(&değişken d, 224); klavye_bayt(&değişken d, 184);
    klavye_bayt(&değişken d, 224); klavye_bayt(&değişken d, 157);
    klavye_bayt(&değişken d, 184);
    ver değil d.ctrl ve değil d.alt ve değil d.altgr ve değil d.e0;'''
cases['bilinmeyen']='''    değişken d: KlavyeDurumu = klavye_yeni();
    eğer değil yok(klavye_bayt(&değişken d, 100)) { ver yanlış; }
    klavye_bayt(&değişken d, 224);
    eğer değil yok(klavye_bayt(&değişken d, 30)) { ver yanlış; }
    eğer değil yok(klavye_bayt(&değişken d, -1)) { ver yanlış; }
    eğer değil yok(klavye_bayt(&değişken d, 256)) { ver yanlış; }
    ver karakter(klavye_bayt(&değişken d, 30), 97) ve değil d.e0;'''
testler('tests/drivers/ps2/klavye_tr_test.kem',header,cases)
print('A1',len(cases),'test; tablo Unicode değerleri ord ile doğrulandı')
