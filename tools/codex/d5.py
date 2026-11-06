from yardim import testler
palette=[0x000000,0xaa0000,0x00aa00,0xaa5500,0x0000aa,0xaa00aa,0x00aaaa,0xaaaaaa,0x555555,0xff5555,0x55ff55,0xffff55,0x5555ff,0xff55ff,0x55ffff,0xffffff]
print('Palet',palette)
header='''// Vektörler: python3 tools/codex/d5.py; ANSI VGA16 RGB paleti, ord ve UTF8 encode.
kullan drivers::ekran::konsol::{ Konsol, konsol_yeni, konsol_metin, konsol_satir };
kullan drivers::ekran::vt::{ VtDurumu, vt_yeni, vt_bayt, vt_metin, vt_bitir };
işlev csi(d: &değişken VtDurumu, k: &değişken Konsol, s: metin) {
    vt_bayt(d, k, 27); vt_bayt(d, k, 91); vt_metin(d, k, s);
}
'''
def wrap(s):return '''    eşleş konsol_yeni(16, 4, 11184810, 0) { hata(e) => { ver yanlış; } tamam(v) => {
        değişken k: Konsol = v; değişken d: VtDurumu = vt_yeni(11184810, 0);
'''+s+'\n    } }'
cases={}
cases['imlec']=wrap('''        csi(&değişken d, &değişken k, "3;4H");
        eğer k.x != 3 veya k.y != 2 { ver yanlış; }
        csi(&değişken d, &değişken k, "A"); csi(&değişken d, &değişken k, "2C");
        eğer k.x != 5 veya k.y != 1 { ver yanlış; }
        csi(&değişken d, &değişken k, "B"); csi(&değişken d, &değişken k, "D");
        eğer k.x != 4 veya k.y != 2 { ver yanlış; }
        csi(&değişken d, &değişken k, "H"); ver k.x == 0 ve k.y == 0;''')
cases['sinir']=wrap('''        csi(&değişken d, &değişken k, "999999999999C"); csi(&değişken d, &değişken k, "999B");
        eğer k.x != 15 veya k.y != 3 { ver yanlış; }
        csi(&değişken d, &değişken k, "999D"); csi(&değişken d, &değişken k, "999A"); ver k.x == 0 ve k.y == 0;''')
cases['sil']=wrap('''        vt_metin(&değişken d, &değişken k, "ABCDEFG"); csi(&değişken d, &değişken k, "1;3H"); csi(&değişken d, &değişken k, "K");
        eğer değil metin_esit(konsol_satir(k, 0), "AB              ") { ver yanlış; }
        csi(&değişken d, &değişken k, "2J");
        ver k.x == 2 ve k.y == 0 ve metin_esit(konsol_satir(k, 0), "                ");''')
cases['sgr']=wrap(f'''        csi(&değişken d, &değişken k, "31;44m"); vt_metin(&değişken d, &değişken k, "A");
        csi(&değişken d, &değişken k, "1m"); vt_metin(&değişken d, &değişken k, "B");
        csi(&değişken d, &değişken k, "92m"); vt_metin(&değişken d, &değişken k, "C");
        csi(&değişken d, &değişken k, "0m"); vt_metin(&değişken d, &değişken k, "D");
        ver dizi_al(k.renkler, 0) == {palette[1]} ve dizi_al(k.arkalar, 0) == {palette[4]} ve
            dizi_al(k.renkler, 1) == {palette[9]} ve dizi_al(k.renkler, 2) == {palette[10]} ve
            dizi_al(k.renkler, 3) == {palette[7]} ve dizi_al(k.arkalar, 3) == 0;''')
cases['bilinmeyen']=wrap('''        csi(&değişken d, &değişken k, "123Z"); csi(&değişken d, &değişken k, "?25h");
        vt_bayt(&değişken d, &değişken k, 27); vt_metin(&değişken d, &değişken k, "]0;baslik"); vt_bayt(&değişken d, &değişken k, 7);
        vt_metin(&değişken d, &değişken k, "X");
        ver k.x == 1 ve dizi_al(k.kodlar, 0) == 88;''')
cases['st']=wrap('''        vt_bayt(&değişken d, &değişken k, 27); vt_metin(&değişken d, &değişken k, "Pcop");
        vt_bayt(&değişken d, &değişken k, 27); vt_bayt(&değişken d, &değişken k, 92); vt_metin(&değişken d, &değişken k, "X");
        ver k.x == 1 ve dizi_al(k.kodlar, 0) == 88;''')
raw='ğ😀'.encode();s=''
for i,b in enumerate(raw):
 s+=f'        vt_bayt(&değişken d, &değişken k, {b});\n'
 if i in (0,2,3,4):s+=f'        eğer k.x != {0 if i==0 else 1} {{ ver yanlış; }}\n'
cases['utf8']=wrap(s+f'        ver k.x == 2 ve dizi_al(k.kodlar, 0) == {ord("ğ")} ve dizi_al(k.kodlar, 1) == {ord("😀")};')
cases['bozuk_utf8']=wrap('''        vt_bayt(&değişken d, &değişken k, 224); vt_bayt(&değişken d, &değişken k, 128); vt_bayt(&değişken d, &değişken k, 128);
        vt_metin(&değişken d, &değişken k, "A");
        ver k.x == 4 ve dizi_al(k.kodlar, 0) == 65533 ve dizi_al(k.kodlar, 1) == 65533 ve dizi_al(k.kodlar, 2) == 65533 ve dizi_al(k.kodlar, 3) == 65;''')
cases['yarim']=wrap('''        vt_bayt(&değişken d, &değişken k, 240); vt_bayt(&değişken d, &değişken k, 159); vt_bitir(&değişken d, &değişken k);
        csi(&değişken d, &değişken k, "123"); vt_bitir(&değişken d, &değişken k);
        ver k.x == 2 ve dizi_al(k.kodlar, 0) == 65533 ve dizi_al(k.kodlar, 1) == 65533;''')
cases['cok_parametre']=wrap('''        csi(&değişken d, &değişken k, "1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1m"); vt_metin(&değişken d, &değişken k, "X");
        ver k.x == 1 ve k.on == 11184810 ve dizi_al(k.kodlar, 0) == 88;''')
testler('tests/drivers/ekran/vt_test.kem',header,cases)
print(len(cases),'test')
