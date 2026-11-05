from yardim import testler
import struct
entries=[struct.pack('<BBBBI',0,8,7,9,1),struct.pack('<BBBBII',1,12,2,0,0xfec00000,24),struct.pack('<BBBBIH',2,10,0,1,33,15),struct.pack('<BBBHB',4,6,255,5,1),struct.pack('<BBHQ',5,12,0,0x1fee00000),bytes([99,3,255])]
b=bytearray(44)+b''.join(entries);b[:4]=b'APIC';struct.pack_into('<I',b,4,len(b));b[8]=3;struct.pack_into('<II',b,36,0xfee00000,1);b[9]=(-sum(b))&255
header='''// Fikstür: python3 tools/codex/c1.py; struct.pack ve (-sum(b))&255.
kullan drivers::acpi::tablo::{ SdtBaslik, AcpiHatasi };
kullan drivers::acpi::madt::{ Madt, MadtGirdisi, madt_coz };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le32, bayt_toplam8 };
işlev madt_verisi() -> Dizi<dtam8> {
'''+f'    değişken b: Dizi<dtam8> = bayt_sifir({len(b)});\n'
for i,x in enumerate(b):
 if x:header+=f'    yaz_u8(b, {i}, {x});\n'
header+='''    ver b;
}
işlev saglama_yenile(b: Dizi<dtam8>, n: tam32) {
    yaz_u8(b, 9, 0); yaz_u8(b, 9, ((256-bayt_toplam8(b, 0, n)) & 255) olarak tam64);
}
'''
cases={}
cases['girdiler']=f'''    eşleş madt_coz(madt_verisi()) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken m: Madt = v; değişken xs: Dizi<MadtGirdisi> = m.girdiler;
        eğer m.lapic_adresi != {0x1fee00000} veya m.bayraklar != 1 veya dizi_boyut(xs) != 5 {{ ver yanlış; }}
        değişken a: MadtGirdisi = dizi_al(xs, 0); değişken io: MadtGirdisi = dizi_al(xs, 1);
        değişken iso: MadtGirdisi = dizi_al(xs, 2); değişken nmi: MadtGirdisi = dizi_al(xs, 3);
        değişken ust: MadtGirdisi = dizi_al(xs, 4);
        ver a.tur == 0 ve a.islemci == 7 ve a.apic == 9 ve a.bayraklar == 1 ve
            io.apic == 2 ve io.adres == {0xfec00000} ve io.gsi == 24 ve
            iso.veriyolu == 0 ve iso.kaynak == 1 ve iso.gsi == 33 ve iso.bayraklar == 15 ve
            nmi.islemci == 255 ve nmi.lint == 1 ve nmi.bayraklar == 5 ve ust.adres == {0x1fee00000};
    }} }}'''
for name,change,expected in [('bir','yaz_u8(b, 45, 1);','girdi boyu'),('sifir','yaz_u8(b, 45, 0);','girdi boyu'),('tasma','yaz_u8(b, 45, 255);','girdi boyu'),('lapic_kisa','yaz_u8(b, 45, 7);','girdi boyu'),('imza','yaz_u8(b, 0, 0);','imza')]:
 cases[name]=f'''    değişken b: Dizi<dtam8> = madt_verisi(); {change} saglama_yenile(b, {len(b)});
    eşleş madt_coz(b) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "{expected}"); }} }}'''
cases['saglama']='''    değişken b: Dizi<dtam8> = madt_verisi(); yaz_u8(b, 9, 0);
    eşleş madt_coz(b) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "sdt"); } }'''
cases['kisa']='    eşleş madt_coz(bayt_sifir(35)) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "sdt"); } }'
cases['bos']='''    değişken b: Dizi<dtam8> = madt_verisi(); yaz_le32(b, 4, 44); saglama_yenile(b, 44);
    eşleş madt_coz(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken m: Madt = v; ver dizi_boyut(m.girdiler) == 0 ve m.lapic_adresi == 4276092928; } }'''
cases['yarim_baslik']='''    değişken b: Dizi<dtam8> = madt_verisi(); yaz_le32(b, 4, 45); saglama_yenile(b, 45);
    eşleş madt_coz(b) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "girdi boyu"); } }'''
testler('tests/drivers/acpi/madt_test.kem',header,cases)
print(len(cases),'test; boy',len(b))
