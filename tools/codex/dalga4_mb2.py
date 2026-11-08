#!/usr/bin/env python3
"""GNU Multiboot2 §3.6: bağımsız struct.pack bilgi bloğu kâhini."""
from pathlib import Path
import struct


def tag(kind, payload):
    data = struct.pack('<II', kind, len(payload)+8) + payload
    return data + bytes((-len(data)) % 8)


def rsdp(rev):
    b = bytearray(36 if rev == 2 else 20)
    b[:8] = b'RSD PTR '
    b[9:15] = b'KEMGU '
    b[15] = rev
    struct.pack_into('<I', b, 16, 0x12340000)
    b[8] = (-sum(b[:20])) & 255
    if rev == 2:
        struct.pack_into('<I', b, 20, 36)
        struct.pack_into('<Q', b, 24, 0x123456789)
        b[32] = (-sum(b)) & 255
    return bytes(b)


parts = [(1,b'kok=1A2B-3C4D sessiz hata=ayikla\0'),(2,b'GRUB\0'),
         (3,struct.pack('<II',0x100000,0x120000)+b'ramdisk\0'),
         (4,struct.pack('<II',640,1048576)),
         (6,struct.pack('<IIQQII',24,0,0x100000,0x400000,1,0)),
         (8,struct.pack('<QIIIBBH6B',0x100000000,4096,1024,768,32,1,0,16,8,8,8,0,8)),
         (14,rsdp(0)),(15,rsdp(2)),(999,b'abc'),(0,b'')]
b = bytearray(8)
positions = {}
for kind,payload in parts:
    positions[kind] = len(b)
    b += tag(kind,payload)
struct.pack_into('<II',b,0,len(b),0)
print('boy=',len(b),'etiketler=',positions,'kok=',int('1A2B3C4D',16))
out = Path('tests/drivers/acilis');out.mkdir(parents=True,exist_ok=True)
literal = ',\n        '.join(','.join(f'{x} olarak dtam8' for x in b[i:i+16]) for i in range(0,len(b),16))
(out/'mb2_fikstur.kem').write_text('// Üretim: python3 tools/codex/dalga4_mb2.py\ngenel işlev mb2_ornek() -> Dizi<dtam8> {\n    değişken b: Dizi<dtam8> = [\n        '+literal+'\n    ]; ver b;\n}\n',encoding='utf-8')
mutations=[(4,1),(0,7),(12,0),(positions[3]+12,0),(positions[6]+8,20),(positions[8]+32,40),(positions[15]+16,1),(positions[0],1)]
mut = '\n'.join(f'    b=mb2_ornek(); yaz_le32(b,{o},{v}); eğer değil reddet(b) {{ ver yanlış; }}' for o,v in mutations)
(out/'multiboot2_test.kem').write_text('''// Kâhin: python3 tools/codex/dalga4_mb2.py (struct.pack, ACPI sağlama, hizalama).
kullan tests::drivers::acilis::mb2_fikstur::{ mb2_ornek };
kullan drivers::acilis::multiboot2::{ AcilisBilgi, AcilisModulu, AcilisEkrani, KomutSecenegi, multiboot2_coz, komut_satiri_coz, kok_seri_coz };
kullan drivers::bellek::harita::{ BellekBolgesi };
kullan drivers::acpi::tablo::{ Rsdp };
kullan drivers::ortak::bayt::{ yaz_le32 };
işlev reddet(b: Dizi<dtam8>) -> mantıksal { eşleş multiboot2_coz(b) { tamam(v) => { ver yanlış; } hata(e) => { ver doğru; } } }
işlev test_tam() -> mantıksal {
    eşleş multiboot2_coz(mb2_ornek()) {
        hata(e) => { yazdir_metin(e); ver yanlış; }
        tamam(b) => {
            eğer değil metin_esit(b.acici,"GRUB") veya b.alt_bellek != 640 veya b.ust_bellek != 1048576 { ver yanlış; }
            eğer dizi_boyut(b.moduller) != 1 veya dizi_boyut(b.harita) != 1 { ver yanlış; }
            değişken m: AcilisModulu = dizi_al(b.moduller,0);
            eğer m.baslangic != 1048576 veya m.bitis != 1179648 veya değil metin_esit(m.ad,"ramdisk") { ver yanlış; }
            değişken h: BellekBolgesi = dizi_al(b.harita,0);
            eğer h.taban != 1048576 veya h.uzunluk != 4194304 veya h.tur != 1 { ver yanlış; }
            eşleş b.ekran { hiç => { ver yanlış; } değer(e) => { eğer e.adres != 4294967296 veya e.genislik != 1024 veya e.yukseklik != 768 veya e.adim != 4096 veya e.bpp != 32 veya dizi_al(e.renk,0) != 16 { ver yanlış; } } }
            eşleş b.acpi { hiç => { ver yanlış; } değer(a) => { eğer a.revizyon != 2 veya a.xsdt != 4886718345 { ver yanlış; } } }
            eşleş b.kok { hiç => { ver yanlış; } değer(k) => { ver k == 439041101; } }
        }
    }
}
işlev test_kisa() -> mantıksal {
    değişken tam: Dizi<dtam8> = mb2_ornek(); değişken b: Dizi<dtam8> = [];
    değişken i: tam32 = 0;
    iken i < dizi_boyut(tam) { eğer değil reddet(b) { ver yanlış; } dizi_ekle(b,dizi_al(tam,i)); i=i+1; }
    ver doğru;
}
işlev test_bozuk() -> mantıksal {
    değişken b: Dizi<dtam8> = [];
'''+mut+'''
    ver doğru;
}
işlev test_komut() -> mantıksal {
    eşleş komut_satiri_coz("kok=1A2B-3C4D sessiz hata=ayikla") {
        hata(e) => { ver yanlış; }
        tamam(xs) => { eğer dizi_boyut(xs) != 3 { ver yanlış; } değişken x: KomutSecenegi = dizi_al(xs,1); eğer değil metin_esit(x.anahtar,"sessiz") veya değil metin_esit(x.deger,"") { ver yanlış; } }
    }
    eşleş kok_seri_coz("1a2b3c4d") { hata(e) => { ver yanlış; } tamam(v) => { eğer v != 439041101 { ver yanlış; } } }
    eşleş komut_satiri_coz("kok=GGGG0000") { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş komut_satiri_coz("kok=12345678 kok=12345678") { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş komut_satiri_coz("=x") { tamam(v) => { ver yanlış; } hata(e) => { } }
    eşleş kok_seri_coz("1-2-3-4-5-6-7-8") { tamam(v) => { ver yanlış; } hata(e) => { } }
    ver doğru;
}
işlev main() -> tam32 {
    değişken h: tam32 = 0;
    eğer değil test_tam() { yazdir_metin("tam"); h=h+1; }
    eğer değil test_kisa() { yazdir_metin("kisa"); h=h+1; }
    eğer değil test_bozuk() { yazdir_metin("bozuk"); h=h+1; }
    eğer değil test_komut() { yazdir_metin("komut"); h=h+1; }
    ver h;
}
''',encoding='utf-8')
