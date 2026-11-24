"""Üç taşıyıcıda aynı LBA için blkdebug EIO, yeniden deneme ve istek yalıtımı."""
import argparse
import re
from pathlib import Path

from dalga12_fat import Fat
from dalga12_imaj import images
from dalga12_kahin import Console, boot, fsck


def kos(w, aygit, kalici):
    usb,nvme,sata = images(w,'gpt')
    bolum = {'usb':usb[1],'nvme':nvme,'sata':sata}[aygit]
    fat = Fat(bolum.read())
    kume = fat.directory(fat.root)['merhaba.txt'][0]
    lba = bolum.start + fat.first + (kume-2)*fat.spc
    conf = w/'hata.conf'
    conf.write_text('[inject-error]\nevent = "none"\niotype = "read"\nerrno = "5"\n'
                    f'sector = "{lba}"\nonce = "{"off" if kalici else "on"}"\n',encoding='ascii')
    with boot(w,usb[0],nvme,sata,disk_hatasi=(aygit,conf)) as (q,p,seri):
        c=Console(w,q,p,seri)
        def hatalar():
            stats=next(s['stats'] for s in q.call('query-blockstats') if s.get('device')=='d'+aygit)
            return stats['failed_rd_operations']
        yol=f'/diskler/{aygit}0p{bolum.no}'
        def duyu():
            return len(re.findall(rb'^scsi_req_parsed .+ command 3 dir ',(w/'dalga12-qemu.log').read_bytes(),re.M))
        eski=duyu() if aygit=='usb' else 0
        ilk=hatalar()
        def denetle(b):
            if kalici:
                alanlar={'sata':rb'ahci LBA %d TFD (\d+) PxIS (\d+)', 'nvme':rb'nvme LBA %d SC (\d+) SCT (\d+)',
                         'usb':rb'usb LBA %d SENSE (\d+) ASC (\d+) ASCQ (\d+)'}[aygit]
                m=re.fullmatch(b'KABUK HATA '+alanlar % lba+b'\n',b)
                assert m,(aygit,lba,b)
                degerler=list(map(int,m.groups()))
                if aygit=='sata': assert degerler[0]&1 and degerler[1]&(1<<30),degerler
                else: assert any(degerler),degerler
                print(b.decode('utf-8').strip(),flush=True)
            else:
                assert b == fat.get('/merhaba.txt')+b'\n',(aygit,b)
        yanitlar=[]
        c.run(f'cat {yol}/merhaba.txt',yanitlar.append)
        fark=hatalar()-ilk
        sense=duyu()-eski if aygit=='usb' else 0
        diger=fat.get('/belgeler/not.txt')
        c.run(f'cat {yol}/belgeler/not.txt',yanitlar.append)
        beklenen=diger if diger.endswith(b'\n') else diger+b'\n'
        assert yanitlar[1] == beklenen, ('Sonraki geçerli okuma',yanitlar)
        denetle(yanitlar[0])
        assert fark == (4 if kalici else 1),(aygit,kalici,fark)
        if aygit=='usb':
            assert sense == fark,(sense,fark)
        assert hatalar() == ilk+fark
        c.shutdown()
    fsck(w,bolum,'hata-'+aygit)
    print('DISK_HATA OK',aygit,'kalıcı' if kalici else 'tek',lba,fark,flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dizin',type=Path)
    parser.add_argument('--aygit',choices=['sata','nvme','usb'])
    parser.add_argument('--tur',choices=['tek','kalici'])
    a=parser.parse_args()
    for aygit in ([a.aygit] if a.aygit else ('sata','nvme','usb')):
        for tur in ([a.tur] if a.tur else ('tek','kalici')):
            kos(a.dizin.resolve(),aygit,tur=='kalici')
