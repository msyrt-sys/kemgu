"""Üç gerçek QEMU aygıtı: kirli LRU taşması, flush, host bayt kâhini."""
from pathlib import Path
import subprocess,sys,shutil
w=Path(sys.argv[1]).resolve()
for n in range(int(__import__('os').environ.get('BLOK_TEKRAR','1'))):
    for ad in ('nvme','ahci','usb'):(w/(ad+'.img')).write_bytes(bytes(8192*512))
    shutil.copy('/usr/share/OVMF/OVMF_VARS_4M.fd',w/'blok-vars.fd')
    ser=w/'blok.seri';ser.unlink(missing_ok=True)
    cmd=['qemu-system-x86_64','-M','q35','-m','256M','-display','none','-no-reboot',
     '-drive','if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd',
     '-drive',f'if=pflash,format=raw,file={w}/blok-vars.fd',
     '-drive',f'if=none,id=boot,format=raw,file=fat:rw:{w}/esp','-device','virtio-blk-pci,drive=boot,bootindex=1',
     '-serial',f'file:{ser}','-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
     '-device','qemu-xhci,id=x','-drive',f'if=none,id=n,format=raw,file={w}/nvme.img','-device','nvme,drive=n,serial=dalga12',
     '-drive',f'if=none,id=a,format=raw,file={w}/ahci.img','-device','ide-hd,drive=a,bus=ide.0',
     '-drive',f'if=none,id=u,format=raw,file={w}/usb.img','-device','usb-storage,drive=u,bus=x.0']
    with (w/'blok-qemu.log').open('w') as log:
        p=subprocess.run(cmd,stdout=log,stderr=log,timeout=60)
    text=ser.read_text(errors='replace');assert p.returncode==33,(p.returncode,text)
    assert text.count('BLOK_BASLA\n')==1 and 'BLOK_BITTI\n' in text,text
    names=[l.split()[1] for l in text.splitlines() if l.startswith('BLOK_OK ')]
    assert set(names)=={'nvme0','ahci0','usb0'},names
    for no,name in enumerate(names):
        data=(w/(name[:-1]+'.img')).read_bytes();expected=bytearray(len(data))
        for s in range(65):expected[(200+s)*512:(201+s)*512]=bytes((no*37+s*7+j*13+5)&255 for j in range(512))
        assert data==expected,('kalicilik veya dokunulmayan sektor',name)
    print(f'BLOK_KAHIN OK #{n+1}: üç aygıt, 65 kirli sektör/64 LRU, flush ve host baytları',flush=True)
