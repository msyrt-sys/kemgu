#!/usr/bin/env python3
"""IRQ12 maskesi ve gerçek AUX kayık baytı: koşum kırmızı, sonra kaynak geri alınır."""
from pathlib import Path
import os,subprocess,json
w=Path('build/dalga11_irq_sabotaj');w.mkdir(exist_ok=True)
cases=[('irq12',Path('runtime/mimari/x86_64/kem_apic_hw.kem'),b'kap_io_oku(kap_io12,kap_reg12)&4294901759',b'kap_io_oku(kap_io12,kap_reg12)|65536'),
('senkron',Path('drivers/ps2/fare.kem'),'eğer (b & 8) == 0 { ver hiç; }'.encode(),b'// Sabotaj: ilk bayt senkron biti yok sayildi.')]
results=[]
for name,path,old,new in cases:
 original=path.read_bytes();assert original.count(old)==1
 try:
  path.write_bytes(original.replace(old,new))
  if name=='senkron':
   r=subprocess.run(['bash','otomasyon/chatgpt/dogrula.sh','tests/drivers/ps2/fare_test.kem'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
   (w/'saf.log').write_bytes(r.stdout);assert r.returncode!=0
  red=0
  for i in range(10):
   env=os.environ|{'UEFI_YALNIZ_DERLE':'1','UEFI_KAYNAK':'test/ornekler/kem_os_x86_kabuk.kem'}
   subprocess.run(['bash','test/bm_x86_uefi_harness.sh'],env=env,check=True,stdout=subprocess.DEVNULL)
   code="import sys;sys.path.insert(0,'tests/qemu');sys.argv=['bm_uefi.py','build/_bm_uefi'];import bm_uefi;bm_uefi.boot(n=2)"
   r=subprocess.run(['python3','-c',code],env=os.environ|{'UEFI_PS2_IRQ':'1','UEFI_KABUK':'1'},stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=80)
   (w/f'{name}-{i+1}.log').write_bytes(r.stdout)
   assert r.returncode==1 and b'AssertionError' in r.stdout and b'FARE dx=' in r.stdout,(name,i,r.returncode,r.stdout)
   red+=1;print(f'{name} sabotaj {i+1}/10 KIRMIZI',flush=True)
  results.append({'sabotaj':name,'acilis':10,'kirmizi':red})
 finally:path.write_bytes(original)
(w/'sonuc.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
