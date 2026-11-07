#!/usr/bin/env python3
"""USB dalları: gerçek QEMU, C ve self için ayrı süreç/imaj, toplam 58 sn sınır."""
from pathlib import Path
import os,sys,socket,subprocess,time,tempfile,shutil
from fat_kahin import dogrula as fat_dogrula
ROOT=Path(__file__).resolve().parents[2]
os.chdir(ROOT)
MODE=sys.argv[1]
START=time.monotonic()
DEADLINE=START+58

def kalan():
    n=DEADLINE-time.monotonic()
    if n<=0:raise TimeoutError('qtest dalı 58 saniye sınırı')
    return n

def kos(cmd,**kw):
    return subprocess.run(cmd,timeout=kalan(),check=True,**kw)

def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1',0));return s.getsockname()[1]

# QMP sözlüğü tools/codex/dalga6_sabit.py ile sabit QEMU sürümünden doğrulanır.
def klavye_kos(exe,qp,mp):
    import json,selectors
    events=[]
    def key(k,down):events.append({'type':'key','data':{'down':down,'key':{'type':'qcode','data':k}}})
    for k,shift in [('a',True),('bracket_left',False),('a',False),('dot',False),('spc',False),('apostrophe',True),('semicolon',False),('ret',False)]:
        if shift:key('shift',True)
        key(k,True);key(k,False)
        if shift:key('shift',False)
    with socket.create_connection(('127.0.0.1',mp),timeout=kalan()) as sock:
        f=sock.makefile('rwb',buffering=0)
        def response():
            while True:
                sock.settimeout(kalan());line=f.readline()
                if not line:raise RuntimeError('QMP kapandı')
                data=json.loads(line)
                if 'error' in data:raise RuntimeError(data['error'])
                if 'return' in data or 'QMP' in data:return data
        def command(name,args=None):
            msg={'execute':name}
            if args is not None:msg['arguments']=args
            f.write(json.dumps(msg).encode()+b'\n');return response()
        response();command('qmp_capabilities')
        child=subprocess.Popen(['stdbuf','-oL',str(exe),str(qp)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        sel=selectors.DefaultSelector();sel.register(child.stdout,selectors.EVENT_READ)
        lines=[];buf=b'';sent=0;codes=[]
        try:
            while sel.get_map():
                ready=sel.select(kalan())
                if not ready:raise TimeoutError('klavye çıktı zaman aşımı')
                chunk=os.read(child.stdout.fileno(),65536)
                if not chunk:
                    sel.unregister(child.stdout);break
                buf+=chunk
                while b'\n' in buf:
                    raw,buf=buf.split(b'\n',1);line=raw.decode();lines.append(line)
                    if line.startswith('KOD '):codes.append(int(line[4:]))
                    if line in ['KLAVYE_HAZIR','RAPOR'] and sent<len(events):
                        command('input-send-event',{'events':[events[sent]]});sent+=1
            rc=child.wait(timeout=kalan())
            out='\n'.join(lines)+'\n'
            print(out,end='',flush=True)
            if rc or codes!=[ord(c) for c in 'Ağaç İş\n']:raise RuntimeError(f'HID rc={rc}, kodlar={codes}')
            return subprocess.CompletedProcess([],rc,out,'')
        finally:
            sel.close()
            if child.poll() is None:child.kill();child.wait()
            child.stdout.close();f.close()

def qemu_kos(w,exe,eksik=False):
    with (w/'disk.img').open('wb') as f:f.truncate((64 if MODE=='usb_depo' else 4)*1024*1024)
    if MODE=='usb_depo':kos(['mkfs.fat','--invariant','-F','32','-s','1','-n','USBQTEST',str(w/'disk.img')],stdout=subprocess.DEVNULL)
    qp=port();mp=port()
    cmd=['qemu-system-x86_64','-machine','q35','-accel','tcg','-bios',str(w/'dur.bin'),'-display','none','-nodefaults','-m','64M','-qtest',f'tcp:127.0.0.1:{qp},server=on,wait={"off" if MODE=="usb_kbd" else "on"}','-qtest-log','/dev/null','-device','qemu-xhci,id=x','-drive',f'file={w}/disk.img,if=none,id=u0,format=raw','-device','usb-storage,bus=x.0,drive=u0']
    if MODE=='usb_kbd':cmd+=['-qmp',f'tcp:127.0.0.1:{mp},server=on,wait=off']
    if not eksik:cmd+=['-device','usb-kbd,bus=x.0']
    with (w/'qemu.log').open('w') as log:
        p=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT)
        try:
            while True:
                kalan()
                if p.poll() is not None:raise RuntimeError((w/'qemu.log').read_text())
                r=subprocess.run(['ss','-ltnH',f'sport = :{qp}'],capture_output=True,timeout=kalan())
                if r.stdout.strip():break
                time.sleep(.01)
            r=klavye_kos(exe,qp,mp) if MODE=='usb_kbd' else subprocess.run([str(exe),str(qp)],capture_output=True,text=True,timeout=kalan())
            if MODE!='usb_kbd':print(r.stdout,end='')
            if eksik:
                if r.returncode!=1 or 'QTEST_XHCI HATA xhci aygit sayisi' not in r.stdout:raise RuntimeError('eksik aygıt olumsuzu yakalanmadı')
            elif r.returncode!=0 or f'QTEST_{MODE.upper()} OK' not in r.stdout:
                raise RuntimeError(f'{exe.name}: rc={r.returncode}, {r.stderr}')
        except Exception:
            out=ROOT/'build/codex6';out.mkdir(exist_ok=True)
            shutil.copyfile(w/'qemu.log',out/'usb-son-qemu.log');raise
        finally:
            p.terminate()
            try:p.wait(timeout=2)
            except subprocess.TimeoutExpired:p.kill();p.wait()

def main():
    if MODE not in ['xhci','usb_depo','usb_kbd']:raise ValueError(MODE)
    with tempfile.TemporaryDirectory(prefix='usb-qtest-',dir=ROOT/'build') as td:
        w=Path(td)
        bios=bytearray(65536);bios[0xfff0:0xfff3]=bytes([0xf4,0xeb,0xfd]);(w/'dur.bin').write_bytes(bios)
        for der in ['kemgu','codegen']:
            with (w/f'{der}.ll').open('w') as out:
                kos([f'build/{der}','--llvm',f'tests/qemu/{MODE}_qtest.kem'],stdout=out)
            kos(['clang','-Wno-override-module','-x','ir',str(w/f'{der}.ll'),'-x','none','build/kdl_runtime.o','-o',str(w/der)])
            qemu_kos(w,w/der)
            if MODE=='usb_depo':
                kos(['fsck.fat','-n',str(w/'disk.img')])
                fat_dogrula(w/'disk.img')
            if MODE=='xhci':qemu_kos(w,w/der,eksik=True)
            print(f'  ✓ {der}: QEMU {MODE}, gerçek aygıt doğrulaması')
        print(f'=== qtest {MODE}: C + self-host GEÇTİ ({time.monotonic()-START:.2f} sn) ===')
if __name__=='__main__':
    try:main()
    except Exception as e:
        print(f'QTEST_{MODE.upper()} BAŞARISIZ: {e}',file=sys.stderr);sys.exit(2)
