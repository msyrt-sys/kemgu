"""Yerel SCSI ve USB storage başlıkları ile bağımsız paket kâhini."""
from pathlib import Path
import re,struct
h=Path('/usr/include/scsi/scsi.h').read_text()
for n,v in dict(TEST_UNIT_READY=0,REQUEST_SENSE=3,INQUIRY=18,READ_CAPACITY=37,READ_10=40,WRITE_10=42,SYNCHRONIZE_CACHE=53).items():
 m=re.search(r'^#define\s+'+n+r'\s+(0x[0-9a-fA-F]+|\d+)\b',h,re.M);assert m and int(m[1],0)==v
b=struct.pack('<IIIBBB',0x43425355,0x12345678,1024,128,0,10)+struct.pack('>BBIBHB',40,0,0x01020304,0,2,0)+bytes(6)
assert len(b)==31
out='// Python struct kâhini, dalga4_scsi.py.\ngenel işlev cbw_ornek() -> Dizi<dtam8> { ver ['+', '.join(str(x)+' olarak dtam8' for x in b)+']; }\n'
Path('tests/drivers/usb/scsi_fikstur.kem').write_text(out)
print('CBW/CSW imza',int.from_bytes(b'USBC','little'),int.from_bytes(b'USBS','little'))
print('DMA',2*65536,'halka',256,'ERDP',0x1038,'cycle mask',~1&0xffffffff)

usb=Path("/usr/src/linux-nvidia-6.17-headers-6.17.0-1029/include/linux/usb/storage.h").read_text()
for n,v in dict(US_BULK_CB_SIGN=0x43425355,US_BULK_CS_SIGN=0x53425355,US_BULK_RESET_REQUEST=255).items():
 m=re.search(r"^#define\s+"+n+r"\s+(0x[0-9a-fA-F]+|\d+)\b",usb,re.M);assert m and int(m[1],0)==v

print("Çekirdek transfer sayısı",2+3+3+4*3+2,"DMA üst sınırı",(2**63-1)-2*65536)
