"""Sabit satır bütçesinde aygıt durumu, komut ve halka erişimleri üretir."""
from pathlib import Path
import argparse

BASLIK = """// Sentetik sürücü ölçümü. Birimler aynı tohumla aynı kaynak baytlarını üretir.
çeşit Emir { Oku(tam32), Yaz(tam32), Sifirla }
yapı Aygit { kayit: Dizi<tam32>; sayac: tam32; son: tam32; }
"""


def islev(no):
    ofset = (no * 17 + 3) % 32
    govde = [
        f'işlev aygit_{no}_isle(d: &değişken Aygit, e: Emir) -> tam32 {{',
        f'    değişken yuva: tam32 = (d.sayac+{ofset})&31;',
        '    değişken veri: tam32 = dizi_al(d.kayit,yuva);',
        '    eşleş e {',
        '        Emir::Oku(i) => {',
        '            veri=dizi_al(d.kayit,(yuva+i)&31);',
        '        }',
        '        Emir::Yaz(v) => {',
        '            veri=(veri^v)&65535;',
        '            dizi_yaz(d.kayit,yuva,veri);',
        '        }',
        '        Emir::Sifirla => {',
        '            dizi_yaz(d.kayit,yuva,0);',
        '            veri=0;',
        '        }',
        '    }',
        '    d.son=veri;',
        '    d.sayac=(d.sayac+1)&65535;',
        '    ver veri;',
        '}',
    ]
    return govde


def uret(hedef, yol):
    adet = max(1,(hedef-13)//(len(islev(0))+1))
    satirlar = BASLIK.splitlines()
    for no in range(adet):
        satirlar.extend(islev(no))
    satirlar.extend([
        'işlev main() -> tam32 {',
        '    değişken k: Dizi<tam32> = []; değişken i: tam32 = 0;',
        '    iken i < 32 { dizi_ekle(k,i); i=i+1; }',
        '    değişken d: Aygit = Aygit {kayit:k,sayac:0,son:0};',
        '    değişken r: tam32 = 0;',
    ])
    for no in range(adet):
        emir = (f'Emir::Oku({no%32})', f'Emir::Yaz({no%65536})', 'Emir::Sifirla')[no%3]
        satirlar.append(f'    r=r^aygit_{no}_isle(&değişken d,{emir});')
    satirlar.extend(['    ver r&255;', '}'])
    yol.write_text('\n'.join(satirlar)+'\n',encoding='utf-8')


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('satir',type=int,choices=(5000,10000,20000))
    p.add_argument('yol',type=Path)
    a=p.parse_args()
    uret(a.satir,a.yol)
