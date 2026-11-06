from pathlib import Path
from yardim import yaz,testler
# Geçerli fikstür gövdelerini, daha önce bağımsız kahinle doğrulanan testlerden alır.
fixture='''// Kaynaklar tools/codex/e2.py içinde; mevcut Python kahin fikstürleri aynen kullanılır.
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, yaz_le32, yaz_le64, yaz_ascii, bayt_toplam8 };
'''
sources=[('disk/bolum','test_diski'),('fs/fat32_bpb','bpb_verisi'),('fs/fat32_dizin','uzun_veri'),('acpi/tablo','rsdp2'),('acpi/tablo','sdt_verisi'),('acpi/madt','madt_verisi'),('acpi/fadt','fadt_verisi'),('pci/yapilandirma','pci_verisi'),('bellek/harita','e820_veri'),('bellek/harita','mb2_veri'),('elf/elf64','elf_verisi')]
for file,fn in sources:
 s=Path('tests/drivers/'+file+'_test.kem').read_text();a=s.index('işlev '+fn+'(');b=s.index('{',a);depth=1;i=b+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 fixture+='genel '+s[a:i]+'\n'
yaz('tests/drivers/bulanik/veri/fikstur.kem',fixture)
common='''// Deterministik panik-yok kahini: python3 tools/codex/e2.py; her döngü tam 300 örnek.
kullan drivers::ortak::bayt::{ bayt_sifir, oku_u8, yaz_u8 };
yapı Tohum { x: tam64; }
işlev rastgele(s: &değişken Tohum) -> tam32 {
    s.x = (s.x * 1103515245 + 12345) & 2147483647; ver s.x olarak tam32;
}
'''
# ad, içe aktarma, bütün hedef çağrıları, geçerli fikstürler ve başarı sınaması
specs=[
 ('bolum','kullan drivers::disk::bolum::{ Bolum, BolumHatasi, mbr_coz, gpt_coz, gpt_mi };','mbr_coz(b); gpt_mi(b); gpt_coz(b, 512);',[('gpt','test_diski()','gpt_coz(b, 512)')]),
 ('bpb','kullan drivers::fs::fat32_bpb::{ Fat32Bpb, BpbHatasi, bpb_coz, FatGirdi, fat_girdi, zincir };','bpb_coz(b); fat_girdi(b, 2 olarak tam64); zincir(b, 2 olarak tam64, 100);',[('bpb','bpb_verisi()','bpb_coz(b)')]),
 ('dizin','kullan drivers::fs::fat32_dizin::{ DizinGirdisi, dizin_coz };','dizin_coz(b);',[('lfn','uzun_veri()','dizin_coz(b)')]),
 ('acpi','kullan drivers::acpi::tablo::{ Rsdp, SdtBaslik, AcpiHatasi, rsdp_coz, rsdp_ara, sdt_coz, sdt_girdileri };','rsdp_coz(b); rsdp_ara(b); sdt_coz(b); sdt_girdileri(b, doğru); sdt_girdileri(b, yanlış);',[('rsdp','rsdp2()','rsdp_coz(b)'),('xsdt','sdt_verisi(doğru)','sdt_coz(b)'),('rsdt','sdt_verisi(yanlış)','sdt_coz(b)')]),
 ('madt','kullan drivers::acpi::tablo::{ SdtBaslik, AcpiHatasi };\nkullan drivers::acpi::madt::{ Madt, MadtGirdisi, madt_coz };','madt_coz(b);',[('madt','madt_verisi()','madt_coz(b)')]),
 ('fadt','kullan drivers::acpi::tablo::{ SdtBaslik, AcpiHatasi };\nkullan drivers::acpi::fadt::{ Fadt, Gas, fadt_coz };','fadt_coz(b);',[('fadt','fadt_verisi()','fadt_coz(b)')]),
 ('pci','kullan drivers::pci::yapilandirma::{ PciAygit, PciBar, pci_coz };','pci_coz(b);',[('pci','pci_verisi()','pci_coz(b)')]),
 ('harita','kullan drivers::bellek::harita::{ BellekBolgesi, e820_coz, mb2_coz };','e820_coz(b); mb2_coz(b);',[('e820','e820_veri()','e820_coz(b)'),('mb2','mb2_veri()','mb2_coz(b)')]),
 ('elf','kullan drivers::elf::elf64::{ Elf64, ElfSegment, elf64_coz };','elf64_coz(b);',[('elf','elf_verisi()','elf64_coz(b)')]),
 ('ustar','kullan drivers::fs::ustar::{ TarGirdisi, tar_listele, tar_bul };\nkullan tests::drivers::fs::veri::ustar_veri::{ tar_imaj };','tar_listele(b); tar_bul(b, "boot/ğ.txt");',[('ustar','tar_imaj()','tar_listele(b)')]),
 ('psf','kullan drivers::ekran::psf::{ PsfFont, psf_coz };\nkullan tests::drivers::ekran::veri::psf_sentetik::{ sentetik_font };','psf_coz(b);',[('psf','sentetik_font()','psf_coz(b)')]),
 ('bmp','kullan drivers::ekran::cerceve::{ Cerceve };\nkullan drivers::ekran::bmp::{ Bmp, bmp_coz };\nkullan tests::drivers::ekran::veri::bmp_fikstur::{ alt24, maske32 };','bmp_coz(b);',[('24','alt24()','bmp_coz(b)'),('32','maske32()','bmp_coz(b)')]),
]
randomcase='''    değişken s: Tohum = Tohum { x: 123 }; değişken i: tam32 = 0;
    iken i < 300 {
        değişken n: tam32 = rastgele(&değişken s) % 601; değişken b: Dizi<dtam8> = []; değişken j: tam32 = 0;
        iken j < n { dizi_ekle(b, (rastgele(&değişken s) & 255) olarak dtam8); j = j + 1; }
        eğer değil dene(b) { ver yanlış; } i = i + 1;
    }
    ver i == 300;'''
for name,imports,calls,fixtures in specs:
 header=common+imports+'\nkullan tests::drivers::bulanik::veri::fikstur::{ '+', '.join(fn for _,fn in sources)+' };\n'
 header+='işlev dene(b: Dizi<dtam8>) -> mantıksal { '+calls+' ver doğru; }\n'
 cases={'sifir_ve_bir':'    ver dene(bayt_sifir(0)) ve dene(bayt_sifir(1));','rastgele':randomcase}
 for key,make,check in fixtures:
  yes,no=('değer','hiç') if name=='pci' else ('tamam','hata')
  match=f'eşleş {check} {{ {yes}(v) => {{ }} '+('hiç' if no=='hiç' else 'hata(e)')+' => { ver yanlış; } }'
  cases['mutasyon_'+key]=f'''    değişken b: Dizi<dtam8> = {make}; {match}
    değişken s: Tohum = Tohum {{ x: 456 }}; değişken i: tam32 = 0;
    iken i < 300 {{
        değişken yer: tam32 = rastgele(&değişken s) % dizi_boyut(b);
        değişken eski: tam32 = oku_u8(b, yer); değişken fark: tam32 = 1 + rastgele(&değişken s) % 255;
        yaz_u8(b, yer, (eski ^ fark) olarak tam64);
        eğer değil dene(b) {{ ver yanlış; }}
        yaz_u8(b, yer, eski olarak tam64); i = i + 1;
    }} ver i == 300;'''
 testler('tests/drivers/bulanik/'+name+'_test.kem',header,cases)
# VT akışında güvenlik yanında ekran imleci ve tampon sınırları da korunmalı.
header=common+'''kullan drivers::ekran::konsol::{ Konsol, konsol_yeni };
kullan drivers::ekran::vt::{ VtDurumu, vt_yeni, vt_bayt, vt_bitir };
işlev dene(b: Dizi<dtam8>) -> mantıksal {
    eşleş konsol_yeni(16, 4, 7, 0) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken k: Konsol = v; değişken d: VtDurumu = vt_yeni(7, 0); değişken i: tam32 = 0;
        iken i < dizi_boyut(b) { vt_bayt(&değişken d, &değişken k, oku_u8(b, i)); i = i + 1; }
        vt_bitir(&değişken d, &değişken k);
        ver k.x >= 0 ve k.x < 16 ve k.y >= 0 ve k.y < 4 ve dizi_boyut(d.utf8) == 0 ve dizi_boyut(d.parametreler) <= 16;
    } }
}
'''
raw=b'\x1b[2J\x1b[1;1H'+ 'ğ😀'.encode()+b'\x1b[31mABC\x1b[0m\n'
case=f'''    değişken b: Dizi<dtam8> = [{', '.join(str(v)+' olarak dtam8' for v in raw)}];
    eğer değil dene(b) {{ ver yanlış; }} değişken s: Tohum = Tohum {{ x: 456 }}; değişken i: tam32 = 0;
    iken i < 300 {{ değişken yer: tam32 = rastgele(&değişken s) % dizi_boyut(b); değişken eski: tam32 = oku_u8(b, yer);
        değişken fark: tam32 = 1 + rastgele(&değişken s) % 255; yaz_u8(b, yer, (eski ^ fark) olarak tam64);
        eğer değil dene(b) {{ ver yanlış; }} yaz_u8(b, yer, eski olarak tam64); i = i + 1;
    }} ver i == 300;'''
testler('tests/drivers/bulanik/vt_test.kem',header,{'sifir_ve_bir':'    ver dene(bayt_sifir(0)) ve dene(bayt_sifir(1));','rastgele':randomcase,'mutasyon':case})
p=Path('Makefile');s=p.read_text();s=s.replace('tests/drivers/sim/*_test.kem))','tests/drivers/sim/*_test.kem tests/drivers/bulanik/*_test.kem))');p.write_text(s)
print('13 ayrıştırıcı, 3900 rastgele +',sum(len(x[3]) for x in specs)*300+300,'tek-bayt mutasyonu')
