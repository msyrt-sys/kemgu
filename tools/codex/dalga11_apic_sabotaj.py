#!/usr/bin/env python3
"""ISO bilgisini boz; saf test ve gerçek QEMU kapısında kırmızıyı ölç."""
from pathlib import Path
import subprocess,os,json
p=Path('drivers/acpi/yonlendirme.kem');original=p.read_bytes()
a=b'y.gsi=g.gsi;';b=b'y.gsi=irq olarak tam64;';assert original.count(a)==1
w=Path('build/dalga11_apic_sabotaj');w.mkdir(exist_ok=True)
try:
 p.write_bytes(original.replace(a,b))
 r=subprocess.run(['bash','otomasyon/chatgpt/dogrula.sh','tests/drivers/acpi/yonlendirme_test.kem'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (w/'saf.log').write_bytes(r.stdout);assert r.returncode!=0 and b'APIC YOL OK' not in r.stdout
 for i in range(10):
  # Her açılıştan önce taze ikili; derleme başarısızlığı sabotaj sayılmaz.
  subprocess.run(['bash','test/bm_x86_uefi_harness.sh'],env=os.environ|{'UEFI_YALNIZ_DERLE':'1'},check=True,stdout=subprocess.DEVNULL)
  code="import sys;sys.path.insert(0,'tests/qemu');sys.argv=['bm_uefi.py','build/_bm_uefi'];import bm_uefi;bm_uefi.boot()"
  r=subprocess.run(['python3','-c',code],env=os.environ|{'UEFI_APIC':'1'},stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=70)
  (w/f'iso-{i+1}.log').write_bytes(r.stdout)
  assert r.returncode==1 and b'AssertionError' in r.stdout and b'APIC PIT ISO zaman asimi' in r.stdout,(i,r.returncode,r.stdout)
  print(f'ISO sabotaj {i+1}/10: KIRMIZI',flush=True)
 (w/'sonuc.json').write_text(json.dumps({'sabotaj':'IRQ0 ISO yok','acilis':10,'kirmizi':10}))
finally:p.write_bytes(original)
