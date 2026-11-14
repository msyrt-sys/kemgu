#!/usr/bin/env python3
"""FADT reset yazısını kaldır: fallback gerçek QEMU'yu kapatsa da doğru yol kapısı kırmızı."""
from pathlib import Path
import os,subprocess
f=Path('drivers/acpi/fadt.kem');saved=f.read_bytes();old='ps2_var:surum>=3 ve (boot&2)!=0'.encode();assert saved.count(old)==1
try:
 f.write_bytes(saved.replace(old,'ps2_var:surum>=3 ve (boot&2)==0'.encode()))
 r=subprocess.run(['bash','otomasyon/chatgpt/dogrula.sh','tests/drivers/acpi/fadt_guc_test.kem'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 Path('build/dalga11_fadt_sabotaj.log').write_bytes(r.stdout)
 assert r.returncode!=0 and 'HATA[C çalıştır]'.encode() in r.stdout,(r.returncode,r.stdout)
 print('FADT 8042 biti ters: saf test KIRMIZI',flush=True)
finally:f.write_bytes(saved)
p=Path('runtime/mimari/x86_64/kem_guc_hw.kem');original=p.read_bytes();old=b'guc_fadt_yaz();';assert original.count(old)==1
try:
 p.write_bytes(original.replace(old,b'kem_bekle_us(1);'))
 subprocess.run(['bash','test/bm_x86_uefi_harness.sh'],env=os.environ|{'UEFI_YALNIZ_DERLE':'1','UEFI_KAYNAK':'test/ornekler/kem_os_x86_kabuk.kem'},check=True)
 code="import sys;sys.path.insert(0,'tests/qemu');sys.argv=['bm_uefi.py','build/_bm_uefi'];import bm_uefi;bm_uefi.boot()"
 r=subprocess.run(['python3','-c',code],env=os.environ|{'UEFI_GUC':'yenidenbaslat','UEFI_KABUK':'1','UEFI_PS2_IRQ':'1'},stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
 Path('build/dalga11_guc_sabotaj.log').write_bytes(r.stdout)
 assert r.returncode==1 and b'AssertionError' in r.stdout and b'GUC YENIDENBASLAT 8042' in r.stdout,(r.returncode,r.stdout)
 print('FADT reset yazısı yok: 1/1 KIRMIZI (8042 fallback ölçüldü)')
finally:p.write_bytes(original)
