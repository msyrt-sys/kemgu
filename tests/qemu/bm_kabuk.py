#!/usr/bin/env python3
"""Gerçek CPU/USB klavye: üç disk türü, beş açılış, tam seri çıktı eşitliği."""
from bekle import bekle, gozle, konuk_sayaci, seri_oku, qmp_baglan, tam_seri
from protokol_suresi import BOSTA_GOZLEM
from pathlib import Path
import os,sys,json,time,subprocess,hashlib
from fat_yazici import hazirla,MERHABA,NOT,KAYDIR
from psf_kahin import font_yukle,glif_ciz,konsol,screendump,karsilastir,kabuk_goruntu
elf=Path(sys.argv[1]).resolve();w=Path(sys.argv[2]).resolve()
COMMANDS=['ls /','cat /merhaba.txt','cat /yok','echo Ağaç İş','cat /belgeler/not.txt','yanlis','cat /kaydir.txt']
OUTPUTS=[b'dalga8.txt\nbelgeler/\nmerhaba.txt\nkaydir.txt\n',MERHABA+b'\n',b'HATA bulunamadi\n','Ağaç İş\n'.encode(),NOT,b'HATA bilinmeyen komut\n',KAYDIR]
# qcodes ayrıca QMP query-qmp-schema ile çalışan QEMU'nun enum sözlüğünden doğrulanır.
def keys(c):
    special={'/':('7',True),'.':('slash',False),' ':('spc',False),'\n':('ret',False),'ğ':('bracket_left',False),'ç':('dot',False),'ş':('semicolon',False),'İ':('apostrophe',True),'i':('apostrophe',False),'ı':('i',False)}
    if c in special:return special[c]
    return c.lower(),c.isupper()
def boot(mode,n):
    disk=w/'disk.img';hazirla(disk,True,True);before=hashlib.sha256(disk.read_bytes()).hexdigest()
    serial=w/'seri.txt';qmp=w/'qmp.sock'
    qmp.unlink(missing_ok=True)
    cmd=['qemu-system-x86_64','-M','q35','-m','256M','-display','none','-no-reboot','-serial',f'file:{serial}','-kernel',str(elf),'-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-qmp',f'unix:{qmp},server=on,wait=off','-device','qemu-xhci,id=x','-device','usb-kbd,bus=x.0','-drive',f'file={disk},if=none,id=d0,format=raw']
    cmd+=['-device',{'nvme':'nvme,drive=d0,serial=kemgu8','ahci':'ide-hd,drive=d0,bus=ide.0','usb':'usb-storage,bus=x.0,drive=d0'}[mode]]
    if os.environ.get('KABUK_KLAVYE','usb')=='ps2':
        i=cmd.index('usb-kbd,bus=x.0');del cmd[i-1:i+1]
        if os.environ.get('KABUK_XHCI','1')=='0':
            assert mode!='usb'
            i=cmd.index('qemu-xhci,id=x');del cmd[i-1:i+1]
    deadline=time.monotonic()+60
    with (w/'qemu.log').open('w') as log:
        p=subprocess.Popen(cmd,stdout=log,stderr=log)
        sock=None
        try:
            sock=qmp_baglan(qmp,p,sure=60,okuma_suresi=5)
            f=sock.makefile('rwb',buffering=0)
            def response():
                while True:
                    d=json.loads(f.readline())
                    if 'error' in d:raise RuntimeError(d['error'])
                    if 'return' in d:return d['return']
                    if 'QMP' in d:return d
            def command(name,args=None):
                m={'execute':name}
                if args is not None:m['arguments']=args
                f.write(json.dumps(m).encode()+b'\n');return response()
            response();command('qmp_capabilities')
            schema=command('query-qmp-schema')
            (w/'qmp-schema.json').write_text(json.dumps(schema))
            enum=next(x for x in schema if any(m.get('name')=='bracket_left' for m in x.get('members',[]) if isinstance(m,dict)))
            allowed=set(enum.get('values',[]))|{x['name'] for x in enum.get('members',[])}
            for text in COMMANDS:
                for c in text+'\n':assert keys(c)[0] in allowed,(c,keys(c))
            assert {'shift','backspace','left','right'} <= allowed
            kabuk_basligi=b'KEMGU-OS kabuk v0\nDISK '+{'nvme':b'NVMe','ahci':b'AHCI','usb':b'USB'}[mode]+b'\nKABUK_HAZIR\n'
            # PVH'de LAPIC/MSI kurulmaz. AHCI sürücüsü yoklama yolunu seri hatta bildirir.
            expected=(b'AHCI YOL yoklama\n' if mode=='ahci' else b'')+kabuk_basligi
            def await_output(target):
                tam_seri('PVH kabuk',lambda:seri_oku(serial),target,p,deadline-time.monotonic())
            ps2 = os.environ.get('KABUK_KLAVYE','usb')=='ps2'
            if not ps2: halka=konuk_sayaci(w,command,'bh_bas',elf=elf)
            def key(k,down):
                if not ps2: ilk=halka()
                command('input-send-event',{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':k}}}]})
                if ps2:
                    def durum():
                        yanit=command('human-monitor-command',{'command-line':'i /b 0x64'})
                        return int(yanit.split()[-1],16)
                    bekle('i8042 çıkış tamponu boş',durum,lambda s:not s&1,surec=p)
                else:
                    bekle('PVH xHCI klavye tamamlaması',halka,lambda bas:bas!=ilk,surec=p,aralik=.002)
            await_output(expected)
            # [D-747] İlk açılışta kabuk BOŞTA bekler (gerçek kullanıcı hemen yazmaz). Sürücü
            # yoklamasıyla beklemek ~4 sn'de "xhci olay zaman asimi" veriyor ve her yoklama
            # heap sızdırıyordu; hemen yazan bir kâhin bunu GÖREMEZ.
            if n==1:
                sure=max(BOSTA_GOZLEM,float(os.environ.get('KABUK_BOSTA',BOSTA_GOZLEM)))
                deadline+=sure
                def sessiz():
                    assert seri_oku(serial)==expected, 'boşta seri değişti'
                gozle('PVH boşta kabuk',sure,sessiz,surec=p)
            akis=kabuk_basligi.decode('utf-8')
            def ekran(girdi='',imlec=None):
                img=kabuk_goruntu(akis,girdi,imlec);bas=time.monotonic()
                def fark():
                    gw,gh,px=screendump(f,w/'ekran.ppm',response)
                    assert (gw,gh)==(640,480)
                    return karsilastir(px,img,640)
                bekle(f'kabuk ekranı {girdi!r}',fark,lambda sonuc:sonuc[0]==0,sure=10,surec=p)
                return time.monotonic()-bas
            ekran()
            for text,out in zip(COMMANDS,OUTPUTS):
                for c in text:
                    k,shift=keys(c)
                    if shift:key('shift',True)
                    key(k,True);key(k,False)
                    if shift:key('shift',False)
                # Enter ÖNCESİ canlı yankı, bir fazlalık ve geri silme; seri hiç değişmez.
                ekran(text)
                if text==COMMANDS[0]:
                    key('left',True);key('left',False);ekran(text,len(text)-1)
                    key('right',True);key('right',False);ekran(text)
                key('x',True);key('x',False);ekran(text+'x')
                key('backspace',True);key('backspace',False);ekran(text)
                assert serial.read_bytes()==expected,'canlı yankı seriyi değiştirdi'
                key('ret',True);key('ret',False)
                expected+=out+b'KABUK_HAZIR\n';await_output(expected)
                akis+='> '+text+'\n'+out.decode('utf-8')+'KABUK_HAZIR\n'
                sure=ekran()
                if text=='cat /kaydir.txt':print(f'  70 satır kaydırma + screendump/model karşılaştırma {sure:.4f} s',flush=True)
            # Bump geriye gitmez: kalıcı + geçici tahsislerin üst sınırını QMP ile ölç.
            semboller={l.split()[-1]:int(l.split()[0],16) for l in subprocess.check_output(['nm','-n',str(elf)],text=True).splitlines() if len(l.split())==3}
            hp=w/'heap.bin';command('pmemsave',{'val':semboller['kem_bump'],'size':8,'filename':str(hp)})
            kullanilan=int.from_bytes(hp.read_bytes(),'little')-semboller['__heap_start']
            assert 0<kullanilan<16*1024*1024,f'16 MB test bütçesi aşıldı: {kullanilan}'
            print(f'  heap bump üst sınırı {kullanilan} bayt (<16 MiB)',flush=True)
            command('quit');p.wait(timeout=5)
            assert p.returncode==0
            assert hashlib.sha256(disk.read_bytes()).hexdigest()==before,'salt okuma diski değiştirdi'
            print(f'✓ kabuk {mode} klavye={os.environ.get("KABUK_KLAVYE","usb")} açılış {n}: 7 komut, canlı yankı/geri silme/imleç/70 satır, tam seri UTF-8, ekran 640x480 BİREBİR ve disk SHA256 aynı',flush=True)
        finally:
            if sock is not None: sock.close()
            if p.poll() is None:p.kill();p.wait()
            out=w
            if serial.exists():(out/f'kabuk-{mode}-{n}.seri').write_bytes(serial.read_bytes())
for mode in os.environ.get('KABUK_TURLER','nvme ahci usb').split():
    for n in range(1,int(os.environ.get('KABUK_TEKRAR','5'))+1):boot(mode,n)
print('BM_KABUK OK',flush=True)
