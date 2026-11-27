#!/usr/bin/env python3
"""Make tariflerini açar ve bölünmüş şemsiye hedeflerin kâhin çağrılarını sayar."""
import argparse
import os
import shlex
import subprocess
import sys
from collections import Counter
from pathlib import Path

from make_tarifleri import make_programi

KOK = Path(__file__).resolve().parents[2]
TABAN = KOK / 'test/olcum/kapsam_taban.tsv'
KAPILAR = ('bm_x86_ag_saglamlik', 'bm_x86_cok_surec', 'bm_x86_kabuk14',
           'bm_x86_kaynak', 'bm_x86_ps2_irq', 'bm_x86_ps2_karma',
           'bm_x86_tcp_verim', 'bm_x86_v1', 'kem_os_pc_matris')
sys.path.insert(0, str(KOK / 'tests/qemu'))
from pc_matris import durumlar


def olaylar(program, ortam, arg):
    def derleyiciler(ad):
        for d in ortam.get(ad, 'build/kemgu build/codegen').split():
            yield Path(d).stem

    if program == 'bm_x86_nic_harness.sh':
        kip, = arg
        for d in derleyiciler('NIC_DERLEYICILER'):
            if kip == 'ag_pc':
                yield d, 'ag_pc_matris.py'
            elif kip == 'verim':
                for nokta in ortam.get('NIC_VERIM_KIPI', 'olcekli olceksiz').split():
                    yield d, 'ag_tcp_secenek.py', nokta
            elif kip == 'saglamlik_ag':
                for senaryo in ortam.get('NIC_SAGLAMLIK', 'cerceve aktarim zaman').split():
                    yield d, 'ag_saglamlik_tel.py', senaryo
            else:
                raise ValueError(f'Bilinmeyen NIC kâhini {kip}')
    elif program == 'bm_x86_surec_harness.sh':
        for d in derleyiciler('SUREC_DERLEYICILER'):
            if ortam.get('SUREC_KAHIN') == 'pc_matris':
                for durum in durumlar():
                    if ortam.get('PC_MATRIS_TABLO', durum[-1]) == durum[-1]:
                        for tur in range(1, 4):
                            yield d, 'pc_matris.py', *durum, str(tur)
            else:
                ilk = int(ortam.get('SUREC_ILK', 1))
                for senaryo in ortam['SUREC_SENARYO'].split():
                    for tur in range(ilk, ilk + int(ortam.get('SUREC_TEKRAR', 10))):
                        yield d, 'surec_kahin.py', senaryo, str(tur)
    elif program == 'bm_x86_uefi_harness.sh':
        d = Path(ortam.get('UEFI_DERLEYICI', 'build/kemgu')).stem
        pc = ortam.get('UEFI_PC_URET') == '1'
        kipler = [k for k in ('UEFI_PS2_IRQ', 'UEFI_PS2_KARMA') if ortam.get(k) == '1']
        if len(kipler) != 1:
            raise ValueError(f'UEFI giriş kipleri {kipler}')
        kip = kipler[0]
        birim = (d, 'bm_uefi.py', kip, ortam.get('UEFI_KAYNAK', 'varsayılan'),
                 'kabuk=' + ortam.get('UEFI_KABUK', '0'))
        if not pc:
            for hata in ('ELF ac', 'ELF baslik', 'ELF giris', 'ELF LOAD'):
                yield (*birim, hata)
        for aygit in ortam.get('UEFI_PC_TURLER', 'nvme').split():
            for ram in ortam['UEFI_RAM'].split():
                for tur in range(1, 1 + int(ortam['UEFI_TEKRAR'])):
                    yield (*birim, aygit, ram, str(tur))
    else:
        raise ValueError(f'Bilinmeyen harness {program}')


def liste(makefile):
    sayim = Counter()
    for ad in KAPILAR:
        hedef = 'calistir_' + ad
        tarif = subprocess.check_output(
            [make_programi(), '--no-print-directory', '-n', '-f', makefile, hedef],
            cwd=KOK, env={**os.environ, 'MAKEFLAGS':'', 'MFLAGS':''},
            encoding='utf-8', errors='replace')
        for satir in tarif.splitlines():
            if 'bash test/bm_x86_' not in satir:
                continue
            args = shlex.split(satir)
            i = args.index('bash')
            ortam = dict(parca.split('=', 1) for parca in args[:i])
            for olay in olaylar(Path(args[i+1]).name, ortam, args[i+2:]):
                sayim['\t'.join((hedef, *olay))] += 1
    return sayim


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--makefile', default='Makefile')
    p.add_argument('--liste', action='store_true')
    a = p.parse_args()
    gercek = liste(a.makefile)
    if a.liste:
        for kayit in sorted(gercek.elements()):
            print(kayit)
        return 0
    once = Counter(TABAN.read_text(encoding='utf-8').splitlines())
    for tur, fark in [('Eksik', once-gercek), ('Fazla', gercek-once)]:
        for kayit, adet in sorted(fark.items()):
            print(f'{tur} ({adet}): {kayit}', file=sys.stderr)
    for ad in KAPILAR:
        sayi = sum(n for k, n in gercek.items() if k.startswith('calistir_' + ad + '\t'))
        print(f'{ad}: {sayi} kâhin çağrısı')
    return int(gercek != once)


if __name__ == '__main__':
    raise SystemExit(main())
