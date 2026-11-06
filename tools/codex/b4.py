from yardim import testler
header='''// Beklentiler: python3 tools/codex/b4.py; ASCII upper, gövde[:8-len(~N)], ljust.
kullan drivers::fs::fat32_kisa_ad::{ kisa_ad_uret, lfn_gerekli_mi };
işlev ad_esit(ad: metin, mevcut: Dizi<metin>, beklenen: metin) -> mantıksal {
    eşleş kisa_ad_uret(ad, mevcut) { hata(e) => { ver yanlış; } tamam(v) => { ver metin_esit(v, beklenen); } }
}
'''
cases={}
for i,(ad,base,ext,direct) in enumerate([('FOO.TXT','FOO','TXT',True),('README','README','',True),('12345678.ABC','12345678','ABC',True),('foo.txt','FOO','TXT',False),('Uzun Döküman.txt','UZUN_D_K_MAN','TXT',False),(' .foo.txt. ','FOO','TXT',False),('a+b?.text','A_B_','TEXT',False),('😀.txt','_','TXT',False),('a.b.c','A_B','C',False)]):
 expected=base.ljust(8)+ext[:3].ljust(3) if direct else (base[:6]+'~1').ljust(8)+ext[:3].ljust(3)
 assert len(expected)==11
 cases['ad_'+str(i)]=f'''    değişken bos: Dizi<metin> = [];
    ver ad_esit("{ad}", bos, "{expected}") ve lfn_gerekli_mi("{ad}") == {"yanlış" if direct else "doğru"};'''
for count in (1,9,10,99):
 candidates=[('UZUNDO'[:8-len('~'+str(n))]+'~'+str(n)).ljust(8)+'TXT' for n in range(1,count+1)]
 n=count+1;exp=('UZUNDO'[:8-len('~'+str(n))]+'~'+str(n)).ljust(8)+'TXT'
 cases['cakisma_'+str(count)]='    değişken mevcut: Dizi<metin> = ['+', '.join('"'+x+'"' for x in candidates)+f'];\n    ver ad_esit("Uzundokuman.txt", mevcut, "{exp}");'
cases['dogrudan_cakisma']='''    değişken mevcut: Dizi<metin> = ["FOO     TXT"];
    ver ad_esit("FOO.TXT", mevcut, "FOO~1   TXT");'''
for i,ad in enumerate(['','...',' . . ']):cases['bos_'+str(i)]=f'''    değişken bos: Dizi<metin> = [];
    eşleş kisa_ad_uret("{ad}", bos) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "bos ad"); }} }}'''
cases['yasaklar']='''    değişken bos: Dizi<metin> = [];
    değişken ad: metin = metin_birlestir(kod_metin(34), metin_birlestir(kod_metin(92), "a"));
    ver ad_esit(ad, bos, "__A~1      ") ve lfn_gerekli_mi(ad);'''
testler('tests/drivers/fs/fat32_kisa_ad_test.kem',header,cases)
print(len(cases),'test')
