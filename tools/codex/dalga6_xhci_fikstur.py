#!/usr/bin/env python3
"""USB_QTEST_IZ=1 bash test/qtest_harness.sh xhci günlüğünü KEM fikstürüne çevir."""
from pathlib import Path
import sys,hashlib,json
blocks=[];rows=[]
for line in Path(sys.argv[1]).read_text().splitlines():
    if line.startswith('IZ '):rows.append([int(x) for x in line.split()[1:]])
    if line=='QTEST_XHCI OK':
        assert rows;blocks.append(rows);rows=[]
assert len(blocks)==2
out=['// Gerçek QEMU 8.2.2, HLT BIOS, q35/qemu-xhci/usb-storage/usb-kbd.',
     '// Yenileme: USB_QTEST_IZ=1 bash test/qtest_harness.sh xhci > build/xhci-iz.log',
     '// python3 tools/codex/dalga6_xhci_fikstur.py build/xhci-iz.log',
     'kullan tests::drivers::usb::sahte_xhci::{ XhciIz };',
     'genel işlev xhci_qemu_izi(der: tam32) -> Dizi<XhciIz> {']
for idx,block in enumerate(blocks):
    digest=hashlib.sha256(json.dumps(block,separators=(',',':')).encode()).hexdigest()
    print(('C' if idx==0 else 'self'),len(block),'istek',digest)
    out.append(f'    // {len(block)} istek; kanonik JSON SHA256 {digest}')
    if idx==0:out.append('    eğer der == 0 {')
    out.append('    ver [')
    for row in block:
        assert len(row)>=6
        t,a,v,n,h,y,*b=row
        assert 1<=t<=7 and all(0<=x<=255 for x in b)
        assert (len(b)==n if t==5 else not b)
        bs=','.join(f'{x} olarak dtam8' for x in b)
        out.append(f'        XhciIz {{ tur: {t}, adres: {a}, deger: {v}, boy: {n}, ozet: {h}, yanit: {y}, baytlar: [{bs}] }},')
    # Dil kartında son elemandan sonra virgül yok.
    out[-1]=out[-1].removesuffix(',')
    out.append('    ];')
    if idx==0:out.append('    }')
out.append('}')
Path('tests/drivers/usb/xhci_qemu_fikstur.kem').write_text('\n'.join(out)+'\n')
