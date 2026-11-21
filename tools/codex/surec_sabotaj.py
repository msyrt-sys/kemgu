#!/usr/bin/env python3
"""Dalga 13 kapılarını ayrı kaynak kopyalarında derlenen kusurlarla doğrular."""
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = 'runtime/mimari/x86_64/kem_surec_hw.kem'
VFS = 'test/ornekler/kem_os_x86_vfs_ortak.kem'
KABUK = 'test/ornekler/kem_os_x86_kabuk.kem'
CASES = [
    ('argv', 'baslat', CORE, 'kxm_y64(bas,adet);', 'kxm_y64(bas,0);', 'merhaba.elf pid=1 cikis=0'),
    ('bss', 'tekrar', CORE, 'memset(taban olarak *tam8, 0, 2113536);',
     'memset(taban olarak *tam8, 0, 16384);\n    memset((taban+1064960) olarak *tam8, 0, 1048576);',
     'cikis=124'),
    ('fd', 'yazma', VFS, 'fd >= (KSV_FD_AZAMI olarak tam64)',
     'fd > (KSV_FD_AZAMI olarak tam64)', 'dizi sinir ihlali'),
    ('salt-okunur', 'yazma', KABUK, 'ver yazma_izni(k.kayit,k.yazilabilir);',
     'ver tamam(doğru);', 'salt okunur disk SHA256 değişti'),
    ('cerceve', 'tekrar', CORE, 'kxs_havuz_dolu=kxs_havuz_dolu & (3 ^ (1 << i));',
     'kxs_havuz_dolu=kxs_havuz_dolu;', 'AssertionError: süreç 2'),
    ('izin', 'tus-usb', CORE, 'eğer s.bayraklar == 6 { izin = izin | 2; }',
     'eğer s.bayraklar == 6 veya s.bayraklar == 5 { izin = izin | 2; }', '0x54000000'),
    ('konum', 'yazma', VFS, 'a.konum=bas+ofset;', 'a.konum=bas+ofset+1;', 'cikis=12'),
    ('esitle', 'yazma', VFS, 'eğer b.degisti { ver ksv_esitle(b); }', '', 'Dirty bit is set.'),
]


def snapshot(work):
    # QMP Unix soketinin 108 baytlık yol sınırı için kısa bağımsız kök.
    temp = Path(tempfile.mkdtemp(prefix='k13-'))
    paths = subprocess.check_output(['git', 'ls-files', '-co', '--exclude-standard', '-z'], cwd=ROOT).decode().split('\0')
    for name in set(paths):
        if name and name.startswith(('boot/', 'drivers/', 'runtime/', 'linker/', 'test/', 'tests/qemu/', 'belgeler/')):
            source = ROOT / name
            if source.is_file():
                target = temp / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
    (temp / 'build').mkdir()
    for name in ('kemgu', 'codegen', 'bm_x86_start.o', 'bm_x86_kesme.o', 'bm_x86_gorev.o',
                 'bm_x86_uart.o', 'bm_x86_yazdir.o', 'bm_x86_panik.o'):
        (temp / 'build' / name).symlink_to(ROOT / 'build' / name)
    return temp


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sec', nargs='*')
    args = p.parse_args()
    work = Path(tempfile.mkdtemp(prefix='surec-sabotaj-', dir=ROOT / 'build'))
    print(work, flush=True)
    for name, scenario, source, before, after, evidence in CASES:
        if args.sec and name not in args.sec:
            continue
        temp = snapshot(work)
        path = temp / source
        text = path.read_text(encoding='utf-8')
        assert text.count(before) == 1, (name, text.count(before))
        path.write_text(text.replace(before, after), encoding='utf-8')
        run = subprocess.run(['bash', 'test/bm_x86_surec_harness.sh'], cwd=temp,
                             env={**os.environ, 'SUREC_DERLEYICILER': 'build/kemgu',
                                  'SUREC_TEKRAR': '1', 'SUREC_SENARYO': scenario},
                             capture_output=True, text=True, timeout=600)
        log = run.stdout + run.stderr
        (work / f'{name}.log').write_text(log, encoding='utf-8')
        assert run.returncode != 0 and 'AssertionError' in log and evidence in log, (name, run.returncode, log[-4000:])
        print(f'QEMU KIRMIZI 1/1 {name} ({scenario})', flush=True)
        shutil.rmtree(temp)
    if not args.sec or 'abi' in args.sec:
        temp = snapshot(work)
        path = temp / 'belgeler/KEMGU_OS_Syscall_ABI.md'
        path.write_text(path.read_text(encoding='utf-8').replace('| 55 | tus_bekle |', '| 58 | tus_bekle |'), encoding='utf-8')
        run = subprocess.run(['python3', 'tests/qemu/surec_abi.py'], cwd=temp, capture_output=True, text=True)
        (work / 'abi.log').write_text(run.stdout + run.stderr, encoding='utf-8')
        assert run.returncode != 0 and 'ABI 55:' in run.stdout and 'ABI 58:' in run.stdout
        print('ABI KIRMIZI 1/1 belge numarası (QEMU öncesi kapı)', flush=True)
        shutil.rmtree(temp)
    if not args.sec or 'elf' in args.sec:
        temp = snapshot(work)
        elf = temp / 'build/merhaba.elf'
        shadow = elf.with_name('yasak.elf')
        subprocess.run(['python3', 'tests/qemu/surec_elf.py', elf, '--kaynak', 'test/kullanici/merhaba.kem'], cwd=temp, check=True)
        subprocess.run(['llvm-objcopy', '--add-symbol=kdl_syscall_isle=.text:0,global', elf, shadow], check=True)
        run = subprocess.run(['python3', 'tests/qemu/surec_elf.py', shadow], cwd=temp, capture_output=True, text=True)
        (work / 'elf.log').write_text(run.stdout + run.stderr, encoding='utf-8')
        assert run.returncode != 0 and 'kdl_syscall_isle' in run.stderr
        print('ELF KIRMIZI 1/1 çekirdek sembolü (QEMU öncesi kapı)', flush=True)
        shutil.rmtree(temp)


if __name__ == '__main__':
    main()
