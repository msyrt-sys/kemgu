#!/usr/bin/env python3
"""USB dalları: gerçek QEMU, C ve self için ayrı süreç/imaj, toplam 58 sn sınır."""
from pathlib import Path
import os,sys,socket,subprocess,time,tempfile,shutil
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

def qemu_kos(w,exe,eksik=False):
    with (w/'disk.img').open('wb') as f:f.truncate(4*1024*1024)
    qp=port()
    cmd=['qemu-system-x86_64','-machine','q35','-accel','tcg','-bios',str(w/'dur.bin'),'-display','none','-nodefaults','-m','64M','-qtest',f'tcp:127.0.0.1:{qp},server=on,wait=on','-qtest-log','/dev/null','-device','qemu-xhci,id=x','-drive',f'file={w}/disk.img,if=none,id=u0,format=raw','-device','usb-storage,bus=x.0,drive=u0']
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
            r=subprocess.run([str(exe),str(qp)],capture_output=True,text=True,timeout=kalan())
            print(r.stdout,end='')
            if eksik:
                if r.returncode!=1 or 'QTEST_XHCI HATA xhci aygit sayisi' not in r.stdout:raise RuntimeError('eksik aygıt olumsuzu yakalanmadı')
            elif r.returncode!=0 or 'QTEST_XHCI OK' not in r.stdout:
                raise RuntimeError(f'{exe.name}: rc={r.returncode}, {r.stderr}')
        except Exception:
            out=ROOT/'build/codex6';out.mkdir(exist_ok=True)
            shutil.copyfile(w/'qemu.log',out/'usb-son-qemu.log');raise
        finally:
            p.terminate()
            try:p.wait(timeout=2)
            except subprocess.TimeoutExpired:p.kill();p.wait()

def main():
    if MODE!='xhci':raise ValueError(MODE)
    with tempfile.TemporaryDirectory(prefix='usb-qtest-',dir=ROOT/'build') as td:
        w=Path(td)
        bios=bytearray(65536);bios[0xfff0:0xfff3]=bytes([0xf4,0xeb,0xfd]);(w/'dur.bin').write_bytes(bios)
        for der in ['kemgu','codegen']:
            with (w/f'{der}.ll').open('w') as out:
                kos([f'build/{der}','--llvm','tests/qemu/xhci_qtest.kem'],stdout=out)
            kos(['clang','-Wno-override-module','-x','ir',str(w/f'{der}.ll'),'-x','none','build/kdl_runtime.o','-o',str(w/der)])
            qemu_kos(w,w/der)
            qemu_kos(w,w/der,eksik=True)
            print(f'  ✓ {der}: QEMU {MODE}, iki aygıt + eksik aygıt olumsuzu')
        print(f'=== qtest {MODE}: C + self-host GEÇTİ ({time.monotonic()-START:.2f} sn) ===')
if __name__=='__main__':
    try:main()
    except Exception as e:
        print(f'QTEST_{MODE.upper()} BAŞARISIZ: {e}',file=sys.stderr);sys.exit(2)
