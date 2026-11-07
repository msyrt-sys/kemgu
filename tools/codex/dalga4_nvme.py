#!/usr/bin/env python3
"""Yerel Linux NVMe başlıklarından sabit ve alan sırası doğrulaması."""
from pathlib import Path
import hashlib
import re
import struct

headers=sorted(Path('/usr/src').glob('*/include/linux/nvme.h'))
assert headers, 'Yerel nvme.h gerekli'
h=headers[-1];s=h.read_text();io=Path('/usr/include/linux/nvme_ioctl.h').read_text()
regs={n:int(re.search(r'\bNVME_REG_'+n+r'\s*=\s*(0x[\da-f]+)',s).group(1),0) for n in ['CAP','VS','CC','CSTS','AQA','ASQ','ACQ']}
shifts={n:int(re.search(r'\bNVME_CC_'+n+r'_SHIFT\s*=\s*(\d+)',s).group(1)) for n in ['CSS','MPS','AMS','SHN','IOSQES','IOCQES']}
block=io.split('struct nvme_passthru_cmd {',1)[1].split('};',1)[0]
names=['opcode','flags','nsid','metadata','addr','cdw10','cdw15']
assert [block.index(n+';') for n in names]==sorted(block.index(n+';') for n in names)
cap=31 | 1<<16 | 2<<24 | 3<<32 | 1<<37 | 4<<52
cc=1 | 6<<shifts['IOSQES'] | 4<<shifts['IOCQES']
print(h,'sha256=',hashlib.sha256(h.read_bytes()).hexdigest())
print('regs=',regs,'shifts=',shifts,'CAP=',cap,'CC=',cc,'SQ7=',4096+14*(4<<3),'CQ7=',4096+15*(4<<3))
p=Path('tests/drivers/nvme');p.mkdir(parents=True,exist_ok=True)
(p/'kayit_test.kem').write_text(f'''// Kâhin: python3 tools/codex/dalga4_nvme.py; kaynak Linux nvme.h + nvme_ioctl.h.
kullan drivers::nvme::kayit::{{ NvmeCap, nvme_cap_coz, nvme_zil, nvme_cc_kur, nvme_hazir, nvme_fatal, nvme_kapanma, NVME_REG_CAP, NVME_REG_VS, NVME_REG_CC, NVME_REG_CSTS, NVME_REG_AQA, NVME_REG_ASQ, NVME_REG_ACQ }};
işlev test_kayit() -> mantıksal {{
    ver NVME_REG_CAP == {regs['CAP']} ve NVME_REG_VS == {regs['VS']} ve NVME_REG_CC == {regs['CC']} ve NVME_REG_CSTS == {regs['CSTS']} ve NVME_REG_AQA == {regs['AQA']} ve NVME_REG_ASQ == {regs['ASQ']} ve NVME_REG_ACQ == {regs['ACQ']};
}}
işlev test_cap_zil() -> mantıksal {{
    eşleş nvme_cap_coz({cap}) {{
        hata(e) => {{ ver yanlış; }}
        tamam(c) => {{
            eğer c.kuyruk_azami != 32 veya değil c.bitisik veya c.zaman_ms != 1000 veya c.dstrd != 3 veya c.css != 1 veya c.mps_min != 0 veya c.mps_max != 4 {{ ver yanlış; }}
            eşleş nvme_zil(c,7,yanlış) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ eğer v != 4544 {{ ver yanlış; }} }} }}
            eşleş nvme_zil(c,7,doğru) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ eğer v != 4576 {{ ver yanlış; }} }} }}
            eşleş nvme_zil(c,0-1,doğru) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ }} }}
            eşleş nvme_zil(c,65536,doğru) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ }} }}
        }}
    }}
    eşleş nvme_cap_coz({5<<48 | 4<<52}) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ }} }}
    eşleş nvme_cap_coz(0) {{ hata(e) => {{ ver yanlış; }} tamam(c) => {{ ver c.kuyruk_azami == 1 ve c.zaman_ms == 0; }} }}
}}
işlev test_cc_durum() -> mantıksal {{
    eşleş nvme_cc_kur(doğru,0,0,0,0,6,4) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ eğer v != {cc} {{ ver yanlış; }} }} }}
    eşleş nvme_cc_kur(doğru,16,0,0,0,6,4) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ }} }}
    eşleş nvme_cc_kur(doğru,0,8,0,0,6,4) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ }} }}
    eşleş nvme_cc_kur(doğru,0,0,8,0,6,4) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ }} }}
    eşleş nvme_cc_kur(doğru,0,0,0,4,6,4) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ }} }}
    eşleş nvme_cc_kur(doğru,0,0,0,0,0-1,4) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ }} }}
    ver nvme_hazir(1) ve değil nvme_hazir(2) ve nvme_fatal(2) ve değil nvme_fatal(1) ve nvme_kapanma(8) == 2;
}}
işlev main() -> tam32 {{ değişken h: tam32 = 0; eğer değil test_kayit() {{ h=h+1; }} eğer değil test_cap_zil() {{ h=h+1; }} eğer değil test_cc_durum() {{ h=h+1; }} ver h; }}
''',encoding='utf-8')
