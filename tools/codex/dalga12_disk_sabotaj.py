"""Temiz veya bozulan davranışı, yeni imajla gerçek OVMF üzerinde ölç."""
import argparse
import os
from pathlib import Path
import subprocess

KABUK = 'test/ornekler/kem_os_x86_kabuk.kem'
LAYERS = {
    'esp': ('drivers/fs/bagla.kem', '    eğer k.bolum.esp { ver hata("ESP salt okunur"); }\n', '',
            'ESP SHA256 değişti'),
    'mirror': ('drivers/fs/fat32_sektor_yaz.kem', 'iken i < s.bpb.fat_sayisi {',
               'iken i < 1 {', 'FAT kopyaları farklı'),
    'lfn': ('drivers/fs/fat32_kayit.kem', '+ 13, saglama olarak tam64',
            '+ 13, ((saglama+1)%256) olarak tam64', 'LFN sağlama'),
    'identity': ('boot/efi/stub.c', 'kopya(uzanti+8,hd,42);',
                 'kopya(uzanti+8,hd,42);uzanti[32]^=1;', 'kimlik eşleşmedi'),
    'clock': (KABUK, 'saat.tarih=((rtc_an.yil-1980)<<9)|(rtc_an.ay<<5)|rtc_an.gun;',
              'saat.tarih=33;', 'FAT RTC damgası'),
    'multi': ('test/ornekler/kem_os_x86_blok_ortak.kem',
              'xhci_depo_ekle(x,i,bd_usb_bas_al(),bd_usb_dongu_al())',
              'xhci_depo_ekle(x,i,0,doğru)', 'xhci olay'),
    'flush': ('test/ornekler/kem_os_x86_blok_ortak.kem',
              'işlev bd_bosalt(d: &değişken BlokDonanim) -> sonuç<mantıksal,metin> {\n',
              'işlev bd_bosalt(d: &değişken BlokDonanim) -> sonuç<mantıksal,metin> {\n ver tamam(doğru);\n',
              'Python FAT okuyucu'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cases', nargs='+', choices=LAYERS)
    parser.add_argument('--clean', action='store_true')
    args = parser.parse_args()
    for kind in args.cases:
        name, old, new, evidence = LAYERS[kind]
        path = Path(name)
        original = path.read_bytes()
        assert original.count(old.encode()) == 1, (name,old)
        try:
            if not args.clean:
                path.write_bytes(original.replace(old.encode(),new.encode(),1))
            env = dict(os.environ, DALGA12_DERLEYICILER='build/kemgu',
                       DALGA12_KANIT=kind, DALGA12_CALISMA_DIZINI=f'build/dalga12_kanit_{kind}')
            p = subprocess.run(['bash','test/bm_x86_disk_harness.sh'], env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            print(p.stdout,flush=True)
            if args.clean:
                assert p.returncode==0 and f'DALGA12_KANIT OK {kind}' in p.stdout,kind
            else:
                assert p.returncode!=0 and evidence in p.stdout,kind
                print(f'SABOTAJ OK {kind}: 1/1 kırmızı',flush=True)
        finally:
            if not args.clean:
                path.write_bytes(original)


if __name__=='__main__':
    main()
