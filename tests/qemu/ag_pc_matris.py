"""NIC, açılış diski ve RAM kombinasyonunda gerçek küçük dosya ve paket kurar."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
from pathlib import Path
import sys

from ag_arac_kahin import komut
from ag_http_kahin import HttpKonsol
from ag_iki_konuk import hazir
from ag_paket_kahin import depo_ac, fat_oku
from dalga12_fat import Fat
from dalga12_kahin import boot, fsck, wait_for
from pcap_oku import pcap_kayitlari
from surec_imaj import imajlar

DURUMLAR=(
    ('e1000','usb',256),('e1000e','nvme',512),('igb','sata',256),
    ('virtio-net-pci','usb',512),('usb-net','sata',512),('hub','nvme',256),
    ('e1000e','sata',1024),('virtio-net-pci','nvme',256),
)


def olc(kok,no):
    model,aygit,ram=DURUMLAR[no]
    w=hazir(kok,f'matris{no}')
    tumu={f'{ad}.elf':(kok/f'{ad}.elf').read_bytes() for ad in ('ping','indir','paket')}
    usb,nvme,sata=imajlar(w,tumu)
    depo=Path('build/paket_deposu')
    bek=(depo/'paketler.txt').read_bytes()
    mac=f'52:54:00:17:19:{no+10:02x}'
    usb_nic=model in ('usb-net','hub')
    nic=('usb-net,bus=x.0,port='+('1.4' if model=='hub' else '4')) if usb_nic else model
    pcap=w/'nic.pcap'
    net=('-netdev','user,id=n0','-device',f'{nic},id=nic,netdev=n0,mac={mac}',
         '-object',f'filter-dump,id=tel,netdev=n0,file={pcap}')
    disk=aygit+'0p2'
    with depo_ac(depo) as (port,istek),boot(w,usb[0],nvme,sata,ag_aygitlari=net,
            acilis=aygit,ram=f'{ram}M',hub_derinligi=int(model=='hub')) as (q,p,s):
        c=HttpKonsol(w,q,p,s,disk={'usb':'USB','nvme':'NVMe','sata':'AHCI'}[aygit])
        wait_for(p,lambda:int.from_bytes(c.mem(c.sayi('ag_l5'),4),'little')==3,seconds=22)
        assert q.call('query-memory-size-summary')['base-memory']==ram*1024**2
        once=c.heap(),c.bos_cerceve()
        assert b'DHCP 3 ip 10.0.2.15' in komut(c,'ag durum')
        assert b'PING alinan 1 / 1' in komut(c,'ping 10.0.2.2 1 0 1000')
        komut(c,'bagla --yazilabilir '+disk)
        url=f'http://10.0.2.2:{port}'
        assert f'indir tamam {len(bek)}'.encode() in komut(c,f'indir {url}/paketler.txt /diskler/{disk}/net.txt')
        assert b'paket bildirimi guncellendi' in komut(c,'paket guncelle '+url)
        assert b'paket kuruldu hesapla' in komut(c,'paket kur hesapla')
        assert b'42\n' in komut(c,'hesapla 44-2')
        komut(c,'sync')
        son=c.heap(),c.bos_cerceve()
        assert son[1]==once[1]
        c.expected=c.output()
        c.shutdown()
    assert istek==['/paketler.txt','/paketler.txt','/hesapla.elf']
    with pcap.open('rb') as f:
        kayit=list(pcap_kayitlari(f))
    assert any(x.hedef_port==67 for x in kayit) and any(x.icmp_tur==0 for x in kayit)
    fat=Fat(usb[1].read())
    assert fat_oku(fat,'net.txt')==bek and fat_oku(fat,'bin/hesapla.elf')==(depo/'hesapla.elf').read_bytes()
    fsck(w,usb[1],'matris')
    print('AG19 PC',model,aygit,ram,'MiB DHCP ping indir paket SHA256',hashlib.sha256(bek).hexdigest(),once,son,flush=True)


if __name__=='__main__':
    w=Path(sys.argv[1]).resolve()
    with ThreadPoolExecutor(2) as havuz:
        list(havuz.map(lambda i:olc(w,i),range(len(DURUMLAR))))
