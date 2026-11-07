"""Protokol sınır tabloları; Python tamsayıları ile taşmasız bağımsız beklentiler."""
from pathlib import Path
M=2**63-1
s='''// Python üretimi: dalga4_sinir.py. Protokol hizalama/genişlik/sınır tablosu.
kullan drivers::nvme::komut::{ NvmePrp, PrpSayfa, nvme_prp_kur } olarak nk;
kullan drivers::ahci::komut::{ ahci_fis_kur, ahci_prdt_kur } olarak ak;
kullan drivers::usb::depo::{ scsi_rw10, UsbDepo, usb_depo_yeni };
kullan drivers::usb::xhci_trb::{ xhci_normal, xhci_slot_baglam, xhci_uc_baglam };
kullan drivers::usb::tanim::{ usb_setup };
kullan drivers::ortak::bayt::{ bayt_sifir, oku_u8, oku_be16, oku_be32, oku_be64, yaz_be16, yaz_be32, yaz_be64 };
işlev prp(a: tam64, n: tam64, p: tam32, l: tam64) -> mantıksal { eşleş nvme_prp_kur(a,n,p,l) { tamam(v) => { ver doğru; } hata(e) => { ver yanlış; } } }
işlev fis(l: tam64, n: tam32) -> mantıksal { eşleş ahci_fis_kur(37,l,n) { tamam(v) => { ver doğru; } hata(e) => { ver yanlış; } } }
işlev scsi(l: tam64, n: tam32) -> mantıksal { eşleş scsi_rw10(yanlış,l,n) { tamam(v) => { ver doğru; } hata(e) => { ver yanlış; } } }
işlev normal(a: tam64, n: tam32) -> mantıksal { eşleş xhci_normal(a,n,doğru,doğru) { tamam(v) => { ver doğru; } hata(e) => { ver yanlış; } } }
işlev paket(a: tam32, v: tam32) -> mantıksal { eşleş usb_setup(a,1,v,0,0) { tamam(b) => { ver doğru; } hata(e) => { ver yanlış; } } }
işlev main() -> tam32 {
'''
checks=[]
# NVMe PRP: data range and page/list alignment; disjoint list at 32 MiB.
for a,n,p in [(4096,1,4096),(4097,1,4096),(4096,0,4096),(4096,4096,4096),(4092,8,4096),(4096,16777216,4096),(4096,16777217,4096),(4096,1,2048),(4096,1,65536),(4096,1,131072),(M-3,3,4096),(M-3,4,4096),(M-12287,12287,4096)]:
 ok=a>0 and a%4==0 and 1<=n<=2**24 and a+n<=M and p in [4096,8192,16384,32768,65536]
 checks.append((f'prp({a},{n},{p},33554432)',ok))
for l,n in [(0,0),(0,1),(0,65536),(0,65537),(2**48-1,1),(2**48-1,2),(2**48,1),(-1,1)]:checks.append((f'fis({l},{n})',0<=l<2**48 and 1<=n<=65536 and l+n<=2**48))
for l,n in [(0,0),(0,1),(0,65535),(0,65536),(2**32-1,1),(2**32-1,2),(2**32,1),(-1,1)]:checks.append((f'scsi({l},{n})',0<=l<2**32 and 1<=n<=65535 and l+n<=2**32))
for a,n in [(65536,65536),(65536,65537),(131071,1),(131071,2),(M-1,1),(M-1,2),(0,1),(-1,1),(65536,0)]:checks.append((f'normal({a},{n})',a>=0 and 1<=n<=65536 and a+n<=M and a%65536+n<=65536))
for a,v in [(0,0),(255,65535),(256,0),(-1,0),(0,65536),(0,-1)]:checks.append((f'paket({a},{v})',0<=a<=255 and 0<=v<=65535))
for i,(expr,ok) in enumerate(checks,1):s+=f'    eğer {expr} != {"doğru" if ok else "yanlış"} {{ ver {i}; }}\n'
s+='''    // Hata veren BE yazısı tamponu değiştiremez.
    değişken b: Dizi<dtam8> = bayt_sifir(8); değişken i: tam32 = 0;
    eşleş yaz_be64(b,0,81985529216486895) { hata(e) => { ver 50; } tamam(v) => { } }
    değişken bek: Dizi<tam32> = [1,35,69,103,137,171,205,239];
    eşleş yaz_be32(b,5,1) { tamam(v) => { ver 51; } hata(e) => { } }
    eşleş yaz_be16(b,0,65536) { tamam(v) => { ver 52; } hata(e) => { } }
    eşleş yaz_be32(b,-1,1) { tamam(v) => { ver 53; } hata(e) => { } }
    eşleş yaz_be32(b,0,4294967296) { tamam(v) => { ver 68; } hata(e) => { } }
    eşleş yaz_be32(b,0,-1) { tamam(v) => { ver 69; } hata(e) => { } }
    eşleş yaz_be16(b,0,-1) { tamam(v) => { ver 70; } hata(e) => { } }
    iken i < 8 { eğer oku_u8(b,i) != dizi_al(bek,i) { ver 54; } i=i+1; }
    eşleş oku_be16(b,6) { hata(e) => { ver 55; } tamam(v) => { eğer v != 52719 { ver 56; } } }
    eşleş oku_be32(b,4) { hata(e) => { ver 57; } tamam(v) => { eğer v != 2309737967 { ver 58; } } }
    eşleş oku_be64(b,0) { hata(e) => { ver 59; } tamam(v) => { eğer v != 81985529216486895 { ver 60; } } }
    eşleş oku_be64(b,1) { tamam(v) => { ver 61; } hata(e) => { } }
    eşleş xhci_slot_baglam(64,1048575,15,127,31) { hata(e) => { ver 62; } tamam(v) => { } }
    eşleş xhci_slot_baglam(64,1048576,15,127,31) { tamam(v) => { ver 63; } hata(e) => { } }
    eşleş xhci_uc_baglam(32,7,1024,255,15,65536,doğru) { hata(e) => { ver 64; } tamam(v) => { } }
    eşleş xhci_uc_baglam(32,7,1025,255,15,65536,doğru) { tamam(v) => { ver 65; } hata(e) => { } }
    eşleş usb_depo_yeni(65536,8196,4152,255,15,143,255) { hata(e) => { ver 66; } tamam(v) => { } }
    eşleş usb_depo_yeni(65536,8196,4152,256,15,143,255) { tamam(v) => { ver 67; } hata(e) => { } }
    ver 0;
}
'''
assert int.from_bytes(bytes([1,35,69,103,137,171,205,239]),'big')==81985529216486895
print('tablo',len(checks),'BE16',int.from_bytes(bytes([205,239]),'big'),'BE32',int.from_bytes(bytes([137,171,205,239]),'big'))
Path('tests/drivers/sinir/dalga4_test.kem').write_text(s)
