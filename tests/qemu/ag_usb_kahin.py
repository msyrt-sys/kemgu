"""Gerçek usb-net, xHCI, kullanıcı ELF'leri ve yazılabilir FAT32 veri bölümü."""
import argparse
import hashlib
from pathlib import Path
import shutil
from ag_arac_kahin import komut
from ag_http_kahin import HttpKonsol
from ag_paket_kahin import depo_ac, fat_oku
from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, wait_for
from pcap_oku import pcap_kayitlari
from surec_imaj import imajlar

AYGIT_MAC = '52:54:00:17:00:16'
# QEMU ECM iMACAddress olarak 40 + aygıt adresinin son beş baytını sunar.
KONUK_MAC = bytes.fromhex('405400170016')
BUYUK = bytes((i*17+i//127)&255 for i in range(2*1024*1024))


def kos(w, hub, ham):
    depo = w/'ecm-depo'
    shutil.copytree('build/paket_deposu', depo, dirs_exist_ok=True)
    (depo/'veri.bin').write_bytes(BUYUK)
    usb,nvme,sata = imajlar(w, {ad+'.elf':(w/(ad+'.elf')).read_bytes() for ad in ('ping','indir','paket')})
    etiket = ('hub' if hub else 'kok')+'-'+('es' if ham else 'slirp')
    pcap = w/('ecm-'+etiket+'.pcap')
    usb_port = '1.4' if hub else '4'
    disk = 'nvme0p2' if hub else 'usb0p2'
    if ham:
        from ag_http_ham_es import ham_depo
        sunucu = ham_depo(depo,KONUK_MAC)
    else:
        sunucu = depo_ac(depo)
    with sunucu as (port,istekler):
        net = port.qemu_netdev() if ham else 'user,id=n0'
        tcp_port = 8016 if ham else port
        gecit = '192.0.2.1' if ham else '10.0.2.2'
        url = f'http://{gecit}:{tcp_port}'
        args = ('-netdev',net,'-device',f'usb-net,id=ecm,bus=x.0,port={usb_port},netdev=n0,mac={AYGIT_MAC}',
                '-object',f'filter-dump,id=f0,netdev=n0,file={pcap}')
        with boot(w,usb[0],nvme,sata,hub_derinligi=int(hub),acilis='nvme' if hub else 'usb',ag_aygitlari=args) as (q,p,seri):
            c = HttpKonsol(w,q,p,seri,disk='NVMe' if hub else 'USB')
            def kira():
                wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=25)
            kira()
            durum = komut(c,'donanim')
            assert b'NIC cdc-ecm 40:54:00:17:00:16 link 1' in durum,durum
            assert b'PING alinan 2 / 2' in komut(c,f'ping {gecit} 2 0 1000')
            if ham:
                port.tam_paketi_sina()
            komut(c,'bagla --yazilabilir '+disk)
            s = komut(c,f'indir {url}/veri.bin /diskler/{disk}/veri.bin',150)
            assert f'indir tamam {len(BUYUK)}'.encode() in s,s
            assert b'paket bildirimi guncellendi' in komut(c,'paket guncelle '+url,45)
            assert b'paket kuruldu hesapla' in komut(c,'paket kur hesapla',60)
            komut(c,'sync')
            q.call('device_del',{'id':'ecm'})
            wait_for(p,lambda:c.sayi('be_bagli')==0 and c.sayi('be_kayit')==0,seconds=12)
            assert b'DHCP 0 ip 0.0.0.0' in komut(c,'ag durum')
            q.call('device_add',{'driver':'usb-net','id':'ecm','bus':'x.0','port':usb_port,'netdev':'n0','mac':AYGIT_MAC})
            kira()
            assert b'PING alinan 1 / 1' in komut(c,f'ping {gecit} 1 0 1000')
            c.expected=c.output()
            c.shutdown()
        assert istekler==['/veri.bin','/paketler.txt','/hesapla.elf'],istekler
    fat = Fat(usb[1].read())
    assert fat_oku(fat,'veri.bin')==BUYUK
    assert fat_oku(fat,'bin/hesapla.elf')==(depo/'hesapla.elf').read_bytes()
    fsck(w,usb[1],'ecm-'+etiket)
    with pcap.open('rb') as f:
        kayit=list(pcap_kayitlari(f))
    assert sum(k.kaynak==KONUK_MAC and k.hedef_port==67 for k in kayit)>=4
    assert sum(k.kaynak==KONUK_MAC and k.icmp_tur==8 for k in kayit)>=3
    print('✓ CDC ECM',etiket,'DHCP ping HTTP FAT32 SHA256',hashlib.sha256(BUYUK).hexdigest(),'paket kur, çıkar/tak',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('dizin',type=Path)
    p.add_argument('--hub',action='store_true')
    p.add_argument('--ham',action='store_true')
    a=p.parse_args()
    kos(a.dizin.resolve(),a.hub,a.ham)
