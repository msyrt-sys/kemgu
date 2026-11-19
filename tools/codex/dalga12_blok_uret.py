"""Kapalı blok çeşidinin tekrarlı yönlendirme kollarını üretir; dinamik dispatch yok."""
from pathlib import Path
kinds=[('Nvme','nvme','drivers::nvme::cekirdek','NvmeDurum'),('Ahci','ahci','drivers::ahci::cekirdek','AhciDurum'),('Usb','usb_depo','drivers::usb::depo','UsbDepo')]
s='// Kapalı blok aygıtı kümesi; MMIO/DMA yürütücüsü çağırana aittir.\n'
for kind,pre,mod,typ in kinds:s+=f'kullan {mod}::{{ {typ}, {pre}_adim, {pre}_oku, {pre}_yaz, {pre}_flush }} olarak {pre}_mod;\n'
s+='kullan drivers::ortak::gc::{ GcIstek, GcYanit };\n'
s+='genel çeşit BlokCesidi { '+', '.join(f'{k}({t})' for k,p,m,t in kinds)+' }\ngenel yapı BlokAygiti { durum: BlokCesidi; }\n'
for fn,params,ret,suffix,arg in [('blok_adim','y: GcYanit','GcIstek','adim','y'),('blok_oku','lba: tam64, adet: tam32','sonuç<mantıksal,metin>','oku','lba,adet'),('blok_yaz','lba: tam64, veri: Dizi<dtam8>','sonuç<mantıksal,metin>','yaz','lba,veri'),('blok_bosalt','','sonuç<mantıksal,metin>','flush','')]:
 s+=f'genel işlev {fn}(s: &değişken BlokAygiti'+(', '+params if params else '')+f') -> {ret} {{\n eşleş s.durum {{\n'
 for k,p,m,t in kinds:
  s+=f'  BlokCesidi::{k}(v) => {{ değişken d: {t} = v; değişken r: {ret} = {p}_{suffix}(&değişken d'+(','+arg if arg else '')+f'); s.durum=BlokCesidi::{k}(d); ver r; }}\n'
 s+=' }\n}\n'
for name,field,ret in [('sektor','blok_bayt','tam32'),('kapasite','kapasite','tam64'),('sonuc','sonuc','Dizi<dtam8>')]:
 s+=f'genel işlev blok_{name}(s: BlokAygiti) -> {ret} {{ eşleş s.durum {{\n'
 for k,p,m,t in kinds:s+=f' BlokCesidi::{k}(v) => {{ ver v.{field}; }}\n'
 s+='} }\n'
Path('drivers/blok/aygit.kem').write_text(s,encoding='utf-8')
