"""QMP tuşları, i8042 izi ve USBmon PCAP üzerinden klavyenin iki yolunu ölçer."""
import argparse
import re
import struct
from pathlib import Path

from dalga12_kahin import boot, wait_for
from duzenle_kahin import tus, yaz
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def satir(c, metin):
    beklenen = (metin + '\n').encode()
    wait_for(c.p, lambda: beklenen in c.ser.read_bytes())


def pcap_led(dosya):
    veri = dosya.read_bytes()
    assert struct.unpack_from('<I', veri, 20)[0] == 220
    i, ledler = 24, []
    while i < len(veri):
        n = struct.unpack_from('<I', veri, i + 8)[0]
        assert 64 <= n <= len(veri) - i - 16
        paket = veri[i + 16:i + 16 + n]
        if paket[8:11] == b'S\x02\x00' and paket[40:48] == bytes.fromhex('21 09 00 02 00 00 01 00'):
            assert n == 65
            ledler.append(paket[64])
        i += 16 + n
    return ledler


def duzen(c):
    c.run('duzen us')
    yaz(c, 'echo ')
    tus(c, 'i')
    tus(c, '2', shift=True)
    tus(c, 'ret')
    satir(c, 'i@')
    tus(c, 'spc', ctrl=True)
    # Her fiziksel tuş için üçüncü katmandaki yazılabilir Türkçe Q karşılığı.
    esleme = [('q', '@'), ('e', '€'), ('7', '{'), ('0', '}'), ('8', '['), ('9', ']'),
              ('minus', chr(92)), ('equal', '|'), ('bracket_right', '~'), ('1', '>'),
              ('2', '£'), ('3', '#'), ('4', '$'), ('5', '½'), ('6', '¾'), ('r', '¶'),
              ('t', '₺'), ('y', '←'), ('u', 'û'), ('i', 'î'), ('o', 'ô'), ('a', 'â'),
              ('s', 'ß'), ('f', 'ª'), ('semicolon', '´'), ('apostrophe', "'"),
              ('grave_accent', '<'), ('backslash', chr(96)), ('z', '«'), ('x', '»'),
              ('c', '¢'), ('v', '„'), ('b', '“'), ('n', '”'), ('m', 'µ'),
              ('comma', '×'), ('dot', '·'), ('less', '|')]
    yaz(c, 'tusum ')
    c.key('alt_r', True)
    for ad, _ in esleme:
        tus(c, ad)
    c.key('alt_r', False)
    tus(c, 'ret')
    # Boru ve yönlendirme karakterleri de içeren gerçek satırı, ayrıştırılmadan
    # önce konsola aktarılan UTF-8 tamponundan QMP ile oku.
    beklenen = ('> tusum ' + ''.join(k for _, k in esleme) + chr(10)).encode()
    adres = c.sayi('EKRAN_AYNA_TABAN')
    wait_for(c.p, lambda: c.mem(adres, len(beklenen)) == beklenen)



def kilit(c, tur):
    for ad in ('caps_lock', 'num_lock', 'scroll_lock', 'caps_lock', 'scroll_lock'):
        tus(c, ad)
    yaz(c, 'echo ')
    tus(c, 'kp_7')
    tus(c, 'ret')
    satir(c, '7')
    tus(c, 'num_lock')
    beklenen = [4, 6, 7, 3, 2, 0] if tur == 'ps2' else [2, 3, 7, 5, 1, 0]
    etiket = b'PS2_LED 237 ' if tur == 'ps2' else b'USB_LED 9 '
    wait_for(c.p, lambda: len(re.findall(etiket + rb'(\d+) ', c.ser.read_bytes())) >= 6)
    assert list(map(int, re.findall(etiket + rb'(\d+) ', c.ser.read_bytes())))[-6:] == beklenen
    if tur == 'usb':
        assert pcap_led(c.w / 'klavye.pcap')[-6:] == beklenen
    else:
        iz = (c.w / 'dalga12-qemu.log').read_bytes()
        assert list(map(int, re.findall(rb' ledstate (\d+)', iz)))[-6:] == beklenen
        komut = list(map(int, re.findall(rb'ps2_write_keyboard .*?val (\d+)', iz)))
        assert (243, 127) in zip(komut, komut[1:])
    print('LED', tur, beklenen, flush=True)


def ozel(c):
    pid = c.sayi('kxs_pid_son') + 1
    c.input('calistir /diskler/usb0p2/bin/tus.elf')
    satir(c, 'TUS_HAZIR')
    for ad in ('pause', 'print', 'kp_8'):
        tus(c, ad)
    satir(c, f'[tus.elf pid={pid} cikis=0]')
    kodlar = re.findall(rb'^TUS (\d+) 0 ([01]) 0$', c.ser.read_bytes(), re.M)
    assert [(int(k), int(b)) for k, b in kodlar] == [(581, 1), (311, 1), (311, 0), (328, 1), (328, 0)], kodlar


def kes(c):
    arka = c.sayi('kxs_pid_son') + 1
    on = arka + 1
    c.input('baslat /diskler/usb0p2/bin/hesap.elf uzun &')
    satir(c, f'pid={arka}')
    c.input('calistir /diskler/usb0p2/bin/hesap.elf uzun')
    wait_for(c.p, lambda: c.sayi('kxs_on_pid') == on)
    tus(c, 'c', ctrl=True)
    satir(c, '^C')
    satir(c, f'[hesap.elf pid={on} SONLANDIRILDI: sonlandir (kullanici)]')
    c.input('ps')
    wait_for(c.p, lambda: re.search(rf'^{arka} hazir hesap[.]elf [0-9]+$'.encode(), c.ser.read_bytes(), re.M))
    c.input('echo kabuk')
    satir(c, 'kabuk')


def tasma(c):
    c.input('girdisay 12')
    satir(c, 'PS2_SAY BASLA')
    adet = 1600
    for i in range(0, adet, 8):
        # Küçük paketler aygıt FIFO'sunun IRQ işleyicisi tarafından boşaltılmasına izin verir.
        olaylar = [dict(type='key', data=dict(down=True, key=dict(type='qcode', data='a')))
                   for _ in range(8)]
        once = c.sayi('ps2_irq1_sayi')
        c.q.call('input-send-event', {'events': olaylar})
        wait_for(c.p, lambda: c.sayi('ps2_irq1_sayi') >= once + len(olaylar))
    m = wait_for(c.p, lambda: re.search(rb'^PS2_SAY (\d+) (\d+) (\d+)$', c.ser.read_bytes(), re.M), seconds=30)
    bas, birak, kayip = map(int, m.groups())
    assert birak == 0 and kayip > 0 and adet - bas == kayip, (adet, bas, birak, kayip)
    c.key('a', False)
    satir(c, f'PS2_KAYIP {kayip} 0 0')
    c.input('echo halkadan sonra')
    satir(c, 'halkadan sonra')
    print('PS2 halka', adet, bas, birak, kayip, flush=True)


def denetleyicisiz(w):
    programlar = {p.name: p.read_bytes() for p in (w / 'kullanici').glob('*.elf')}
    usb, nvme, sata = imajlar(w, programlar)
    with boot(w, usb[0], nvme, sata, klavye='usb', i8042=False) as (q, p, seri):
        c = SurecConsole(w, q, p, seri)
        assert b'8042 yok' in seri.read_bytes()
        c.run('echo usb', b'usb\n')
    print('KLAVYE16 OK 8042 yok, USB ile giriş', flush=True)


def kos(w, tur, bolum=None):
    dosyalar = {p.name: p.read_bytes() for p in (w / 'kullanici').glob('*.elf')}
    usb, nvme, sata = imajlar(w, dosyalar)
    with boot(w, usb[0], nvme, sata, klavye=tur, klavye_izi=True) as (q, p, seri):
        c = SurecConsole(w, q, p, seri)
        c.input('donanim')
        satir(c, '8042 oztest port1 port2 85 0 0')
        # Donanım raporunun uzunluğu makinedeki PCI listesine bağlı.
        wait_for(p, lambda: c.output().endswith(b'KABUK_HAZIR\n'))
        c.expected = c.output()
        for ad, denetim in [('duzen', duzen), ('kilit', lambda c: kilit(c, tur)), ('ozel', ozel), ('kes', kes), ('tasma', tasma)]:
            if (bolum is None or ad == bolum) and (ad != 'tasma' or tur == 'ps2'):
                denetim(c)
    print('KLAVYE16 OK', tur, flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dizin', type=Path)
    parser.add_argument('--aygit', choices=('ps2', 'usb'))
    parser.add_argument('--senaryo', choices=('duzen', 'kilit', 'ozel', 'kes', 'tasma'))
    args = parser.parse_args()
    for aygit in ([args.aygit] if args.aygit else ['ps2', 'usb']):
        kos(args.dizin.resolve(), aygit, args.senaryo)
    if not args.aygit and not args.senaryo:
        denetleyicisiz(args.dizin.resolve())
