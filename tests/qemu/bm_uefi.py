#!/usr/bin/env python3
"""Gerçek OVMF: EFI yükleme, ABI ve kendi CR3/GDT'si; KEMGU'dan bağımsız kâhin."""
from pathlib import Path
import os,sys,shutil,subprocess,time,socket,json,struct,re,uuid
from psf_kahin import font_yukle, glif_ciz, screendump, karsilastir
from acpi_uefi_kahin import denetle as acpi_denetle
from fat_yazici import hazirla
from uefi_kabuk_kahin import denetle as kabuk_denetle
W=Path(sys.argv[1]).resolve()
KABUK=os.environ.get('UEFI_KABUK')=='1'

def boot(n=1,negative=False,ram='256M'):
    q=W/'qmp.sock';ser=W/'seri.txt';q.unlink(missing_ok=True);ser.unlink(missing_ok=True)
    shutil.copy('/usr/share/OVMF/OVMF_VARS_4M.fd',W/'vars.fd')
    cmd=['qemu-system-x86_64','-M','q35','-m',ram,'-vga','std','-display','none','-no-reboot',
         '-drive','if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd',
         '-drive',f'if=pflash,format=raw,file={W}/vars.fd','-drive',f'format=raw,file=fat:rw:{W}/esp',
         '-serial',f'file:{ser}','-qmp',f'unix:{q},server=on,wait=off',
         '-device','isa-debug-exit,iobase=0xf4,iosize=0x04']
    if KABUK:
        hazirla(W/'disk.img',True,True)
        cmd+=['-device','qemu-xhci,id=x','-device','usb-kbd,bus=x.0','-drive',f'file={W}/disk.img,if=none,id=d0,format=raw','-device','nvme,drive=d0,serial=kemgu10']
    with (W/'qemu.log').open('w') as log:
        p=subprocess.Popen(cmd,stdout=log,stderr=log);s=socket.socket(socket.AF_UNIX);limit=time.monotonic()+60
        try:
            while not q.exists():
                assert p.poll() is None and time.monotonic()<limit,'QEMU başlatılamadı'
                time.sleep(.02)
            s.settimeout(10);s.connect(str(q));f=s.makefile('rwb',buffering=0)
            def response():
                while True:
                    line=f.readline();assert line,'QMP kapandı';d=json.loads(line)
                    if 'error' in d:raise AssertionError(d['error'])
                    if 'return' in d:return d['return']
            f.readline()
            def command(name,args=None):
                d={'execute':name}
                if args is not None:d['arguments']=args
                f.write(json.dumps(d).encode()+b'\n');return response()
            command('qmp_capabilities')
            target=('EFI HATA '+negative).encode() if negative else (b'KABUK_HAZIR' if KABUK else b'UEFI_HAZIR')
            while target not in (data:=ser.read_bytes() if ser.exists() else b''):
                if p.poll() is not None or time.monotonic()>limit:raise AssertionError(f'hedef yok: {target!r}, rc={p.poll()}, seri={data!r}')
                if not negative and b'EFI HATA' in data:raise AssertionError(data)
                time.sleep(.05)
            if negative:
                assert b'KEMGU-OS UEFI' not in data
                print(f'✓ UEFI negatif: {negative}',flush=True);return
            text=data.decode('utf-8','replace');assert 'KEMGU-OS UEFI\n' in text
            addr=int(re.search(r'BILGI_ADRES (\d+)',text)[1]);assert addr%4096==0
            def dump(a,n,path):
                command('pmemsave',{'val':a,'size':n,'filename':str(path)});return path.read_bytes()
            raw=dump(addr,192,W/'bilgi.bin');fields=struct.unpack('<24Q',raw)
            vals={int(i):int(v)&((1<<64)-1) for i,v in re.findall(r'BILGI (\d+) (-?\d+)',text)}
            assert vals==dict(enumerate(fields)),(vals,fields)
            assert fields[:3]==(int.from_bytes(b'KEMUEFI1','little'),192,1)
            elf=(W/'k.elf').read_bytes();ph=struct.unpack_from('<Q',elf,32)[0];count=struct.unpack_from('<H',elf,56)[0]
            seg=[struct.unpack_from('<II6Q',elf,ph+56*i) for i in range(count)]
            load=[s for s in seg if s[0]==1]
            assert fields[14]==min(x[4]&~4095 for x in load)
            assert fields[15]==max((x[4]+x[6]+4095)&~4095 for x in load)
            assert fields[11]>=40 and fields[10]%fields[11]==0 and fields[12]==1
            mmap=dump(fields[9],fields[10],W/'harita.bin')
            types=sorted({struct.unpack_from('<I',mmap,i)[0] for i in range(0,len(mmap),fields[11])})
            alloc=[(name,int(a),int(n)) for name,a,n in re.findall(r'TAHSIS (\w+) (\d+) (\d+)',text)]
            assert {'AYNA','PANIK','DMA','VIRTIO'}<={x[0] for x in alloc} and {x[0] for x in alloc}<={'AYNA','PANIK','DMA','VIRTIO','TABLO'},alloc
            reserved=[(fields[14],fields[15]),(addr,addr+4096),(fields[9],fields[9]+fields[16]),(fields[3],fields[3]+fields[4]),(fields[13],fields[13]+4096),(fields[20],fields[21])]
            descriptors=[struct.unpack_from('<IIQQQQ',mmap,i) for i in range(0,len(mmap),fields[11])]
            for name,a,size in alloc:
                assert a%4096==0 and size%4096==0 and a>=1048576 and a+size<=2**32
                assert all(a+size<=lo or a>=hi for lo,hi in reserved),(name,a,size,reserved)
                usable=sorted((d[2],d[2]+d[4]*4096) for d in descriptors if d[0] in (3,4,7) and not d[5]&(1<<63))
                pos=a
                for lo,hi in usable:
                    if lo<=pos<hi:pos=hi
                assert pos>=a+size,(name,a,size,usable)
            for i,(_,a,size) in enumerate(alloc):
                assert all(a+size<=b or a>=b+n for _,b,n in alloc[i+1:])
            guard=re.search(r'KORUMA (\d+) (\d+)',text)
            if guard:
                a,b=map(int,guard.groups())
                assert dump(a,8,W/'koruma.bin')==b'KORUMA10','ayrılmış sayfa ezildi'
                assert dump(b,8,W/'ayrilan.bin')==b'YAZILDI!' and a!=b
            print(f'✓ RAM {ram}: fiziksel tahsisler ve korunan bölgeler {alloc}',flush=True)
            rsdp=dump(fields[13],36,W/'rsdp.bin');assert rsdp[:8]==b'RSD PTR ' and sum(rsdp[:20])%256==0
            if rsdp[15]>=2:assert sum(rsdp)%256==0
            acpi_denetle(dump,W,descriptors,text)
            if not KABUK:assert 'ACPI_BOZUK RED\n' in text and 'ACPI_BOZUK KABUL' not in text
            regs=command('human-monitor-command',{'command-line':'info registers'})
            nm=subprocess.check_output(['nm',str(W/'k.elf')],text=True)
            symbols={name:int(a,16) for a,t,name in (l.split() for l in nm.splitlines() if len(l.split())==3)}
            assert int(re.search(r'CR3=([0-9a-fA-F]+)',regs)[1],16)==symbols['uefi_pml4'],regs
            pd=dump(symbols['uefi_pd'],16384,W/'pd.bin')
            for a in map(int,re.findall(r'MMIO (\d+)',text)):
                pt=symbols['uefi_pml4']
                for shift in (39,30):
                    g=int.from_bytes(dump(pt+((a>>shift)&511)*8,8,W/'pt.bin'),'little')
                    assert g&1 and not g&128
                    pt=g&(((1<<52)-1)&~4095)
                g=int.from_bytes(dump(pt+((a>>21)&511)*8,8,W/'pt.bin'),'little')
                assert g&0x9b==0x9b and g&(((1<<52)-1)&~((1<<21)-1))==a&~((1<<21)-1),(a,g)
            for bus in command('query-pci'):
                for dev in bus['devices']:
                    for region in dev.get('regions',[]):
                        a=region.get('address',0);size=region.get('size',0)
                        if region.get('type')=='memory' and a>0 and a+size<=2**32:
                            for index in range(a//2**21,(a+size+2**21-1)//2**21):
                                assert struct.unpack_from('<Q',pd,index*8)[0]&0x18==0x18,(dev,region,index)
            assert int(re.search(r'GDT=\s*([0-9a-fA-F]+)',regs)[1],16)==symbols['kdl_x86_gdt'],regs
            if KABUK:
                kabuk_denetle(command,response,f,W,ser,p,n,ram,W/'disk.img')
            else:
                gw,gh,px=screendump(f,W/'gop.ppm',response)
                font=font_yukle();exp=[0x102040]*(gw*gh)
                lines=[('KEMGU-OS UEFI',0xffd800),('Merhaba Dünya — ğüşıöç İ',0xffffff),('ĞÜŞİÖÇ ğüşıöç 0123456789',0xffffff),('─│┌┐└┘├┤┬┴┼═║',0xffffff)]
                for row,(line,color) in enumerate(lines):
                    for col,ch in enumerate(line):glif_ciz(exp,gw,gh,font,8+col*font[1][0],8+row*(font[1][1]+4),ord(ch),color,0x102040)
                diff,first=karsilastir(px,exp,gw);assert diff==0,(diff,first)
                assert (gw,gh)==(fields[5],fields[6])
                print(f'✓ GOP {gw}x{gh}: {gw*gh} piksel birebir',flush=True)
            guid=str(uuid.UUID(bytes_le=raw[144:160]))
            print(f'✓ UEFI {ram} açılış {n}: ABI 24 alan, ELF, CR3/GDT; GOP {fields[5]}x{fields[6]} adım={fields[7]} biçim={fields[8]}; harita={types}; ACPI GUID={guid}',flush=True)
            (W/f'uefi-{ram}-{n}.seri').write_bytes(data)
        finally:
            s.close()
            if p.poll() is None:p.kill()
            p.wait()

if __name__=='__main__':
    kernel=W/'esp/KEMGU/CEKIRDEK.ELF';save=kernel.read_bytes()
    try:
        kernel.unlink();boot(negative='ELF ac')
        bad=bytearray(save);bad[4]=1;kernel.write_bytes(bad);boot(negative='ELF baslik')
        bad=bytearray(save);struct.pack_into('<Q',bad,24,0);kernel.write_bytes(bad);boot(negative='ELF giris')
        bad=bytearray(save);ph=struct.unpack_from('<Q',bad,32)[0];struct.pack_into('<Q',bad,ph+40,0)
        kernel.write_bytes(bad);boot(negative='ELF LOAD')
    finally:kernel.write_bytes(save)
    for ram in os.environ.get('UEFI_RAM','256M 512M 2G' if KABUK else '256M').split():
        for n in range(1,int(os.environ.get('UEFI_TEKRAR','5'))+1):boot(n,ram=ram)
