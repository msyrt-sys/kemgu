from yardim import testler
import uuid
ids=[('C12A7328-F81F-11D2-BA4B-00A0C93EC93B','EFI System'),('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7','Microsoft basic data'),('0FC63DAF-8483-4772-8E79-3D69D8477DE4','Linux filesystem'),('0657FD6D-A4AB-43C4-84E5-0933C84B4F4F','Linux swap'),('00112233-4455-6677-8899-AABBCCDDEEFF','Bilinmeyen')]
header='''// Beklentiler: python3 tools/codex/b9.py; uuid.UUID(...).bytes_le.
kullan drivers::disk::guid::{ guid_bicimle, guid_coz, guid_tur_adi };
kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8, oku_u8 };
'''
cases={}
for i,(g,name) in enumerate(ids):
 raw=uuid.UUID(g).bytes_le
 s='    değişken b: Dizi<dtam8> = bayt_sifir(16);\n'
 for j,x in enumerate(raw):s+=f'    yaz_u8(b, {j}, {x});\n'
 s+=f'''    eğer değil metin_esit(guid_tur_adi(b), "{name}") {{ ver yanlış; }}
    eşleş guid_bicimle(b) {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{ eğer değil metin_esit(v, "{g}") {{ ver yanlış; }} }} }}
    eşleş guid_coz("{g.lower()}") {{ hata(e) => {{ ver yanlış; }} tamam(v) => {{
        değişken a: Dizi<dtam8> = v; değişken j: tam32 = 0;
        iken j < 16 {{ eğer oku_u8(a, j) != oku_u8(b, j) {{ ver yanlış; }} j = j + 1; }}
    }} }}
    ver doğru;'''
 cases['guid_'+str(i)]=s
for i,s in enumerate(['','0'*35,'0'*37,'00112233_4455-6677-8899-AABBCCDDEEFF','00112233-4455-6677-8899-AABBCCDDEEFG']):
 cases['gecersiz_'+str(i)]=f'    eşleş guid_coz("{s}") {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver doğru; }} }}'
for n in (0,15,17):cases['boy_'+str(n)]=f'    eşleş guid_bicimle(bayt_sifir({n})) {{ tamam(v) => {{ ver yanlış; }} hata(e) => {{ ver metin_esit(e, "guid boyu"); }} }}'
testler('tests/drivers/disk/guid_test.kem',header,cases)
print(len(cases),'test; EFI baytları',list(uuid.UUID(ids[0][0]).bytes_le))
