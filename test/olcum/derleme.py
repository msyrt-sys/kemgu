"""İki bootstrap yolunun süre, tepe RSS ve LLVM IR büyüklüklerini ölçer."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

from birim_uret import uret

OLCUTLER = ('saniye','rss_kib','ir_satir','en_buyuk_ir')
BAS = '<!-- derleme-olcum-bas -->'
SON = '<!-- derleme-olcum-son -->'


def olc(derleyici, kaynak, dizin):
    ir = dizin/f'{derleyici}-{kaynak.stem}.ll'
    sure = ir.with_suffix('.time')
    with ir.open('wb') as f:
        bas=time.perf_counter()
        subprocess.run(['/usr/bin/time','-v','-o',str(sure),f'build/{derleyici}',
                        '--llvm','--mimari','x86_64-none',str(kaynak)],
                       stdout=f,env=dict(os.environ,LC_ALL='C'),check=True)
        gecen=time.perf_counter()-bas
    rss=int(re.search(r'Maximum resident set size \(kbytes\): (\d+)',sure.read_text(encoding='utf-8'))[1])
    boy=en_buyuk=0
    ad=buyuk_ad=None
    with ir.open(encoding='utf-8') as f:
        for toplam,line in enumerate(f,1):
            if line.startswith('define '):
                ad=re.search(r'@([^ (]+)\(',line)[1]
                boy=0
            if ad is not None:
                boy+=1
                if line.startswith('}'):
                    if boy>en_buyuk:
                        en_buyuk, buyuk_ad=boy,ad
                    ad=None
    assert en_buyuk>0 and toplam>=en_buyuk and rss>0
    return dict(saniye=round(gecen,4),rss_kib=rss,ir_satir=toplam,
                en_buyuk_ir=en_buyuk,en_buyuk_islev=buyuk_ad)


def tablo(satirlar):
    txt=['## Derleme büyüme ölçümü','',
         '| Birim | Kaynak satırı | Derleyici | Süre (s) | Tepe RSS (KiB) | IR satırı | En büyük işlev (IR satırı) |',
         '|---|---:|---|---:|---:|---:|---|']
    for r in satirlar:
        txt.append('| {birim} | {kaynak_satiri} | {derleyici} | {saniye:.4f} | {rss_kib} | {ir_satir} | {en_buyuk_islev} ({en_buyuk_ir}) |'.format(**r))
    return '\n'.join(txt)


def rapor_yaz(yol,icerik):
    once=yol.read_text(encoding='utf-8') if yol.exists() else ''
    bolum=BAS+'\n'+icerik+'\n'+SON
    if BAS in once:
        a=once.index(BAS)
        b=once.index(SON,a)+len(SON)
        yeni=once[:a]+bolum+once[b:]
    else:
        yeni=once.rstrip()+'\n\n'+bolum+'\n'
    gecici=yol.with_name(yol.name+f'.{os.getpid()}.tmp')
    gecici.write_text(yeni,encoding='utf-8')
    gecici.replace(yol)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('cekirdek',type=Path)
    p.add_argument('dizin',type=Path)
    p.add_argument('--esikler',type=Path,default=Path('test/olcum/esikler.json'))
    # [D-767] Varsayılan rapor ÖLÇÜM DİZİNİNE: kapı her koşumda izlenen
    # otomasyon/chatgpt/codex16_bulgular.md'yi yeniden yazıyordu (kirli ağaç).
    # Belgeyi bilerek güncellemek için: --rapor otomasyon/chatgpt/codex16_bulgular.md
    p.add_argument('--rapor',type=Path,default=None)
    a=p.parse_args()
    a.dizin.mkdir(parents=True,exist_ok=True)
    kaynaklar={}
    for n in (5000,10000,20000):
        kaynak=a.dizin/f'surucu_{n}.kem'
        uret(n,kaynak)
        kaynaklar[str(n)]=kaynak
    kaynaklar['kabuk']=a.cekirdek
    esikler=json.loads(a.esikler.read_text(encoding='utf-8'))
    satirlar=[]
    hatalar=[]
    for ad,kaynak in kaynaklar.items():
        n=len(kaynak.read_text(encoding='utf-8').splitlines())
        for derleyici in ('kemgu','codegen'):
            r=olc(derleyici,kaynak,a.dizin)
            satirlar.append(dict(birim=ad,kaynak_satiri=n,derleyici=derleyici,**r))
            for olcut in OLCUTLER:
                tavan=esikler[ad][derleyici][olcut]
                if r[olcut]>tavan:
                    hatalar.append(f'{ad} {derleyici} {olcut} {r[olcut]} > {tavan}')
    (a.dizin/'olcum.json').write_text(json.dumps(satirlar,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    metin=tablo(satirlar)
    print(metin,flush=True)
    # [D-767] EVREN bulgusu: KIRMIZI durumda da değerler rapora yazılır.
    rapor_yaz(a.rapor or (a.dizin/'rapor.md'),metin)
    if hatalar:
        raise SystemExit('DERLEME RED\n'+'\n'.join(hatalar))


if __name__ == '__main__':
    main()
