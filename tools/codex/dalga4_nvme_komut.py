#!/usr/bin/env python3
"""Linux nvme.h komut/tamamlanma düzeni ve PRP zincir kâhini."""
from pathlib import Path
import struct
import re
s=sorted(Path('/usr/src').glob('*/include/linux/nvme.h'))[-1].read_text()
for name,value in {'nvme_admin_identify':6,'nvme_admin_create_cq':5,'nvme_admin_create_sq':1,'nvme_cmd_read':2,'nvme_cmd_write':1,'nvme_cmd_flush':0}.items():
    assert int(re.search(r'\b'+name+r'\s*=\s*(0x[0-9a-f]+)',s).group(1),0)==value
b=s.split('struct nvme_common_command {',1)[1].split('};',1)[0]
fields=['opcode','flags','command_id','nsid','cdw2','metadata','dptr','cdw10','cdw15']
assert [b.index(n) for n in fields]==sorted(b.index(n) for n in fields)
assert struct.calcsize('<BBHIQQQQ6I')==64 and struct.calcsize('<QHHHH')==16
sq=struct.pack('<BBHIQQQQ6I',2,0,0x1234,0x1020304,0,0,4096,8192,0x22334455,0x11,6,0,0,0)
cq=struct.pack('<QHHHH',0,7,1,0x1234,1|(0x5a<<1)|(3<<9)|(1<<15))
print('SQ64',sq.hex(),'CQ16',cq.hex(),'PRP',514*4096,513*4096,33554432+4096)
def arr(b):return '['+','.join(f'{n} olarak dtam8' for n in b)+']'
text='''// Kâhin: python3 tools/codex/dalga4_nvme_komut.py (Linux başlığı + struct.pack).
kullan drivers::nvme::komut::{ NvmePrp, PrpSayfa, NvmeTamamlanma, nvme_sq_kur, nvme_io_kur, nvme_identify, nvme_kuyruk_olustur, nvme_flush_kur, nvme_cq_coz, nvme_prp_kur };
kullan drivers::ortak::bayt::{ oku_le32, oku_le64, bayt_sifir, yaz_u8 };
işlev esit(a: Dizi<dtam8>, b: Dizi<dtam8>) -> mantıksal { eğer dizi_boyut(a) != dizi_boyut(b) { ver yanlış; } değişken i: tam32 = 0; iken i < dizi_boyut(a) { eğer dizi_al(a,i) != dizi_al(b,i) { ver yanlış; } i=i+1; } ver doğru; }
işlev test_sq() -> mantıksal {
    değişken beklenen: Dizi<dtam8> = SQ_LITERAL;
    eşleş nvme_io_kur(yanlış,4660,16909060,73588229205,7,4096,8192) { hata(e) => { ver yanlış; } tamam(b) => { eğer değil esit(b,beklenen) { ver yanlış; } } }
    eşleş nvme_io_kur(doğru,65536,1,0,1,4096,0) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş nvme_io_kur(doğru,1,0,0,1,4096,0) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş nvme_io_kur(doğru,1,1,0,0,4096,0) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş nvme_io_kur(doğru,1,1,0,65537,4096,0) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş nvme_identify(2,0,1,4096) { hata(e) => { ver yanlış; } tamam(b) => { eğer oku_le32(b,0) != 131078 veya oku_le32(b,40) != 1 { ver yanlış; } } }
    eşleş nvme_kuyruk_olustur(doğru,3,1,16,4096,1) { hata(e) => { ver yanlış; } tamam(b) => { eğer oku_le32(b,0) != 196613 veya oku_le32(b,40) != 983041 veya oku_le32(b,44) != 1 { ver yanlış; } } }
    eşleş nvme_kuyruk_olustur(yanlış,4,1,16,8192,1) { hata(e) => { ver yanlış; } tamam(b) => { eğer oku_le32(b,0) != 262145 veya oku_le32(b,44) != 65537 { ver yanlış; } } }
    eşleş nvme_flush_kur(5,1) { hata(e) => { ver yanlış; } tamam(b) => { ver oku_le32(b,0) == 327680 ve oku_le64(b,24) == 0; } }
}
işlev test_cq() -> mantıksal {
    değişken b: Dizi<dtam8> = CQ_LITERAL;
    eşleş nvme_cq_coz(b) { hata(e) => { ver yanlış; } tamam(c) => { eğer c.sq_bas != 7 veya c.sqid != 1 veya c.cid != 4660 veya değil c.faz veya c.sc != 90 veya c.sct != 3 veya değil c.dnr veya c.durum == 0 { ver yanlış; } } }
    yaz_u8(b,14,180);
    eşleş nvme_cq_coz(b) { hata(e) => { ver yanlış; } tamam(c) => { eğer c.faz { ver yanlış; } } }
    eşleş nvme_cq_coz(bayt_sifir(15)) { tamam(v) => { ver yanlış; } hata(e) => { ver doğru; } }
}
işlev test_prp() -> mantıksal {
    eşleş nvme_prp_kur(4224,512,4096,0) { hata(e) => { ver yanlış; } tamam(p) => { eğer p.bir != 4224 veya p.iki != 0 veya dizi_boyut(p.listeler) != 0 { ver yanlış; } } }
    eşleş nvme_prp_kur(4224,4096,4096,0) { hata(e) => { ver yanlış; } tamam(p) => { eğer p.iki != 8192 veya dizi_boyut(p.listeler) != 0 { ver yanlış; } } }
    eşleş nvme_prp_kur(4096,2105344,4096,33554432) {
        hata(e) => { ver yanlış; }
        tamam(p) => {
            eğer p.iki != 33554432 veya dizi_boyut(p.listeler) != 2 { ver yanlış; }
            değişken a: PrpSayfa = dizi_al(p.listeler,0); değişken b: PrpSayfa = dizi_al(p.listeler,1);
            eğer a.adres != 33554432 veya b.adres != 33558528 veya oku_le64(a.baytlar,0) != 8192 veya oku_le64(a.baytlar,4088) != 33558528 veya oku_le64(b.baytlar,0) != 2101248 veya oku_le64(b.baytlar,8) != 2105344 veya oku_le64(b.baytlar,16) != 0 { ver yanlış; }
        }
    }
    eşleş nvme_prp_kur(4097,512,4096,0) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş nvme_prp_kur(4096,0,4096,0) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş nvme_prp_kur(4096,12288,4096,8192) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş nvme_prp_kur(4096,512,3000,0) { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş nvme_prp_kur(9223372036854775804,512,4096,0) { tamam(v) => { ver yanlış; } hata(e) => { ver doğru; } }
}
işlev main() -> tam32 { değişken h: tam32 = 0; eğer değil test_sq() { yazdir_metin("sq"); h=h+1; } eğer değil test_cq() { yazdir_metin("cq"); h=h+1; } eğer değil test_prp() { yazdir_metin("prp"); h=h+1; } ver h; }
'''.replace('SQ_LITERAL',arr(sq)).replace('CQ_LITERAL',arr(cq))
Path('tests/drivers/nvme/komut_test.kem').write_text(text,encoding='utf-8')
print('LBA',int('1122334455',16),'admin CDW0',[6|(2<<16),5|(3<<16),1|(4<<16),5<<16], 'QSIZE',(15<<16)|1)
