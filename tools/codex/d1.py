from yardim import testler
from pathlib import Path
header='''// Beklentiler: python3 tools/codex/d1.py; renk.to_bytes(4,little), kırpılmış koordinatlar.
kullan drivers::ekran::cerceve::{ Cerceve, cerceve_yeni, piksel_yaz, piksel_oku, dikdortgen_doldur, kaydir_yukari, kopyala };
kullan drivers::ortak::bayt::{ bayt_sifir, oku_u8, yaz_u8 };
işlev renk_mi(c: Cerceve, x: tam32, y: tam32, k: tam32) -> mantıksal { eşleş piksel_oku(c, x, y) { hiç => { ver yanlış; } değer(v) => { ver v == k; } } }
'''
cases={}
def wrap(s):return '''    eşleş cerceve_yeni(3, 3, 16) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken c: Cerceve = v;
'''+s+'\n    } }'
color=0x123456
cases['bgrx']=wrap(f'''        piksel_yaz(c, 1, 1, {color});
        ver renk_mi(c, 1, 1, {color}) ve oku_u8(c.veri, 20) == {color&255} ve oku_u8(c.veri, 21) == {(color>>8)&255} ve oku_u8(c.veri, 22) == {(color>>16)&255} ve oku_u8(c.veri, 23) == 0;''')
cases['kirpma']=wrap('''        yaz_u8(c.veri, 12, 99);
        dikdortgen_doldur(c, -1, -1, 3, 3, 7);
        değişken y: tam32 = 0;
        iken y < 3 { değişken x: tam32 = 0; iken x < 3 {
            değişken beklenen: tam32 = 0; eğer x < 2 ve y < 2 { beklenen = 7; }
            eğer değil renk_mi(c, x, y, beklenen) { ver yanlış; } x = x + 1;
        } y = y + 1; }
        ver oku_u8(c.veri, 12) == 99;''')
cases['sinir']=wrap('''        piksel_yaz(c, -1, 0, 7); piksel_yaz(c, 3, 0, 7);
        dikdortgen_doldur(c, 2147483647, 0, 2147483647, 3, 7);
        dikdortgen_doldur(c, -2147483648, 0, 2147483647, 3, 7);
        dikdortgen_doldur(c, 0, 0, -1, 3, 7);
        eşleş piksel_oku(c, 0, 3) { değer(v) => { ver yanlış; } hiç => { } }
        ver renk_mi(c, 0, 0, 0) ve oku_u8(c.veri, 12) == 0;''')
cases['kaydir']=wrap('''        dikdortgen_doldur(c, 0, 0, 3, 1, 1);
        dikdortgen_doldur(c, 0, 1, 3, 1, 2);
        dikdortgen_doldur(c, 0, 2, 3, 1, 3);
        kaydir_yukari(c, 1, 9);
        eğer değil renk_mi(c, 0, 0, 2) veya değil renk_mi(c, 0, 1, 3) veya değil renk_mi(c, 0, 2, 9) { ver yanlış; }
        kaydir_yukari(c, 0, 8);
        eğer değil renk_mi(c, 0, 0, 2) { ver yanlış; }
        kaydir_yukari(c, 99, 8); ver renk_mi(c, 0, 0, 8) ve renk_mi(c, 2, 2, 8);''')
cases['ortusen_kopya']=wrap('''        piksel_yaz(c, 0, 0, 1); piksel_yaz(c, 1, 0, 2); piksel_yaz(c, 2, 0, 3);
        kopyala(c, c, 1, 0);
        ver renk_mi(c, 0, 0, 1) ve renk_mi(c, 1, 0, 1) ve renk_mi(c, 2, 0, 2);''')
cases['negatif_kopya']=wrap('''        piksel_yaz(c, 1, 1, 9);
        kopyala(c, c, -1, -1);
        ver renk_mi(c, 0, 0, 9) ve renk_mi(c, 2, 2, 0);''')
for name,w,h,step in [('negatif',-1,1,4),('adim',3,3,11),('buyuk',16384,16384,65536)]:cases[name]=f'    eşleş cerceve_yeni({w}, {h}, {step}) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver doğru; }} }}'
cases['kisa_veri']='''    değişken veri: Dizi<dtam8> = bayt_sifir(3);
    değişken c: Cerceve = Cerceve { genislik: 1, yukseklik: 1, adim: 4, bpp: 32, veri: veri };
    piksel_yaz(c, 0, 0, 7); kaydir_yukari(c, 1, 7); kopyala(c, c, 0, 0);
    eşleş piksel_oku(c, 0, 0) { hiç => { ver oku_u8(veri, 0) == 0; } değer(v) => { ver yanlış; } }'''
testler('tests/drivers/ekran/cerceve_test.kem',header,cases)
p=Path('Makefile');s=p.read_text(encoding='utf-8');needle='tests/drivers/elf/*_test.kem'
if 'tests/drivers/ekran/*_test.kem' not in s:s=s.replace(needle,needle+' tests/drivers/ekran/*_test.kem')
p.write_text(s,encoding='utf-8');print(len(cases),'test',list(color.to_bytes(4,'little')))
