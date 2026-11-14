#!/usr/bin/env python3
"""Gerçek #DE/#UD/#GP/#PF/#DF, OOB; bağımsız seri/PSF, IST ve HLT kâhini."""
import os,sys,json,socket,time,subprocess,re,struct
from pathlib import Path
from psf_kahin import font_yukle,glif_ciz,screendump,karsilastir
elf=Path(sys.argv[1]).resolve();w=Path(sys.argv[2]).resolve()
symbols={v[2]:int(v[0],16) for l in subprocess.check_output(['nm','-n',str(elf)],text=True).splitlines() if len(v:=l.split())==3}
for kip in os.environ.get('ISTISNA_KIPLER','de ud gp pf df oob eksi').split():
 for n in range(1,int(os.environ.get('ISTISNA_TEKRAR','10'))+1):
  ser=w/'seri';q=w/'qmp';q.unlink(missing_ok=True);ser.unlink(missing_ok=True)
  p=subprocess.Popen(['qemu-system-x86_64','-M','q35','-m','256M','-display','none','-vga','std','-no-reboot','-serial',f'file:{ser}','-qmp',f'unix:{q},server=on,wait=off','-kernel',str(elf),'-append',kip],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
  sock=socket.socket(socket.AF_UNIX)
  try:
   end=time.monotonic()+10
   while not q.exists():
    assert time.monotonic()<end and p.poll() is None,'QMP başlamadı'
    time.sleep(.01)
   sock.settimeout(3);sock.connect(str(q));f=sock.makefile('rwb',buffering=0);f.readline()
   def yanit():
    while True:
     d=json.loads(f.readline());assert 'error' not in d,d
     if 'return' in d:return d['return']
   def komut(ad,args=None):
    d={'execute':ad}
    if args is not None:d['arguments']=args
    f.write(json.dumps(d).encode()+b'\n');return yanit()
   komut('qmp_capabilities')
   def dump(a,n):
    dst=w/'dump';komut('pmemsave',{'val':a,'size':n,'filename':str(dst)});return dst.read_bytes()
   if kip in ('oob','eksi'):
    msg=f'dizi sinir ihlali (i={7 if kip=="oob" else -2}, boyut=3)';expected='K11_BOOT\n'+msg+'\n'
   else:
    vec={'de':0,'ud':6,'gp':13,'pf':14,'df':8}[kip];rip=symbols['k11_'+kip+'_fault']
    hata=0x38 if kip=='gp' else 0 # GDT: yedi 8-bayt yuva; ilk dış seçici 7*8.
    cr2={'pf':0x60001000,'df':0x60000ff8}.get(kip,0)
    msg=f'ISTISNA vek=0x{vec:x} hata=0x{hata:x} rip=0x{rip:x} cr2=0x{cr2:x} #{kip.upper()}'
    expected=f'K11_BOOT\nISTISNA tip=0x{vec:x} a=0x{hata:x} b=0x{rip:x} adr=0x{cr2:x}\n'+msg+'\n'
   while (ser.read_text() if ser.exists() else '')!=expected:
    assert p.poll() is None and time.monotonic()<end,(kip,p.poll(),ser.read_text() if ser.exists() else '',expected)
    time.sleep(.02)
   time.sleep(.05)
   regs=komut('human-monitor-command',{'command-line':'info registers'})
   assert 'HLT=1' in regs,regs
   assert ser.read_text().count('K11_BOOT\n')==1,'yeniden açıldı'
   if kip=='df':
    rsp=int(re.search(r'RSP=([0-9a-f]+)',regs)[1],16)
    base=symbols['kdl_ist1_alt'];top=symbols['kdl_ist1_ust']
    assert base+512<rsp<top,(hex(rsp),hex(base),hex(top))
   for i in (1,2,3):
    assert dump(symbols[f'kdl_ist{i}_alt'],8)==struct.pack('<Q',0x4b454d4755495354),'IST kanaryası'
   assert dump(symbols['kem_bump'],8)==bytes(8) and dump(symbols['kem_son'],8)==bytes(8),'panikte heap değişti'
   font=font_yukle();img=[0xAA0000]*(1024*768)
   for y,text in [(8,'PANIK'),(28,msg)]:
    for x,ch in enumerate(text):glif_ciz(img,1024,768,font,8+x*8,y,ord(ch),0xFFFFFF,0xAA0000)
   gw,gh,px=screendump(f,w/'panik.ppm',yanit);assert (gw,gh)==(1024,768)
   diff,first=karsilastir(px,img,1024);assert not diff,(diff,first)
   assert p.poll() is None and ser.read_text()==expected
   print(f'✓ istisna {kip} #{n}: tek açılış, tam seri/piksel, HLT, IST kanaryaları',flush=True)
  finally:
   sock.close()
   if p.poll() is None:p.kill();p.wait()
