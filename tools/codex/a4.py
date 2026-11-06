from yardim import testler
import struct
print('imza',int.from_bytes(b'\x55\xaa','little'),'veri',32+2*1024,'küme',(1048576-2080)//8)
header='''// C005: yapıdan sonraki sabit argümanlarda açık tam64 dönüşümü.
// Hesap: python3 tools/codex/a4.py; struct BPB, (toplam-ayrılmış-FAT)/küme.
kullan drivers::fs::fat32_bpb::{ Fat32Bpb, BpbHatasi, bpb_coz, kume_sektoru, FatGirdi, fat_girdi, zincir };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le16, yaz_le32 };
işlev bpb_verisi() -> Dizi<dtam8> {
    değişken b: Dizi<dtam8> = bayt_sifir(512);
'''
b=bytearray(512)
for ofs,fmt,v in [(11,'H',512),(13,'B',8),(14,'H',32),(16,'B',2),(32,'I',1048576),(36,'I',1024),(44,'I',2),(48,'H',1),(510,'H',0xaa55)]:struct.pack_into('<'+fmt,b,ofs,v)
for i,v in enumerate(b):
 if v:header+=f'    yaz_u8(b, {i}, {v});\n'
header+='''    ver b;
}
işlev bpb_hatasi(b: Dizi<dtam8>) -> tam32 {
    eşleş bpb_coz(b) {
        tamam(v) => { ver 0; }
        hata(e) => { eşleş e {
            BpbHatasi::KisaGirdi => { ver 1; } BpbHatasi::ImzaYok => { ver 2; }
            BpbHatasi::SektorBoyu => { ver 3; } BpbHatasi::KumeBoyu => { ver 4; }
            BpbHatasi::Fat32Degil => { ver 5; }
        } }
    }
    ver 99;
}
'''
cases={}
cases['bpb']=f'''    eşleş bpb_coz(bpb_verisi()) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken b: Fat32Bpb = v;
        ver b.sektor_bayt == 512 ve b.kume_sektor == 8 ve b.ayrilmis == 32 ve b.fat_sayisi == 2 ve
            b.toplam_sektor == 1048576 ve b.fat_sektor == 1024 ve b.kok_kume == 2 ve b.fsinfo == 1 ve
            b.ilk_veri_sektoru == {32+2*1024} ve b.kume_sayisi == {(1048576-2080)//8} ve
            kume_sektoru(b, 2 olarak tam64) == 2080 ve kume_sektoru(b, 3 olarak tam64) == {2080+8};
    }} }}'''
cases['kisa']='    ver bpb_hatasi(bayt_sifir(511)) == 1;'
for name,ofs,size,val,code in [('imza',510,16,0,2),('sektor',11,16,513,3),('kume_sifir',13,8,0,4),('kume_uc',13,8,3,4),('kume_buyuk',13,8,255,4),('kok_girdi',17,16,1,5),('fat16',22,16,1,5),('fat_sifir',36,32,0,5),('toplam',32,32,100,5),('ayrilmis',14,16,0,5),('fat_sayisi',16,8,0,5),('kok_kucuk',44,32,1,5),('kok_buyuk',44,32,9999999,5)]:
 cases[name]=f'    değişken b: Dizi<dtam8> = bpb_verisi();\n    yaz_{"u8" if size==8 else "le"+str(size)}(b, {ofs}, {val});\n    ver bpb_hatasi(b) == {code};'
for sb in (512,1024,2048,4096):
 cases['sektor_'+str(sb)]=f'    değişken b: Dizi<dtam8> = bpb_verisi(); yaz_le16(b, 11, {sb});\n    ver bpb_hatasi(b) == 0;'
cases['toplam16']='''    değişken b: Dizi<dtam8> = bpb_verisi(); yaz_le16(b, 19, 60000);
    eşleş bpb_coz(b) { hata(e) => { ver yanlış; } tamam(v) => { değişken p: Fat32Bpb = v; ver p.toplam_sektor == 60000; } }'''
for val,variant in [(0,'Bos'),(0x0ffffff7,'Bozuk'),(0x0ffffff8,'ZincirSonu'),(0xffffffff,'ZincirSonu')]:
 cases['fat_'+str(val)]=f'''    değişken f: Dizi<dtam8> = bayt_sifir(12); yaz_le32(f, 8, {val});
    eşleş fat_girdi(f, 2) {{ FatGirdi::{variant} => {{ ver doğru; }} _ => {{ ver yanlış; }} }}'''
cases['maske']=f'''    değişken f: Dizi<dtam8> = bayt_sifir(12); yaz_le32(f, 8, {0xf0000003});
    eşleş fat_girdi(f, 2) {{ FatGirdi::Sonraki(v) => {{ ver v == 3; }} _ => {{ ver yanlış; }} }}'''
for i in (-1,3,9223372036854775807):cases['sinir_'+str(i).replace('-','eksi')]=f'    eşleş fat_girdi(bayt_sifir(12), {i}) {{ FatGirdi::Bozuk => {{ ver doğru; }} _ => {{ ver yanlış; }} }}'
cases['zincir']='''    değişken f: Dizi<dtam8> = bayt_sifir(20);
    yaz_le32(f, 8, 3); yaz_le32(f, 12, 4); yaz_le32(f, 16, 268435448);
    eşleş zincir(f, 2, 3) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken xs: Dizi<tam64> = v;
        ver dizi_boyut(xs) == 3 ve dizi_al(xs, 0) == 2 ve dizi_al(xs, 1) == 3 ve dizi_al(xs, 2) == 4;
    } }'''
cases['dongu']='''    değişken f: Dizi<dtam8> = bayt_sifir(16); yaz_le32(f, 8, 3); yaz_le32(f, 12, 2);
    eşleş zincir(f, 2, 4) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "dongu"); } }'''
for val in (0,0x0ffffff7,100):
 cases['bozuk_'+str(val)]=f'''    değişken f: Dizi<dtam8> = bayt_sifir(12); yaz_le32(f, 8, {val});
    eşleş zincir(f, 2, 5) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "bozuk zincir"); }} }}'''
cases['azami']='    eşleş zincir(bayt_sifir(0), 2, 0) { tamam(v) => { ver yanlış; } hata(e) => { ver metin_esit(e, "dongu"); } }'
testler('tests/drivers/fs/fat32_bpb_test.kem',header,cases)
print(len(cases),'test')
