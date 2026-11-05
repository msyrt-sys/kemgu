from yardim import testler
import struct
header='''// Fikstürler: python3 tools/codex/a6.py; Python struct ve (-sum(b)) & 255.
kullan drivers::acpi::tablo::{ Rsdp, SdtBaslik, AcpiHatasi, rsdp_coz, rsdp_ara, sdt_coz, sdt_girdileri };
kullan drivers::ortak::bayt::{ bayt_sifir, bayt_toplam8, oku_u8, yaz_u8, yaz_le32, yaz_le64, yaz_ascii };
'''
for rev in (0,2):
 b=bytearray(36 if rev else 20);b[:8]=b'RSD PTR ';b[9:15]=b'KEMGU ';b[15]=rev;struct.pack_into('<I',b,16,1048576)
 if rev:struct.pack_into('<IQ',b,20,36,8589934592)
 b[8]=(-sum(b[:20]))&255
 if rev:b[32]=(-sum(b))&255
 header+=f'işlev rsdp{rev}() -> Dizi<dtam8> {{\n    değişken b: Dizi<dtam8> = bayt_sifir({len(b)});\n'
 for i,x in enumerate(b):
  if x and i not in (8,32):header+=f'    yaz_u8(b, {i}, {x});\n'
 header+='    yaz_u8(b, 8, ((256 - bayt_toplam8(b, 0, 20)) & 255) olarak tam64);\n'
 if rev:header+='    yaz_u8(b, 32, ((256 - bayt_toplam8(b, 0, 36)) & 255) olarak tam64);\n'
 header+='    ver b;\n}\n'
 print('RSDP',rev,'sum',b[8],b[32] if rev else '')
header+='''işlev sdt_verisi(xsdt: mantıksal) -> Dizi<dtam8> {
    değişken n: tam32 = 44;
    eğer xsdt { n = 52; }
    değişken b: Dizi<dtam8> = bayt_sifir(n);
    eğer xsdt { yaz_ascii(b, 0, "XSDT"); } değilse { yaz_ascii(b, 0, "RSDT"); }
    yaz_le32(b, 4, n olarak tam64); yaz_u8(b, 8, 1); yaz_ascii(b, 10, "KEMGU ");
    eğer xsdt { yaz_le64(b, 36, 4096); yaz_le64(b, 44, 8192); }
    değilse { yaz_le32(b, 36, 4096); yaz_le32(b, 40, 8192); }
    yaz_u8(b, 9, ((256 - bayt_toplam8(b, 0, n)) & 255) olarak tam64); ver b;
}
işlev hata_no(e: AcpiHatasi) -> tam32 {
    eşleş e { AcpiHatasi::KisaGirdi => { ver 1; } AcpiHatasi::ImzaYok => { ver 2; }
        AcpiHatasi::Saglama => { ver 3; } AcpiHatasi::GenisSaglama => { ver 4; }
        AcpiHatasi::Uzunluk => { ver 5; } }
}
işlev rsdp_hata(b: Dizi<dtam8>) -> tam32 {
    eşleş rsdp_coz(b) { tamam(v) => { ver 0; } hata(e) => { ver hata_no(e); } }
}
'''
cases={}
for rev in (0,2):
 cases['rsdp_'+str(rev)]=f'''    değişken b: Dizi<dtam8> = rsdp{rev}();
    eğer bayt_toplam8(b, 0, 20) != 0 veya bayt_toplam8(b, 0, dizi_boyut(b)) != 0 {{ ver yanlış; }}
    eşleş rsdp_coz(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken r: Rsdp = v; ver r.revizyon == {rev} ve r.rsdt == 1048576 ve r.xsdt == {8589934592 if rev else 0} ve metin_esit(r.oem, "KEMGU");
    }} }}'''
for name,base,change,code in [('kisa','bayt_sifir(19)','',1),('imza','rsdp0()','yaz_u8(b, 0, 0);',2),('saglama','rsdp0()','yaz_u8(b, 8, 0);',3),('genis','rsdp2()','yaz_u8(b, 32, 0);',4),('uzunluk','rsdp2()','yaz_le32(b, 20, 35);',5),('tasma','rsdp2()','yaz_le32(b, 20, 4294967295);',5),('v2_kisa','rsdp0()','yaz_u8(b, 15, 2); yaz_u8(b, 8, 0); yaz_u8(b, 8, ((256-bayt_toplam8(b, 0, 20)) & 255) olarak tam64);',1)]:
 cases[name]=f'    değişken b: Dizi<dtam8> = {base}; {change}\n    ver rsdp_hata(b) == {code};'
for pos in (32,33):
 cases[f'ara_{pos}']=f'''    değişken b: Dizi<dtam8> = bayt_sifir(64); değişken r: Dizi<dtam8> = rsdp0();
    değişken i: tam32 = 0;
    iken i < 20 {{ yaz_u8(b, {pos} + i, oku_u8(r, i) olarak tam64); i = i + 1; }}
    eşleş rsdp_ara(b) {{ değer(v) => {{ ver {"v == 32" if pos==32 else "yanlış"}; }} hiç => {{ ver {"yanlış" if pos==32 else "doğru"}; }} }}'''
# Ayrı geçerli v1 kopyaları: bozuk ilk adaydan sonraki hizalı adayı bul.
cases['ara_bozuktan_sonra']='''    değişken b: Dizi<dtam8> = bayt_sifir(64); değişken r: Dizi<dtam8> = rsdp0();
    değişken i: tam32 = 0;
    iken i < 20 { yaz_u8(b, i, oku_u8(r, i) olarak tam64); yaz_u8(b, 32 + i, oku_u8(r, i) olarak tam64); i = i + 1; }
    yaz_u8(b, 8, 0);
    eşleş rsdp_ara(b) { değer(v) => { ver v == 32; } hiç => { ver yanlış; } }'''
for mode in ('doğru','yanlış'):
 cases['girdiler_'+mode]=f'''    değişken b: Dizi<dtam8> = sdt_verisi({mode});
    eğer bayt_toplam8(b, 0, dizi_boyut(b)) != 0 {{ ver yanlış; }}
    eşleş sdt_girdileri(b, {mode}) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken xs: Dizi<tam64> = v; ver dizi_boyut(xs) == 2 ve dizi_al(xs, 0) == 4096 ve dizi_al(xs, 1) == 8192;
    }} }}'''
cases['sdt_baslik']='''    eşleş sdt_coz(sdt_verisi(doğru)) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken h: SdtBaslik = v; ver h.uzunluk == 52 ve h.revizyon == 1 ve metin_esit(h.imza, "XSDT") ve metin_esit(h.oem, "KEMGU");
    } }'''
for name,base,change,code in [('kisa','bayt_sifir(35)','',1),('kucuk','sdt_verisi(doğru)','yaz_le32(b, 4, 35);',5),('buyuk','sdt_verisi(doğru)','yaz_le32(b, 4, 4294967295);',5),('saglama','sdt_verisi(doğru)','yaz_u8(b, 9, 0);',3),('artik','sdt_verisi(doğru)','yaz_le32(b, 4, 50); yaz_u8(b, 9, 0); yaz_u8(b, 9, ((256-bayt_toplam8(b, 0, 50)) & 255) olarak tam64);',5)]:
 cases['sdt_'+name]=f'''    değişken b: Dizi<dtam8> = {base}; {change}
    eşleş sdt_girdileri(b, doğru) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver hata_no(e) == {code}; }} }}'''
testler('tests/drivers/acpi/tablo_test.kem',header,cases)
print(len(cases),'test')
