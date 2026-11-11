#!/usr/bin/env python3
"""USB klavyesiz gerçek QEMU i8042: denetim baytı ve tarama baytları."""
import sys,socket,json,subprocess,time,os
from pathlib import Path
elf=Path(sys.argv[1]).resolve();w=Path(sys.argv[2]).resolve();ser=w/'ps2.seri';q=w/'ps2.qmp';q.unlink(missing_ok=True)
p=subprocess.Popen(['qemu-system-x86_64','-M','q35','-m','256M','-display','none','-no-reboot','-serial',f'file:{ser}','-qmp',f'unix:{q},server=on,wait=off','-kernel',str(elf),'-device','isa-debug-exit,iobase=0xf4,iosize=0x04'],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
sock=socket.socket(socket.AF_UNIX)
try:
 son=time.monotonic()+30
 while not q.exists():
  if time.monotonic()>son:raise TimeoutError()
  time.sleep(.01)
 sock.settimeout(5);sock.connect(str(q));f=sock.makefile('rwb',buffering=0);f.readline()
 def yanit():
  while True:
   d=json.loads(f.readline())
   if 'error' in d:raise RuntimeError(d['error'])
   if 'return' in d:return d['return']
 def komut(ad,args=None):
  d={'execute':ad}
  if args is not None:d['arguments']=args
  f.write(json.dumps(d).encode()+b'\n');return yanit()
 komut('qmp_capabilities');schema=komut('query-qmp-schema')
 enum=next(x for x in schema if any(m.get('name')=='bracket_left' for m in x.get('members',[]) if isinstance(m,dict)))
 allowed={m['name'] for m in enum['members']}
 events=[('a',True),('a',False),('bracket_left',True),('bracket_left',False),('dot',True),('dot',False),('shift',True),('apostrophe',True),('apostrophe',False),('shift',False),('left',True),('left',False)]
 assert all(k in allowed for k,d in events)
 while b'PS2_HAZIR\n' not in (ser.read_bytes() if ser.exists() else b''):
  if p.poll() is not None or time.monotonic()>son:raise RuntimeError(ser.read_bytes() if ser.exists() else b'')
  time.sleep(.01)
 for k,d in events:
  komut('input-send-event',{'events':[{'type':'key','data':{'down':d,'key':{'type':'qcode','data':k}}}]});time.sleep(.03)
 p.wait(timeout=10);text=ser.read_text();print(text,end='');assert p.returncode==1,p.returncode
 # İlk gerçek QEMU ölçümü: 30/158=a, 26/154=ğ, 52/180=ç,
 # Shift 42/170 + 40/168=İ, E0 75/E0 203=sol; set 1.
 satirlar=text.splitlines();cfg=int(satirlar[0].split()[1]);assert cfg&0x73==0x60,cfg
 assert satirlar[1]=='PS2_HAZIR' and satirlar[-1]=='PS2_SON'
 assert list(map(int,satirlar[2:-1]))==[30,158,26,154,52,180,42,40,168,170,224,75,224,203],text
 print('PS2 SET1 OK: gerçek QEMU baytları ve XLATE')
finally:
 sock.close()
 if p.poll() is None:p.kill();p.wait()
