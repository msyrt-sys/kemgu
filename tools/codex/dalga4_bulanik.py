"""Yeni ayrıştırıcılara deterministik LCG ve tek bayt mutasyonu testini üretir."""
from pathlib import Path
s='''// Dalga 4 F1: 300 rastgele + her geçerli örnekte 300 tek-bayt mutasyon.
kullan drivers::ortak::bayt::{ bayt_sifir, oku_u8, oku_le64, yaz_u8, yaz_le32, yaz_be32, oku_be16, oku_be32, oku_be64 };
kullan drivers::acilis::multiboot2::{ AcilisBilgi, AcilisBellek, AcilisModulu, AcilisEkrani, AcilisRsdp, KomutSecenegi, multiboot2_coz, komut_satiri_coz, kok_seri_coz };
kullan drivers::nvme::kimlik::{ NvmeDenetleyici, NvmeAdAlani, nvme_denetleyici_coz, nvme_ad_alani_coz };
kullan drivers::nvme::komut::{ NvmeTamamlanma, nvme_cq_coz };
kullan drivers::nvme::kayit::{ NvmeCap, nvme_cap_coz };
kullan drivers::ahci::ata_kimlik::{ AtaKimlik, ata_kimlik_coz };
kullan drivers::ahci::kayit::{ AhciCap, ahci_cap_coz } olarak ak;
kullan drivers::usb::tanim::{ UsbAygit, UsbYapilandirma, UsbArayuz, UsbUc, UsbHid, UsbHidEk, usb_aygit_coz, usb_yapilandirma_coz };
kullan drivers::usb::xhci_kayit::{ XhciYetenek, XhciPort, xhci_yetenek_coz, xhci_portsc_coz };
kullan drivers::usb::xhci_trb::{ XhciTrb, XhciOlay, xhci_trb_coz, xhci_olay_coz };
kullan drivers::usb::depo::{ ScsiKapasite, ScsiDuyu, bot_csw, scsi_kapasite, scsi_duyu };
kullan drivers::usb::hid_fare::{ hid_fare_rapor };
kullan drivers::ps2::fare::{ FareOlayi };
kullan drivers::usb::hid_klavye::{ HidKlavye, hid_klavye_yeni, hid_klavye_rapor };
kullan drivers::ps2::klavye_tr::{ KlavyeDurumu, TusOlayi, klavye_yeni };
kullan tests::drivers::acilis::mb2_fikstur::{ mb2_ornek };
kullan tests::drivers::nvme::kimlik_fikstur::{ denetleyici_ornek, ad_alani_ornek };
kullan tests::drivers::ahci::kimlik_fikstur::{ ata_ornek } olarak af;
kullan tests::drivers::usb::tanim_fikstur::{ usb_aygit_ornek, usb_yapi_ornek };
kullan tests::drivers::usb::xhci_fikstur::{ xhci_cap_ornek };
kullan tests::drivers::usb::trb_fikstur::{ normal_trb_ornek, olay_trb_ornek };
yapı Tohum { x: tam64; }
işlev rastgele(s: &değişken Tohum) -> tam32 { s.x=(s.x*1103515245+12345)&2147483647; ver s.x olarak tam32; }
işlev dene(b: Dizi<dtam8>) -> mantıksal {
    multiboot2_coz(b); nvme_denetleyici_coz(b); nvme_ad_alani_coz(b); nvme_cq_coz(b); ata_kimlik_coz(b);
    usb_aygit_coz(b); usb_yapilandirma_coz(b); xhci_yetenek_coz(b,65536); xhci_trb_coz(b); xhci_olay_coz(b); bot_csw(b,7,512); scsi_kapasite(b); scsi_duyu(b);
    oku_be16(b,0); oku_be32(b,0); oku_be64(b,0);
    değişken h: HidKlavye = hid_klavye_yeni(); değişken k: KlavyeDurumu = klavye_yeni(); hid_klavye_rapor(&değişken h,&değişken k,b); hid_fare_rapor(b);
    eğer dizi_boyut(b) >= 8 { değişken v: tam64 = oku_le64(b,0); nvme_cap_coz(v); ahci_cap_coz(v); xhci_portsc_coz(v); }
    ver doğru;
}
işlev mutasyon(b: Dizi<dtam8>) -> mantıksal {
    değişken s: Tohum = Tohum { x: 456 }; değişken i: tam32 = 0;
    iken i < 300 { değişken yer: tam32 = rastgele(&değişken s)%dizi_boyut(b); değişken eski: tam32 = oku_u8(b,yer); değişken fark: tam32 = 1+rastgele(&değişken s)%255; yaz_u8(b,yer,(eski^fark) olarak tam64); dene(b); yaz_u8(b,yer,eski olarak tam64); i=i+1; } ver i == 300;
}
işlev main() -> tam32 {
    eşleş usb_aygit_coz(usb_aygit_ornek()) { hata(e) => { ver 1; } tamam(v) => { eğer v.vid != 4660 veya v.pid != 22136 { ver 2; } } }
    dene(bayt_sifir(0)); dene(bayt_sifir(1));
    değişken s: Tohum = Tohum { x: 123 }; değişken i: tam32 = 0;
    iken i < 300 { değişken n: tam32 = rastgele(&değişken s)%4201; değişken b: Dizi<dtam8> = []; değişken j: tam32 = 0; iken j < n { dizi_ekle(b,(rastgele(&değişken s)&255) olarak dtam8); j=j+1; } dene(b); i=i+1; }
'''
for fn in ['mb2_ornek','denetleyici_ornek','ad_alani_ornek','ata_ornek','usb_aygit_ornek','usb_yapi_ornek','xhci_cap_ornek','normal_trb_ornek','olay_trb_ornek']:
 s+=f'    eğer değil mutasyon({fn}()) {{ ver 3; }}\n'
s+='''    değişken csw: Dizi<dtam8> = bayt_sifir(13); yaz_le32(csw,0,1396855637); yaz_le32(csw,4,7); mutasyon(csw);
    değişken cap: Dizi<dtam8> = bayt_sifir(8); yaz_be32(cap,0,79999); yaz_be32(cap,4,512); mutasyon(cap);
    değişken sense: Dizi<dtam8> = bayt_sifir(18); yaz_u8(sense,0,112); yaz_u8(sense,7,10); mutasyon(sense);
    mutasyon(bayt_sifir(8)); mutasyon(bayt_sifir(4));
    değişken harf: metin = "0123456789abcdef =-kok"; i=0;
    iken i < 300 { değişken m: metin = ""; değişken n: tam32 = rastgele(&değişken s)%80; değişken j: tam32 = 0; iken j < n { m=metin_birlestir(m,metin_kes(harf,rastgele(&değişken s)%22,1)); j=j+1; } komut_satiri_coz(m); kok_seri_coz(m); i=i+1; }
    ver 0;
}
'''
# Her ayrıştırıcı rastgele ve geçerli temel girdiden mutasyon alır.
Path('tests/drivers/bulanik/dalga4_test.kem').write_text(s)
print('örnekler',300+14*300,'metin',300,'LCG ilk', (123*1103515245+12345)&2147483647)
