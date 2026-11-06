from yardim import testler
import struct
b=bytearray(148);b[:4]=b'FACP';struct.pack_into('<I',b,4,len(b));b[8]=6
for off,fmt,val in [(36,'I',0x100000),(40,'I',0x200000),(46,'H',9),(48,'I',0xb2),(56,'I',0x400),(64,'I',0x404),(116,'B',1),(117,'B',8),(118,'B',0),(119,'B',1),(120,'Q',0xcf9),(128,'B',6),(140,'Q',0x123456789)]:struct.pack_into('<'+fmt,b,off,val)
b[9]=(-sum(b))&255
header='''// Fikstür: python3 tools/codex/c2.py; struct.pack ve (-sum(b))&255.
kullan drivers::acpi::tablo::{ SdtBaslik, AcpiHatasi };
kullan drivers::acpi::fadt::{ Fadt, Gas, fadt_coz, dsdt_adresi };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le32, yaz_le64, bayt_toplam8 };
işlev fadt_verisi() -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = bayt_sifir(148);
'''
for i,x in enumerate(b):
 if x:header+=f'    yaz_u8(b, {i}, {x});\n'
header+='''    ver b;
}
işlev saglama_yenile(b: Dizi<dtam8>, n: tam32) {
    yaz_le32(b, 4, n olarak tam64); yaz_u8(b, 9, 0); yaz_u8(b, 9, ((256-bayt_toplam8(b, 0, n)) & 255) olarak tam64);
}
'''
cases={}
cases['alanlar']=f'''    eşleş fadt_coz(fadt_verisi()) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken f: Fadt = v; değişken g: Gas = f.reset_reg;
        ver f.firmware_ctrl == {0x100000} ve f.dsdt == {0x200000} ve f.sci_int == 9 ve f.smi_cmd == {0xb2} ve
            f.pm1a_evt_blok == {0x400} ve f.pm1a_cnt_blok == {0x404} ve f.reset_degeri == 6 ve
            g.alan_turu == 1 ve g.bit_genisligi == 8 ve g.bit_ofseti == 0 ve g.erisim_boyu == 1 ve g.adres == {0xcf9} ve
            dsdt_adresi(f) == {0x123456789};
    }} }}'''
for n in (129,147,148):
 cases['uzunluk_'+str(n)]=f'''    değişken b: Dizi<dtam8> = fadt_verisi(); saglama_yenile(b, {n});
    eşleş fadt_coz(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ değişken f: Fadt = v; ver dsdt_adresi(f) == {0x123456789 if n==148 else 0x200000}; }} }}'''
cases['x_sifir']='''    değişken b: Dizi<dtam8> = fadt_verisi(); yaz_le64(b, 140, 0); saglama_yenile(b, 148);
    eşleş fadt_coz(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken f: Fadt = v; ver dsdt_adresi(f) == 2097152; } }'''
for name,change,err in [('kisa','saglama_yenile(b, 128);','kisa fadt'),('imza','yaz_u8(b, 0, 0); saglama_yenile(b, 148);','imza'),('saglama','yaz_u8(b, 9, 0);','sdt'),('tasma','yaz_le32(b, 4, 4294967295);','sdt')]:
 cases[name]=f'    değişken b: Dizi<dtam8> = fadt_verisi(); {change}\n    eşleş fadt_coz(b) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "{err}"); }} }}'
testler('tests/drivers/acpi/fadt_test.kem',header,cases)
print(len(cases),'test')
