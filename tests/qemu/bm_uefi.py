#!/usr/bin/env python3
"""Gerçek OVMF: EFI yükleme, ABI ve kendi CR3/GDT'si; KEMGU'dan bağımsız kâhin."""
from pathlib import Path
import os,sys,shutil,subprocess,time,socket,json,struct,re,uuid
W=Path(sys.argv[1]).resolve()

def boot(n=1,negative=False,ram='256M'):
    q=W/'qmp.sock';ser=W/'seri.txt';q.unlink(missing_ok=True);ser.unlink(missing_ok=True)
    shutil.copy('/usr/share/OVMF/OVMF_VARS_4M.fd',W/'vars.fd')
    cmd=['qemu-system-x86_64','-M','q35','-m',ram,'-vga','std','-display','none','-no-reboot',
         '-drive','if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd',
         '-drive',f'if=pflash,format=raw,file={W}/vars.fd','-drive',f'format=raw,file=fat:rw:{W}/esp',
         '-serial',f'file:{ser}','-qmp',f'unix:{q},server=on,wait=off',
         '-device','isa-debug-exit,iobase=0xf4,iosize=0x04']
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
            target=('EFI HATA '+negative).encode() if negative else b'UEFI_HAZIR'
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
            rsdp=dump(fields[13],36,W/'rsdp.bin');assert rsdp[:8]==b'RSD PTR ' and sum(rsdp[:20])%256==0
            if rsdp[15]>=2:assert sum(rsdp)%256==0
            regs=command('human-monitor-command',{'command-line':'info registers'})
            nm=subprocess.check_output(['nm',str(W/'k.elf')],text=True)
            symbols={name:int(a,16) for a,t,name in (l.split() for l in nm.splitlines() if len(l.split())==3)}
            assert int(re.search(r'CR3=([0-9a-fA-F]+)',regs)[1],16)==symbols['uefi_pml4'],regs
            assert int(re.search(r'GDT=\s*([0-9a-fA-F]+)',regs)[1],16)==symbols['kdl_x86_gdt'],regs
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
    for n in range(1,int(os.environ.get('UEFI_TEKRAR','5'))+1):boot(n)
