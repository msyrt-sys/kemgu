#!/usr/bin/env python3
"""USB klavye bekleyişi fare halkasına yol vermezse gerçek IRQ12 kapısı kırmızı."""
from pathlib import Path
import os,subprocess,json
p=Path('test/ornekler/kem_os_x86_kabuk.kem');original=p.read_bytes()
old='eğer ps2_kesme_fare_var() { ver yanlış; }'.encode();assert original.count(old)==1
w=Path('build/dalga11_karma_sabotaj');w.mkdir(exist_ok=True)
try:
 p.write_bytes(original.replace(old,b'// Sabotaj: USB beklerken fare halkasina yol verme.'))
 for i in range(10):
  subprocess.run(['bash','test/bm_x86_uefi_harness.sh'],env=os.environ|{'UEFI_YALNIZ_DERLE':'1','UEFI_PC_URET':'1','UEFI_KAYNAK':'test/ornekler/kem_os_x86_kabuk.kem'},check=True,stdout=subprocess.DEVNULL)
  code="import sys;sys.path.insert(0,'tests/qemu');sys.argv=['bm_uefi.py','build/_bm_uefi'];import bm_uefi;bm_uefi.boot(n=2,mode='usb')"
  r=subprocess.run(['python3','-c',code],env=os.environ|{'UEFI_PS2_KARMA':'1','UEFI_KABUK':'1','UEFI_PC_IMG':'build/kem_os_pc.img'},stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=80)
  (w/f'{i+1}.log').write_bytes(r.stdout)
  assert r.returncode==1 and b'AssertionError' in r.stdout and b'FARE dx=' in r.stdout,(i,r.returncode,r.stdout)
  print(f'USB bekleyişi sabotaj {i+1}/10 KIRMIZI',flush=True)
 (w/'sonuc.json').write_text(json.dumps({'acilis':10,'kirmizi':10}))
finally:p.write_bytes(original)
