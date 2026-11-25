"""ARM virtio-mmio ağının gerçek eşini ve TCP/HTTP konuk diskini karşılaştırır."""
import argparse
from contextlib import contextmanager
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import socket
from socketserver import ThreadingTCPServer, BaseRequestHandler
import struct
import subprocess
import tempfile
import threading
import time

from dalga12_kahin import Qmp, wait_for
from pcap_oku import pcap_kayitlari

TCP = bytes((i*37+i//251)&255 for i in range(1<<20))
HTTP = bytes((i*11+i//137)&255 for i in range((1<<20)-117))


@contextmanager
def sunucular():
    alinan=bytearray()
    istek=[]
    class Yansit(BaseRequestHandler):
        def handle(self):
            self.request.settimeout(15)
            while len(alinan)<len(TCP):
                b=self.request.recv(16384)
                if not b:
                    break
                alinan.extend(b)
                self.request.sendall(b)
    class Http(BaseHTTPRequestHandler):
        def do_GET(self):
            istek.append(self.path)
            self.send_response(200)
            self.send_header('Content-Length',str(len(HTTP)))
            self.end_headers()
            self.wfile.write(HTTP)
        def log_message(self,*args):
            pass
    with ThreadingTCPServer(('127.0.0.1',0),Yansit) as t, ThreadingHTTPServer(('127.0.0.1',0),Http) as h:
        ts=[threading.Thread(target=s.serve_forever) for s in (t,h)]
        for th in ts:
            th.start()
        try:
            yield t.server_address[1],h.server_port,alinan,istek
        finally:
            for srv in (t,h):
                srv.shutdown()
            for th in ts:
                th.join()


@contextmanager
def arm_ac(w,imaj,net,es,yerel,tcp,http):
    w.mkdir(exist_ok=True,parents=True)
    seri=w/'arm.seri'
    seri.unlink(missing_ok=True)
    disk=w/'arm-disk.img'
    with disk.open('wb') as f:
        f.truncate(4352*512)
        f.seek(8*512)
        f.write(Path('build/kem_user.elf').read_bytes())
        f.seek(128*512)
        f.write(struct.pack('!8s4s4sHH',b'KEMAG017',socket.inet_aton(es),socket.inet_aton(yerel),tcp,http))
    with tempfile.TemporaryDirectory(prefix='kemgu-arm-') as tmp, (w/'qemu.log').open('w') as log:
        yol=Path(tmp)/'qmp'
        cmd=['qemu-system-aarch64','-M','virt','-cpu','cortex-a72','-m','128M','-no-reboot','-display','none',
             '-global','virtio-mmio.force-legacy=false','-drive',f'file={disk},format=raw,if=none,id=d0',
             '-device','virtio-blk-device,drive=d0','-netdev',net,'-device','virtio-net-device,netdev=n0,mac=52:54:00:17:00:17',
             '-object',f'filter-dump,id=tel,netdev=n0,file={w}/arm.pcap','-serial',f'file:{seri}',
             '-qmp',f'unix:{yol},server=on,wait=off','-kernel',str(imaj)]
        p=subprocess.Popen(cmd,stdout=log,stderr=log)
        try:
            wait_for(p,yol.exists)
            with socket.socket(socket.AF_UNIX) as s:
                s.settimeout(10)
                s.connect(str(yol))
                with s.makefile('wb',buffering=0) as f,s.makefile('rb') as r:
                    yield Qmp(f,r),p,seri
        finally:
            if p.poll() is None:
                p.kill()
            p.wait()


def kos(w,img):
    zaman=time.monotonic()
    with sunucular() as (tcp,http,alinan,istek),arm_ac(w,img,'user,id=n0','10.0.2.2','0.0.0.0',tcp,http) as (q,p,s):
        wait_for(p,lambda:s.exists() and b'KEMGU KEM-OS OK' in s.read_bytes(),seconds=90)
        satir=s.read_bytes()
        for im in (b'ARM AG DHCP OK',b'[9] NET ARP OK',b'[10] PING CANLI',b'[42] GOREV OK',
                   b'[43] AG TCP 1048576 OK',b'[44] AG HTTP OK'):
            assert im in satir,(im,satir[-420:])
        assert alinan==TCP and istek==['/veri.bin']
    with (w/'arm-disk.img').open('rb') as f:
        f.seek(256*512)
        assert f.read(len(TCP))==TCP
        f.seek(2304*512)
        assert f.read(len(HTTP))==HTTP
    with (w/'arm.pcap').open('rb') as f:
        cerceveler=list(pcap_kayitlari(f))
    assert any(p.hedef_port==67 for p in cerceveler) and any(p.icmp_tur==8 for p in cerceveler)
    for port,govde in ((tcp,TCP),(http,HTTP)):
        bas=next((p.sira+1)&0xffffffff for p in cerceveler if p.kaynak_port==port and p.bayrak&18==18)
        parcalar=[]
        for p in cerceveler:
            if p.protokol==6 and p.kaynak_port==port:
                b=p.yuk[4*(p.yuk[12]>>4):]
                if b:
                    parcalar.append(((p.sira-bas)&0xffffffff,b))
        tel=bytearray()
        for ofs,b in sorted(parcalar):
            assert ofs<=len(tel)
            ortak=min(len(b),len(tel)-ofs)
            assert tel[ofs:ofs+ortak]==b[:ortak]
            if ortak<len(b):
                tel.extend(b[ortak:])
        assert govde in tel
    print('✓ ARM virtio-mmio DHCP ARP ping TCP 1 MiB SHA256',hashlib.sha256(TCP).hexdigest(),
          'HTTP',len(HTTP),hashlib.sha256(HTTP).hexdigest(),'saniye',round(time.monotonic()-zaman,2),flush=True)


if __name__=='__main__':
    a=argparse.ArgumentParser()
    a.add_argument('dizin',type=Path)
    a.add_argument('imaj',type=Path)
    v=a.parse_args()
    kos(v.dizin.resolve(),v.imaj.resolve())
