from yardim import testler
header='''// Pikseller: python3 tools/codex/d3.py; (glif[row*ceil(w/8)+x/8] >> (7-x%8))&1.
kullan drivers::ekran::cerceve::{ Cerceve, cerceve_yeni, piksel_oku };
kullan drivers::ekran::psf::{ PsfFont, psf_coz };
kullan drivers::ekran::yazi::{ glif_ciz, kod_ciz, metin_ciz };
kullan tests::drivers::ekran::veri::psf_sentetik::{ sentetik_font };
kullan drivers::ortak::bayt::{ oku_u8 };
işlev renk_mi(c: Cerceve, x: tam32, y: tam32, k: tam32) -> mantıksal { eşleş piksel_oku(c, x, y) { hiç => { ver yanlış; } değer(v) => { ver v == k; } } }
'''
def wrap(s):return '''    eşleş psf_coz(sentetik_font()) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken f: PsfFont = v;
        eşleş cerceve_yeni(18, 4, 80) { hata(e) => { ver yanlış; } tamam(w) => {
            değişken c: Cerceve = w;
'''+s+'\n        } }\n    } }'
cases={}
# İki glifin bütün görünür pikselleri; satır dolgu bitleri ayrıca sınanır.
glyphs=[[128,128,64,0],[255,128,128,0]]
s='            metin_ciz(c, f, 0, 0, "A😀", 1193046, 11259375);\n'
for gi,raw in enumerate(glyphs):
 for y in range(2):
  for x in range(9):
   on=(raw[y*2+x//8]>>(7-x%8))&1
   s+=f'            eğer değil renk_mi(c, {gi*9+x}, {y}, {0x123456 if on else 0xabcdef}) {{ ver yanlış; }}\n'
cases['tum_pikseller']=wrap(s+'            ver oku_u8(c.veri, 72) == 0 ve oku_u8(c.veri, 152) == 0;')
cases['yedek']=wrap('''            kod_ciz(c, f, 0, 0, 9731, 7, 2);
            ver renk_mi(c, 0, 0, 7) ve renk_mi(c, 1, 0, 2) ve renk_mi(c, 1, 1, 7);''')
cases['soru_yedegi']=wrap('''            değişken kodlar: Dizi<tam32> = [65, 63]; değişken indeksler: Dizi<tam32> = [0, 2];
            f.kodlar = kodlar; f.indeksler = indeksler;
            kod_ciz(c, f, 0, 0, 9731, 7, 2);
            ver renk_mi(c, 0, 0, 7) ve renk_mi(c, 1, 0, 7) ve renk_mi(c, 2, 0, 7) ve renk_mi(c, 3, 0, 2);''')
cases['yedek_yok']=wrap('''            değişken bos: Dizi<tam32> = []; f.kodlar = bos; f.indeksler = bos;
            kod_ciz(c, f, 0, 0, 9731, 7, 2); ver renk_mi(c, 0, 0, 2) ve renk_mi(c, 8, 1, 2);''')
cases['kirpma']=wrap('''            glif_ciz(c, f, 0, -8, -1, 7, 2);
            glif_ciz(c, f, 0, 17, 3, 7, 2);
            glif_ciz(c, f, 0, 2147483647, 2147483647, 7, 2);
            ver renk_mi(c, 0, 0, 2) ve renk_mi(c, 17, 3, 7) ve renk_mi(c, 1, 0, 0) ve oku_u8(c.veri, 312) == 0;''')
cases['satir']=wrap('''            metin_ciz(c, f, 0, 0, metin_birlestir("A", metin_birlestir(kod_metin(10), "ğ")), 7, 2);
            ver renk_mi(c, 0, 0, 7) ve renk_mi(c, 0, 2, 7) ve renk_mi(c, 9, 0, 0);''')
cases['gecersiz_glif']=wrap('''            glif_ciz(c, f, -1, 0, 0, 7, 2); glif_ciz(c, f, 4, 0, 0, 7, 2);
            f.glif_bayt = 1; glif_ciz(c, f, 0, 0, 0, 7, 2);
            ver renk_mi(c, 0, 0, 0);''')
testler('tests/drivers/ekran/yazi_test.kem',header,cases)
print(len(cases),'test; 36 piksel bağımsız hesaplandı')
