"""Yerel tablo sınamasını yalnız geçici çekirdek kaynağına ekler."""
from pathlib import Path
import sys

p=Path('test/ornekler/kem_os_x86_kabuk.kem')
s=p.read_text(encoding='utf-8')
im='işlev ag_komut(arg: metin) -> sonuç<mantıksal,metin> {'
assert s.count(im)==1
fikstur=Path('tests/qemu/ag_tablo_fikstur.kem').read_text(encoding='utf-8')
dal='''
    eğer bm_esit(arg,"tablolar") {
        değişken kod: tam32=ag_tablo_sina();
        ekran_cift_yaz(kd_tamsayi("AG18 tablo 100 tur",kod olarak tam64)); ver tamam(doğru);
    }
'''
Path(sys.argv[1]).write_text(s.replace(im,fikstur+im+dal),encoding='utf-8')
