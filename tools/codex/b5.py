from yardim import testler
import struct
T=70000;k=1;fat=(T-32+2*k+128*k+1)//(128*k+2);first=32+2*fat;clusters=(T-first)//k
print('fat',fat,'ilk',first,'kume',clusters,'boy',(first+k)*512,'serial',int.from_bytes(b'KEMG','little'))
header='''// Hesap: python3 tools/codex/b5.py; kapasite eşitsizliği 128*F >= floor((T-32-2F)/K)+2.
// C001: seçili ithalat T024 verdiği için modülle aynı adlı API nitelikli çağrılır.
kullan drivers::fs::fat32_bicimle;
kullan drivers::fs::fat32_bpb::{ Fat32Bpb, bpb_coz };
kullan drivers::fs::fat32_dizin::{ DizinGirdisi, dizin_coz };
kullan drivers::ortak::bayt::{ oku_u8, oku_le32, imza_esit };
'''
cases={}
cases['yerlesim']=f'''    eşleş fat32_bicimle::fat32_bicimle(70000, 1, "KEMGU") {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken b: Dizi<dtam8> = v;
        eğer dizi_boyut(b) != {(first+k)*512} {{ ver yanlış; }}
        eşleş bpb_coz(b) {{ hata(e) => {{ ver yanlış; }} tamam(p) => {{
            değişken bp: Fat32Bpb = p;
            eğer bp.fat_sektor != {fat} veya bp.ilk_veri_sektoru != {first} veya bp.kume_sayisi != {clusters} {{ ver yanlış; }}
        }} }}
        eğer oku_le32(b, 512) != {0x41615252} veya oku_le32(b, 996) != {0x61417272} veya
            oku_le32(b, 1020) != {0xaa550000} veya oku_le32(b, 1000) != {clusters-1} veya oku_le32(b, 1004) != 3 {{ ver yanlış; }}
        değişken i: tam32 = 0;
        iken i < 1536 {{ eğer oku_u8(b, i) != oku_u8(b, 3072+i) {{ ver yanlış; }} i = i + 1; }}
        i = 0;
        iken i < {fat*512} {{ eğer oku_u8(b, 16384+i) != oku_u8(b, {(32+fat)*512}+i) {{ ver yanlış; }} i = i + 1; }}
        ver oku_le32(b, 16384) == {0x0ffffff8} ve oku_le32(b, 16388) == {0x0fffffff} ve
            oku_le32(b, 16392) == {0x0fffffff} ve oku_le32(b, 16396) == 0 ve
            imza_esit(b, {first*512}, "KEMGU      ") ve oku_u8(b, {first*512+11}) == 8 ve oku_u8(b, {first*512+32}) == 0;
    }} }}'''
cases['etiketsiz']=f'''    eşleş fat32_bicimle::fat32_bicimle(70000, 1, "") {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken b: Dizi<dtam8> = v;
        ver imza_esit(b, 71, "NO NAME    ") ve oku_u8(b, {first*512}) == 0;
    }} }}'''
for i,(total,ks,label) in enumerate([(0,1,''),(1000,1,''),(70000,0,''),(70000,3,''),(70000,256,''),(4294967296,1,''),(70000,1,'123456789012'),(70000,1,'ğ'),(70000,1,'a/b'),(268000000,1,'')]):
 cases['hata_'+str(i)]=f'    eşleş fat32_bicimle::fat32_bicimle({total} olarak tam64, {ks}, "{label}") {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver doğru; }} }}'
cases['iki_sektor']='''    eşleş fat32_bicimle::fat32_bicimle(140000, 2, "disk") { hata(e) => { ver yanlış; } tamam(v) => {
        değişken b: Dizi<dtam8> = v;
        eşleş bpb_coz(b) { hata(e) => { ver yanlış; } tamam(p) => { değişken bp: Fat32Bpb = p; ver bp.kume_sektor == 2 ve imza_esit(b, 71, "DISK       "); } }
    } }'''
testler('tests/drivers/fs/fat32_bicimle_test.kem',header,cases)
print(len(cases),'test')
