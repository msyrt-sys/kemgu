"""OVMF + üç disk. ELF, bölüm kimliği, seri/GOP ve host disk baytları ölçülür."""
from contextlib import contextmanager
from datetime import datetime, timedelta
from hashlib import file_digest, sha256
from pathlib import Path
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time

from dalga12_imaj import images, gpt, DATA
from fat_yazici import MERHABA
from psf_kahin import screendump, kabuk_goruntu, karsilastir
from uefi_kabuk_kahin import keys

RTC = datetime(2026,7,14,9,42)


class Qmp:
    def __init__(self, f, reader):
        self.f = f
        self.reader = reader
        assert 'QMP' in self.response(greeting=True)
        self.call('qmp_capabilities')

    def response(self, greeting=False):
        while True:
            line = self.reader.readline()
            assert line, 'QMP kapandı'
            reply = json.loads(line)
            assert 'error' not in reply, reply
            if greeting or 'return' in reply:
                return reply if greeting else reply['return']

    def call(self, command, arguments=None):
        self.f.write(json.dumps({'execute':command,'arguments':arguments or {}}).encode()+b'\n')
        return self.response()


def wait_for(proc, fn, seconds=15):
    end = time.monotonic()+seconds
    while time.monotonic()<end:
        v = fn()
        if v:
            return v
        assert proc.poll() is None, (proc.returncode, v)
        time.sleep(.02)
    raise AssertionError('QEMU sonucu gelmedi')


@contextmanager
def boot(w, usb, nvme, sata, usb2=None, *, klavye="usb", fare="ps2"):
    path = w/'dalga12.sock'
    path.unlink(missing_ok=True)
    ser = w/'dalga12.seri'
    ser.unlink(missing_ok=True)
    shutil.copy('/usr/share/OVMF/OVMF_VARS_4M.fd',w/'dalga12-vars.fd')
    cmd = ['qemu-system-x86_64','-M','q35','-m','256M','-vga','std','-display','none','-no-reboot',
           '-drive','if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd',
           '-drive',f'if=pflash,format=raw,file={w}/dalga12-vars.fd',
           '-device','qemu-xhci,id=x','-device','usb-kbd,bus=x.0',
           '-drive',f'if=none,id=u,format=raw,file={usb.disk}',
           '-device','usb-storage,bus=x.0,drive=u,bootindex=1',
           '-drive',f'if=none,id=n,format=raw,file={nvme.disk}',
           '-device','nvme,drive=n,serial=dalga12',
           '-drive',f'if=none,id=a,format=raw,file={sata.disk}',
           '-device','ide-hd,drive=a,bus=ide.0',
           '-serial',f'file:{ser}','-qmp',f'unix:{path},server=on,wait=off',
           '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
           '-rtc',f'base={RTC.isoformat()},clock=vm']
    if klavye == "ps2":
        i = cmd.index("usb-kbd,bus=x.0")
        del cmd[i-1:i+1]
    if fare == "usb":
        cmd.extend(["-device", "usb-mouse,bus=x.0"])
    if usb2:
        cmd.extend(['-drive',f'if=none,id=u2,format=raw,file={usb2.disk}',
                    '-device','usb-storage,bus=x.0,drive=u2'])
    with (w/'dalga12-qemu.log').open('w') as err:
        proc = subprocess.Popen(cmd,stdout=err,stderr=err)
        try:
            wait_for(proc,path.exists)
            with socket.socket(socket.AF_UNIX) as sock:
                sock.settimeout(10)
                sock.connect(str(path))
                with sock.makefile('wb', buffering=0) as f, sock.makefile('rb') as reader:
                    yield Qmp(f, reader),proc,ser
        finally:
            if proc.poll() is None:
                proc.kill()
            proc.wait()


def sha(path):
    with path.open('rb') as f:
        return file_digest(f,'sha256').hexdigest()


class Console:
    def __init__(self, w, q, p, ser):
        self.w, self.q, self.p, self.ser = w,q,p,ser
        try:
            start = wait_for(p,lambda: b'KABUK_HAZIR\n' in ser.read_bytes() if ser.exists() else False)
        except AssertionError as exc:
            raise AssertionError(ser.read_text(errors='replace')) from exc
        assert start
        self.marker = b'KEMGU-OS kabuk v0\n'
        self.expected = self.marker+b'DISK USB\nKABUK_HAZIR\n'
        self.akis = self.expected.decode()
        assert self.output() == self.expected, self.output()
        schema = q.call('query-qmp-schema')
        self.qcodes = next({m['name'] for m in v['members']} for v in schema
                           if any(m.get('name')=='apostrophe' for m in v.get('members',[])))
        self.symbols = {l.split()[-1]:int(l.split()[0],16) for l in
                        subprocess.check_output(['nm','-n',str(w/'k.elf')],text=True).splitlines()
                        if len(l.split())==3}

    def output(self):
        s = self.ser.read_bytes()
        assert s.count(b'KEMGU-OS UEFI\n')==1
        return s[s.index(self.marker):]

    def mem(self,address,length):
        path = self.w/'dalga12-ram.bin'
        self.q.call('pmemsave',dict(val=address,size=length,filename=str(path)))
        return path.read_bytes()

    def heap(self):
        return int.from_bytes(self.mem(self.symbols['kem_bump'],8),'little')-self.symbols['__heap_start']

    def input(self,text):
        tr={'ü':('bracket_right',False),'ö':('comma',False),'-':('equal',False)}
        for c in text+'\n':
            k, shifted = tr[c] if c in tr else keys(c)
            assert k in self.qcodes,k
            seq = [(k,True),(k,False)]
            if shifted:
                seq = [('shift',True),*seq,('shift',False)]
            for name,down in seq:
                self.q.call('input-send-event',{'events':[dict(type='key',data=dict(
                    down=down,key=dict(type='qcode',data=name)))]})
                time.sleep(.012)

    def run(self,text,out=b''):
        before = self.heap()
        self.input(text)
        prompt=b'KABUK_HAZIR\n'
        if callable(out):
            def measured():
                tail=self.output()[len(self.expected):]
                return tail if tail.endswith(prompt) else None
            tail=wait_for(self.p,measured)
            out(tail[:-len(prompt)])
        else:
            tail=out+prompt
        self.expected += tail
        self.akis += '> '+text+'\n'+tail.decode()
        def matches():
            actual=self.output()
            assert self.expected.startswith(actual),(text,actual,self.expected)
            return actual==self.expected
        wait_for(self.p,matches)
        after=self.heap()
        assert before<=after<28*1024**2, (text,before,after)
        print(f'HEAP {text!r} +{after-before} toplam={after}',flush=True)

    def shutdown(self):
        try:
            self.input('kapat')
        except (BrokenPipeError,ConnectionResetError,AssertionError):
            # QEMU çıkışı son QMP tuş yanıtından önce gelebilir.
            pass
        assert self.p.wait(timeout=5)==1
        assert self.output()==self.expected+'Sistem durduruldu; güç tuşuna basın.\n'.encode()

    def screen(self):
        def exact():
            w,h,pixels=screendump(self.q.f,self.w/'dalga12.ppm',self.q.response)
            expected=kabuk_goruntu(self.akis,'',gen=w,yuk=h)
            exact.fark=karsilastir(pixels,expected,w)
            return exact.fark[0]==0
        try:
            wait_for(self.p,exact)
        except AssertionError as e:
            raise AssertionError(("kabuk ekranı farklı",getattr(exact,"fark",None))) from e


def check_identity(c,part):
    raw=c.ser.read_bytes()
    addr=int(re.search(rb'BILGI_ADRES (\d+)',raw)[1])
    info=c.mem(addr,248)
    assert struct.unpack_from('<QQ',info,8)==(192,1)
    assert struct.unpack_from('<QQQ',info,176)==(192,56,1),info[176:]
    typ,sub,len_,number,beg,count,sig,fmt,kind=struct.unpack_from('<BBHIQQ16sBB',info,200)
    assert (typ,sub,len_,number,beg,count,fmt,kind)==(4,1,42,part.no,part.start,part.count,part.kind,part.kind)
    assert sig[:len(part.sig)]==part.sig
    assert b'ACILIS_BOLUMU /diskler/usb0p1\n' in raw,raw[-500:]
    print('EFI_HD',kind,number,beg,count,sig.hex(),flush=True)


def run(w,mode):
    usb,nvme,sata=images(w,mode)
    before={p.disk:sha(p.disk) for p in [usb[0],nvme,sata]}
    with boot(w,usb[0],nvme,sata) as (q,p,ser):
        c=Console(w,q,p,ser)
        check_identity(c,usb[0])
        c.run('cat /merhaba.txt',MERHABA+b'\n')
        c.run('cat /diskler/nvme0p1/merhaba.txt',MERHABA+b'\n')
        c.run('cat /diskler/sata0p1/merhaba.txt',MERHABA+b'\n')
        expected=b'/diskler/nvme0p1 salt okunur\n/diskler/sata0p1 salt okunur\n'
        expected+= b'/diskler/usb0p1 ESP salt okunur kok\n/diskler/usb0p2 salt okunur\n' if mode=='gpt' else b'/diskler/usb0p1 yazilabilir kok\n'
        c.run('diskler',expected)
        if mode=='gpt':
            c.run('bagla --yazilabilir usb0p1',b'HATA ESP salt okunur\n')
        c.screen()
        q.call('quit')
        assert p.wait(timeout=5)==0
    assert before=={d:sha(d) for d in before}
    print(f'DALGA12_BAG_OK {mode}: üç disk, EFI kimliği, tam seri/GOP, SHA256 aynı',flush=True)


def check_rtc(raw):
    dt=datetime.strptime(raw.decode(),'%Y-%m-%d %H:%M:%S\n')
    assert RTC<=dt<RTC+timedelta(minutes=3),dt
    print('RTC',dt.isoformat(),flush=True)


def check_stamp(fat,path):
    tenth,ctime,cdate,adate,mtime,mdate=fat.timestamp(path)
    assert mdate==cdate==adate
    assert mtime==ctime and tenth in (0,100)
    dt=datetime(1980+(cdate>>9),(cdate>>5)&15,cdate&31,
                ctime>>11,(ctime>>5)&63,(ctime&31)*2+tenth//100)
    assert RTC<=dt<RTC+timedelta(minutes=3),dt
    print('FAT_RTC',path,dt.isoformat(),flush=True)


def fsck(w,part,suffix,dirty=False):
    volume=w/f'dalga12-{suffix}.fat'
    volume.write_bytes(part.read())
    p=subprocess.run(['fsck.fat','-n',str(volume)],capture_output=True,text=True)
    (w/f'dalga12-{suffix}.fsck.log').write_text(p.stdout+p.stderr)
    assert p.returncode==int(dirty), (suffix,p.returncode,p.stdout,p.stderr)
    if dirty:
        assert 'Dirty bit is set' in p.stdout,p.stdout
    print(f'FSCK {suffix} rc={p.returncode}',flush=True)
    volume.unlink()


def write_matrix(w,mode):
    from dalga12_fat import Fat
    usb,nvme,sata=images(w,mode)
    disk_hash={part.disk:sha(part.disk) for part in (nvme,sata)}
    esp_hash=sha256(usb[0].read()).digest() if mode=='gpt' else None
    written=usb[-1]
    with boot(w,usb[0],nvme,sata) as (q,p,ser):
        c=Console(w,q,p,ser)
        check_identity(c,usb[0])
        c.run('cat /merhaba.txt',MERHABA+b'\n')
        c.run('cat /diskler/nvme0p1/merhaba.txt',MERHABA+b'\n')
        c.run('cat /diskler/sata0p1/merhaba.txt',MERHABA+b'\n')
        expected=b'/diskler/nvme0p1 salt okunur\n/diskler/sata0p1 salt okunur\n'
        expected+=b'/diskler/usb0p1 ESP salt okunur kok\n/diskler/usb0p2 salt okunur\n' if esp_hash else b'/diskler/usb0p1 yazilabilir kok\n'
        c.run('diskler',expected)
        if esp_hash:
            c.run('yaz /n.txt yasak',b'HATA ESP salt okunur\n')
            c.run('bagla --yazilabilir usb0p1',b'HATA ESP salt okunur\n')
        c.run('yaz /diskler/nvme0p1/x.txt yasak',b'HATA bolum salt okunur\n')
        if esp_hash:
            c.run('bagla --yazilabilir usb0p2')
            c.run('cd /diskler/usb0p2')
        c.run('mkdir çalışma')
        c.run('cd çalışma')
        c.run('saat',check_rtc)
        c.run('pwd',('/diskler/usb0p2/çalışma\n' if esp_hash else '/çalışma\n').encode())
        c.run('yaz günlük.txt Ağaç İş')
        c.run('cat günlük.txt','Ağaç İş\n'.encode())
        c.run('cp günlük.txt kopya.txt')
        c.run('mv kopya.txt taşınan.txt')
        c.run('cat taşınan.txt','Ağaç İş\n'.encode())
        c.run('sil taşınan.txt')
        c.run('cd ..')
        c.run('rmdir çalışma',b'HATA dizin bos degil\n')
        c.run('mkdir boş')
        c.run('rmdir boş')
        c.run('sync')
        assert disk_hash=={part.disk:sha(part.disk) for part in (nvme,sata)},'salt okunur disk değişti'
        if esp_hash:
            assert sha256(usb[0].read()).digest()==esp_hash,'ESP değişti'
        c.run('bagla --yazilabilir nvme0p1')
        c.run('yaz /diskler/nvme0p1/iç.txt Merhaba')
        c.run('sync')
        c.screen()
        q.call('quit')
        assert p.wait(timeout=5)==0
    assert sha(sata.disk)==disk_hash[sata.disk],'AHCI salt okunur disk değişti'
    if esp_hash:
        assert sha256(usb[0].read()).digest()==esp_hash,'ESP değişti'
    for i,part in enumerate([*usb,nvme,sata]):
        fsck(w,part,f'{mode}-{i}')
    outer=Fat(written.read())
    assert outer.get('/çalışma/günlük.txt')=='Ağaç İş'.encode()
    assert set(outer.get('/çalışma'))=={'.','..','günlük.txt'}
    assert 'boş' not in outer.directory(outer.root)
    assert outer.get('/merhaba.txt')==MERHABA
    check_stamp(outer,'/çalışma/günlük.txt')
    outer.verify_info()
    internal=Fat(nvme.read())
    assert internal.get('/iç.txt')==b'Merhaba'
    check_stamp(internal,'/iç.txt')
    internal.verify_info()
    assert Fat(sata.read()).get('/merhaba.txt')==MERHABA
    print(f'DALGA12_YAZ_OK {mode}: üç disk, koruma SHA256, tam seri/GOP, fsck, LFN ve baytlar',flush=True)


def interrupted(w):
    usb,nvme,sata=images(w,'mbr')
    with boot(w,usb[0],nvme,sata) as (q,p,ser):
        c=Console(w,q,p,ser)
        c.run('yaz kesik.txt kesinti')
        q.call('quit')
        assert p.wait(timeout=5)==0
    fsck(w,usb[0],'kesinti',dirty=True)
    # Aynı imaj yalnız kesinti ile yeniden açma arasındaki kanıt için korunur.
    with boot(w,usb[0],nvme,sata) as (q,p,ser):
        c=Console(w,q,p,ser)
        assert b'UYARI kirli FAT32 /diskler/usb0p1\n' in ser.read_bytes()
        c.run('cat kesik.txt',b'kesinti\n')
        c.run('sync')
        q.call('quit')
        assert p.wait(timeout=5)==0
    fsck(w,usb[0],'kesinti-sonra')
    print('DALGA12_KESINTI_OK: kirli uyarı, yeniden açılış ve sync',flush=True)


def multi_usb(w):
    from dalga12_fat import Fat
    usb,nvme,sata=images(w,'mbr')
    usb2,=gpt(w/'dalga12-usb2.img',[(w/'dalga12-fs.fat',0,DATA)])
    with boot(w,usb[0],nvme,sata,usb2) as (q,p,ser):
        c=Console(w,q,p,ser)
        check_identity(c,usb[0])
        c.run('cat /diskler/usb1p1/merhaba.txt',MERHABA+b'\n')
        c.run('bagla --yazilabilir usb1p1')
        c.run('yaz /diskler/usb1p1/ikinci.txt ikinci')
        c.run('yaz ilk.txt birinci')
        c.run('cat /diskler/usb1p1/ikinci.txt',b'ikinci\n')
        c.screen()
        c.shutdown()
    fsck(w,usb[0],'cok-usb-ilk')
    fsck(w,usb2,'cok-usb-ikinci')
    assert Fat(usb[0].read()).get('/ilk.txt')==b'birinci'
    assert Fat(usb2.read()).get('/ikinci.txt')==b'ikinci'
    print('DALGA12_COK_USB_OK: iki BOT ve USB HID aynı xHCI halkasında',flush=True)


def prove(w,kind):
    if kind=='multi':
        multi_usb(w)
        print('DALGA12_KANIT OK multi',flush=True)
        return
    from dalga12_fat import Fat
    mode='gpt' if kind in ('identity','esp') else 'mbr'
    usb,nvme,sata=images(w,mode)
    head=usb[0]
    initial=sha256(head.read()).hexdigest()
    with boot(w,head,nvme,sata) as (q,p,ser):
        if kind=='identity':
            wait_for(p,lambda: ser.exists() and b'KABUK_HAZIR\n' in ser.read_bytes())
            raw=ser.read_bytes()
            assert b'ACILIS_BOLUMU /diskler/usb0p1\n' in raw, ('kimlik eşleşmedi',raw[-350:])
        c=Console(w,q,p,ser)
        check_identity(c,head)
        if kind=='esp':
            try:
                c.run('yaz /n.txt yasak',b'HATA ESP salt okunur\n')
            finally:
                current=sha256(head.read()).hexdigest()
                assert current==initial, ('ESP SHA256 değişti',initial,current)
        else:
            c.run('yaz günlük.txt denetim')
            c.run('sync')
        q.call('quit')
        assert p.wait(timeout=5)==0
    if mode=='mbr':
        errors=[]
        try:
            fsck(w,head,'islem')
        except AssertionError as exc:
            errors.append(('fsck',str(exc)))
        try:
            result=Fat(head.read())
            assert result.get('/günlük.txt')==b'denetim'
            if kind=='clock':
                try:
                    check_stamp(result,'/günlük.txt')
                except AssertionError as exc:
                    raise AssertionError(('FAT RTC damgası',str(exc))) from exc
        except (AssertionError,KeyError) as exc:
            errors.append(('Python FAT okuyucu',str(exc)))
        assert not errors,errors
    print(f'DALGA12_KANIT OK {kind}',flush=True)


if __name__=='__main__':
    w=Path(sys.argv[1]).resolve()
    if os.getenv('DALGA12_YALNIZ_COK_USB')=='1':
        multi_usb(w)
        sys.exit(0)
    if proof:=os.getenv('DALGA12_KANIT'):
        prove(w,proof)
        sys.exit(0)
    suite=write_matrix if os.getenv('DALGA12_YAZ')=='1' else run
    for _ in range(int(os.getenv('DALGA12_TEKRAR','1'))):
        for mode in os.getenv('DALGA12_TURLER','gpt mbr').split():
            suite(w,mode)
    if os.getenv('DALGA12_KESINTI')=='1':
        interrupted(w)
    if os.getenv('DALGA12_COK_USB')=='1':
        multi_usb(w)
