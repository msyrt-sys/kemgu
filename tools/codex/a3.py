from yardim import testler
import struct,zlib,uuid
b=bytearray(1536)
struct.pack_into('<BII',bytearray(9),0,238,1,2047)
b[450]=238;struct.pack_into('<II',b,454,1,2047);b[510:512]=b'\x55\xaa'
b[1024:1040]=uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b').bytes_le
b[1040:1056]=bytes([34])*16
struct.pack_into('<QQ',b,1056,34,2014);b[1080:1086]='EFI'.encode('utf-16le')
b[512:520]=b'EFI PART'
struct.pack_into('<II',b,520,65536,92)
struct.pack_into('<QQQQ',b,536,1,2047,34,2014);b[568:584]=bytes([17])*16
struct.pack_into('<QIII',b,584,2,4,128,zlib.crc32(b[1024:]))
struct.pack_into('<I',b,528,zlib.crc32(b[512:604]))
print('girdi CRC',zlib.crc32(b[1024:]),'başlık CRC',struct.unpack_from('<I',b,528)[0])
assert zlib.crc32(b[1024:])==2282802036 and struct.unpack_from('<I',b,528)[0]==3793980875
header='''// Hesap: python3 tools/codex/a3.py (struct, uuid.bytes_le, zlib.crc32).
// Bulgu C001: aynı adlı modül/işlev için probe ile doğrulanmış nitelikli çağrı.
kullan drivers::disk::crc32;
kullan drivers::disk::bolum::{ Bolum, BolumHatasi, mbr_coz, gpt_mi, gpt_coz };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le32, yaz_le64, yaz_ascii };
işlev test_diski() -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = bayt_sifir(1536);
'''
for i,x in enumerate(b):
    if x: header+=f'    yaz_u8(b, {i}, {x});\n'
header+='''    ver b;
}
işlev baslik_onar(b: Dizi<dtam8>) {
    yaz_le32(b, 528, 0); yaz_le32(b, 528, crc32::crc32(b, 512, 92));
}
işlev hata_kodu(r: sonuç<Dizi<Bolum>, BolumHatasi>) -> tam32 {
    eşleş r {
        tamam(v) => { ver 0; }
        hata(e) => { eşleş e {
            BolumHatasi::KisaGirdi => { ver 1; } BolumHatasi::ImzaYok => { ver 2; }
            BolumHatasi::KorumaciMBRDegil => { ver 3; } BolumHatasi::GptImzaYok => { ver 4; }
            BolumHatasi::GptBaslikCrc => { ver 5; } BolumHatasi::GptGirdiCrc => { ver 6; }
            BolumHatasi::GptBoyut => { ver 7; }
        } }
    }
    ver 99;
}
'''
cases={}
for name,v in [('rakam',b'123456789'),('bos',b''),('a',b'a')]:
    cases['crc_'+name]=f'''    değişken b: Dizi<dtam8> = bayt_sifir({len(v)});
    yaz_ascii(b, 0, "{v.decode()}");
    ver crc32::crc32(b, 0, {len(v)}) == {zlib.crc32(v)} ve crc32::crc32(b, -1, 1) == 0 ve crc32::crc32(b, 0, 2147483647) == 0;'''
cases['mbr']='''    değişken b: Dizi<dtam8> = test_diski();
    yaz_u8(b, 450, 12); yaz_le32(b, 454, 2048); yaz_le32(b, 458, 100000);
    eğer gpt_mi(b) { ver yanlış; }
    eşleş mbr_coz(b) { hata(e) => { ver yanlış; } tamam(v) => {
        eğer dizi_boyut(v) != 1 { ver yanlış; }
        // Bulgu C003: eşleş bağını açık dizi tipiyle yerel değişkene al.
        değişken bolumler: Dizi<Bolum> = v;
        değişken p: Bolum = dizi_al(bolumler, 0);
        ver p.tur == 12 ve p.baslangic_lba == 2048 ve p.sektor_sayisi == 100000 ve metin_esit(p.ad, "");
    } }
}'''.removesuffix('\n}')
cases['gpt']=f'''    değişken b: Dizi<dtam8> = test_diski();
    eğer değil gpt_mi(b) {{ ver yanlış; }}
    eşleş gpt_coz(b, 512) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        eğer dizi_boyut(v) != 1 {{ ver yanlış; }}
        // Bulgu C003: eşleş bağını açık dizi tipiyle yerel değişkene al.
        değişken bolumler: Dizi<Bolum> = v;
        değişken p: Bolum = dizi_al(bolumler, 0);
        ver p.tur == 238 ve p.baslangic_lba == 34 ve p.sektor_sayisi == {2014-34+1} ve metin_esit(p.ad, "EFI");
    }} }}'''
cases['kisa']='    ver hata_kodu(mbr_coz(bayt_sifir(511))) == 1 ve değil gpt_mi(bayt_sifir(0));'
for name,change,api,code in [
 ('mbr_imza','yaz_u8(b, 510, 0);','mbr_coz(b)',2),
 ('korumaci','yaz_u8(b, 450, 12);','gpt_coz(b, 512)',3),
 ('gpt_imza','yaz_u8(b, 512, 0);','gpt_coz(b, 512)',4),
 ('baslik_crc','yaz_u8(b, 528, 0);','gpt_coz(b, 512)',5),
 ('girdi_crc','yaz_u8(b, 1024, 0);','gpt_coz(b, 512)',6),
 ('baslik_kisa','yaz_le32(b, 524, 91);','gpt_coz(b, 512)',7),
 ('baslik_tasma','yaz_le32(b, 524, 4294967295);','gpt_coz(b, 512)',7),
 ('girdi_kisa','yaz_le32(b, 596, 127); baslik_onar(b);','gpt_coz(b, 512)',7),
 ('girdi_tasma','yaz_le32(b, 592, 4294967295); baslik_onar(b);','gpt_coz(b, 512)',7),
 ('lba_tasma','yaz_le64(b, 584, 9223372036854775807); baslik_onar(b);','gpt_coz(b, 512)',7),
 ('lba_negatif','yaz_le64(b, 584, -1); baslik_onar(b);','gpt_coz(b, 512)',7),
 ('sektor_sifir','','gpt_coz(b, 0)',7),('sektor_tasma','','gpt_coz(b, 2147483647)',7),
 ('ters_lba','yaz_le64(b, 1064, 33); yaz_le32(b, 600, crc32::crc32(b, 1024, 512)); baslik_onar(b);','gpt_coz(b, 512)',7)]:
    cases[name]=f'    değişken b: Dizi<dtam8> = test_diski();\n    {change}\n    ver hata_kodu({api}) == {code};'
testler('tests/drivers/disk/bolum_test.kem',header,cases)
print(len(cases),'test')
