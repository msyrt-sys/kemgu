#!/usr/bin/env python3
"""Linux v6.17 AHCI ve yerel ATA başlıklarından sabit doğrulaması."""
from pathlib import Path
import urllib.request
import re
import hashlib
p=Path('build/codex4/linux-ahci.h');p.parent.mkdir(parents=True,exist_ok=True)
if not p.exists():
    with urllib.request.urlopen('https://raw.githubusercontent.com/torvalds/linux/v6.17/drivers/ata/ahci.h',timeout=20) as r:p.write_bytes(r.read())
s=p.read_text()
names=['HOST_CAP','HOST_CTL','HOST_IRQ_STAT','HOST_PORTS_IMPL','HOST_VERSION','PORT_LST_ADDR','PORT_LST_ADDR_HI','PORT_FIS_ADDR','PORT_FIS_ADDR_HI','PORT_IRQ_STAT','PORT_IRQ_MASK','PORT_CMD','PORT_TFDATA','PORT_SIG','PORT_SCR_STAT','PORT_SCR_CTL','PORT_SCR_ERR','PORT_CMD_ISSUE']
regs={name:int(re.search(r'\b'+name+r'\s*=\s*(0x[0-9a-f]+)',s).group(1),0) for name in names}
print('AHCI SHA256',hashlib.sha256(p.read_bytes()).hexdigest(),regs)
a=sorted(Path('/usr/src').glob('*/include/linux/ata.h'))[-1].read_text()
for n,v in {'ATA_CMD_ID_ATA':236,'ATA_CMD_READ_EXT':37,'ATA_CMD_WRITE_EXT':53,'ATA_CMD_FLUSH_EXT':234}.items():assert int(re.search(r'\b'+n+r'\s*=\s*(0x[0-9A-Fa-f]+)',a).group(1),0)==v
assert re.search(r'PORT_IRQ_TF_ERR\s*=\s*BIT\(30\)',s)
print('CAP',0x80001f1f,'port31 CI',0x100+31*0x80+0x38,'CR|FR',(1<<15)|(1<<14),'TFES',1<<30)
