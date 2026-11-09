from pathlib import Path
import importlib.util,struct
spec=importlib.util.spec_from_file_location('fat','tests/qemu/fat_yazici.py');fat=importlib.util.module_from_spec(spec);spec.loader.exec_module(fat)
fat.hazirla('build/codex8/d-test.img');b=Path('build/codex8/d-test.img').read_bytes()
fs=struct.unpack_from('<I',b,36)[0];ilk=32+2*fs
sektorler=[0,32]+list(range(ilk,ilk+7))
out=['// Bağımsız Python FAT32 yazıcısından sektörler; üretici tools/codex/dalga8_fikstur.py.', 'kullan drivers::ortak::bayt::{ bayt_sifir, yaz_u8 };','genel işlev sektor(lba: tam64) -> Dizi<dtam8> {','    değişken b: Dizi<dtam8> = bayt_sifir(512);']
for lba in sektorler:
 out.append(f'    eğer lba == {lba} {{')
 for i,v in enumerate(b[lba*512:(lba+1)*512]):
  if v:out.append(f'        yaz_u8(b,{i},{v});')
 out.append('    }')
out+=['    ver b;','}']
Path('tests/drivers/fs/sektor_fikstur.kem').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('Sektörler:',sektorler,'merhaba UTF8:',list(fat.MERHABA),'dosya kümeleri:',(1300+511)//512)
