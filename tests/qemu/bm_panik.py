#!/usr/bin/env python3
"""Panikte tam piksel/seri, HLT ve ekran kapalıyken RAM kanaryası."""
from bekle import bekle, hlt_bekle, seri_oku, qmp_baglan
import sys,json,subprocess,os
from pathlib import Path
from psf_kahin import font_yukle,glif_ciz,screendump,karsilastir
elf=Path(sys.argv[1]).resolve();w=Path(sys.argv[2]).resolve()
mesaj='PANIK: kasitli ekran deneyi ğ'
for kip in os.environ.get('PANIK_KIPLER','acik ekransiz kapali').split():
 for n in range(1,int(os.environ.get('PANIK_TEKRAR','5'))+1):
  ser=w/'seri';q=w/'qmp';q.unlink(missing_ok=True);ser.unlink(missing_ok=True)
  p=subprocess.Popen(['qemu-system-x86_64','-M','q35','-m','256M','-display','none','-vga','std','-no-reboot','-serial',f'file:{ser}','-qmp',f'unix:{q},server=on,wait=off','-kernel',str(elf),'-append',kip],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
  sock=None
  try:
   sock=qmp_baglan(q,p,sure=10,okuma_suresi=5)
   f=sock.makefile('rwb',buffering=0);f.readline()
   def yanit():
    while True:
     d=json.loads(f.readline())
     if 'error' in d:raise RuntimeError(d['error'])
     if 'return' in d:return d['return']
   def komut(ad,args=None):
    d={'execute':ad}
    if args is not None:d['arguments']=args
    f.write(json.dumps(d).encode()+b'\n');return yanit()
   komut('qmp_capabilities')
   bekle('panik seri iletisi',lambda: seri_oku(ser),lambda b: b==(mesaj+'\n').encode(),surec=p)
   hlt_bekle(komut,p)
   if kip=='acik':
    font=font_yukle();img=[0xAA0000]*(640*480)
    for y,text in [(8,'PANIK'),(28,mesaj)]:
     for x,ch in enumerate(text):glif_ciz(img,640,480,font,8+x*8,y,ord(ch),0xFFFFFF,0xAA0000)
    gw,gh,px=screendump(f,w/'panik.ppm',yanit);assert (gw,gh)==(640,480)
    fark,ilk=karsilastir(px,img,640);assert not fark,(fark,ilk)
   if kip=='kapali':
    dst=w/'kanarya';komut('pmemsave',{'val':209846272,'size':8,'filename':str(dst)})
    assert int.from_bytes(dst.read_bytes(),'little')==0x12345678,'kapalı ekrana yazıldı; RAM kanaryası bozuldu'
   print(f'✓ panik {kip} {n}: seri birebir, HLT=1, ekran/kanarya doğru',flush=True)
  finally:
   if sock is not None: sock.close()
   if p.poll() is None:p.kill();p.wait()
