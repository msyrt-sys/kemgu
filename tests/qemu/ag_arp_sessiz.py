"""Tek konuğun ARP yanıtını sessize alan geçici test çekirdeği kaynağını oluşturur."""
from pathlib import Path
import sys

s=Path('test/ornekler/kem_os_x86_kabuk.kem').read_text(encoding='utf-8')
im='değişken sonuc: tam32 = l3_al(&değişken s,rx,n,tx,ms,lo);'
assert s.count(im)==1
ek='\n        eğer sonuc==42 ve ag_be16(tx,12)==2054 ve ag_be16(tx,20)==2 { sonuc=0; }'
Path(sys.argv[1]).write_text(s.replace(im,im+ek),encoding='utf-8')
