from yardim import testler
import struct
from pathlib import Path
b=bytearray(256)
for ofs,fmt,v in [(0,'H',0x1234),(2,'H',0x5678),(4,'H',7),(6,'H',16),(8,'B',2),(9,'B',1),(10,'B',6),(11,'B',1),(14,'B',128),(16,'I',0xc001),(20,'I',0x80000008),(24,'I',0x1234500c),(28,'I',2),(60,'B',11),(61,'B',1)]:struct.pack_into('<'+fmt,b,ofs,v)
header='''// Sabitler: python3 tools/codex/c3.py; struct, maske tümleyeni+1, 64 bit birleştirme.
kullan drivers::pci::yapilandirma::{ PciAygit, PciBar, pci_coz, bar_boyu, bar_boyu64, pci_sinif_adi };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le16, yaz_le32 };
işlev pci_verisi() -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = bayt_sifir(256);
'''
for i,x in enumerate(b):
 if x:header+=f'    yaz_u8(b, {i}, {x});\n'
header+='    ver b;\n}\n'
cases={}
cases['alanlar']=f'''    eşleş pci_coz(pci_verisi()) {{ hiç => {{ ver yanlış; }} değer(v) => {{
        değişken p: PciAygit = v; değişken bs: Dizi<PciBar> = p.barlar;
        eğer dizi_boyut(bs) != 5 {{ ver yanlış; }}
        değişken io: PciBar = dizi_al(bs, 0); değişken mm: PciBar = dizi_al(bs, 1); değişken ust: PciBar = dizi_al(bs, 2); değişken son: PciBar = dizi_al(bs, 3);
        ver p.satici == {0x1234} ve p.aygit == {0x5678} ve p.komut == 7 ve p.durum == 16 ve p.revizyon == 2 ve
            p.prog_if == 1 ve p.alt_sinif == 6 ve p.sinif == 1 ve p.cok_islevli ve p.baslik_turu == 0 ve p.kesme_hatti == 11 ve p.kesme_pini == 1 ve
            io.g_c ve io.adres == {0xc000} ve değil io.genis64 ve mm.onceden_getirilebilir ve mm.adres == {0x80000000} ve
            ust.genis64 ve ust.onceden_getirilebilir ve ust.adres == {(2<<32)|0x12345000} ve ust.indeks == 2 ve son.indeks == 4;
    }} }}'''
for name,change in [('aygit_yok','yaz_le16(b, 0, 65535);'),('tip1','yaz_u8(b, 14, 1);'),('son_bar64','yaz_le32(b, 36, 4);'),('bar_rezerve','yaz_le32(b, 16, 6);')]:
 cases[name]=f'    değişken b: Dizi<dtam8> = pci_verisi(); {change}\n    eşleş pci_coz(b) {{ hiç => {{ ver doğru; }} değer(v) => {{ ver yanlış; }} }}'
cases['kisa']='    eşleş pci_coz(bayt_sifir(255)) { hiç => { ver doğru; } değer(v) => { ver yanlış; } }'
cases['boylar']=f'''    değişken m: dtam64 = ((4294967295 olarak dtam64) << 32) | ({0xfffff00c} olarak dtam64);
    ver bar_boyu({0xffffff01}) == {((0xffffff01&0xfffffffc)^0xffffffff)+1} ve bar_boyu({0xfffff008}) == {((0xfffff008&0xfffffff0)^0xffffffff)+1} ve
        bar_boyu64(m) == (4096 olarak dtam64) ve bar_boyu(0) == 0 ve bar_boyu64(0 olarak dtam64) == (0 olarak dtam64);'''
for i,(s,a,p,n) in enumerate([(1,6,1,'AHCI'),(12,3,48,'xHCI'),(12,3,32,'EHCI'),(3,0,0,'VGA'),(2,0,0,'Ethernet'),(6,0,0,'Host koprusu'),(6,4,0,'PCI koprusu'),(1,8,2,'NVMe'),(255,255,255,'Bilinmeyen')]):cases['sinif_'+str(i)]=f'    ver metin_esit(pci_sinif_adi({s}, {a}, {p}), "{n}");'
testler('tests/drivers/pci/yapilandirma_test.kem',header,cases)
p=Path('Makefile');s=p.read_text(encoding='utf-8');needle='tests/drivers/acpi/*_test.kem'
if 'tests/drivers/pci/*_test.kem' not in s:s=s.replace(needle,needle+' tests/drivers/pci/*_test.kem')
p.write_text(s,encoding='utf-8')
print(len(cases),'test')
