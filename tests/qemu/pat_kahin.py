"""CPU'nun etkin CR3'ünden QMP ile yürür, küçük ve büyük yaprağın PAT indeksini çözer."""
import re
import struct


def denetle(dump, w, cr3, text, fields, pci):
    tablolar = {}
    maske = (1 << 52) - 1

    def yaprak(adres):
        pt = cr3 & maske & ~4095
        for kayma in (39, 30, 21, 12):
            if pt not in tablolar:
                tablolar[pt] = struct.unpack('<512Q', dump(pt, 4096, w/'pat-tablo.bin'))
            g = tablolar[pt][adres >> kayma & 511]
            assert g & 1, (hex(adres), kayma)
            if kayma == 12 or (kayma in (30, 21) and g & 128):
                taban = g & maske & ~((1 << kayma) - 1)
                assert taban == adres & ~((1 << kayma) - 1), (hex(adres), hex(g))
                return g, kayma
            pt = g & maske & ~4095
        raise AssertionError(adres)

    once, pat, tablo = map(int, re.search(r'^PAT_MSR (\d+) (\d+) (\d+)$', text, re.M).groups())
    assert tablo == cr3
    assert pat & ((1 << 56)-1) == once & ((1 << 56)-1)
    fb, boy = fields[3:5]
    bas, son = fb & ~4095, (fb+boy+4095) & ~4095

    def bellek_tipi(adres, beklenen):
        g, kayma = yaprak(adres)
        bit = 7 if kayma == 12 else 12
        indeks = (g >> bit & 1) * 4 + (g >> 4 & 1) * 2 + (g >> 3 & 1)
        tip = (pat >> (8 * indeks)) & 255
        assert tip == beklenen, (hex(adres), hex(g), indeks, {0:'UC',1:'WC',6:'WB'}.get(tip,tip))
        if beklenen == 1:
            assert indeks == 7 and kayma == 12, (indeks, kayma)
        return g, kayma

    for ad, adres, tip in [('ILK',fb,1), ('SON',fb+boy-1,1), ('RAM',fields[14],6), ('MMIO',0xfee00000,0)]:
        a, g = map(int,re.search(rf'^PAT_{ad} (\d+) (\d+) 0$', text, re.M).groups())
        gercek, _ = bellek_tipi(adres, tip)
        assert a == adres and gercek & ~96 == g & ~96
    for a in range(bas, son, 4096):
        bellek_tipi(a, 1)
    for b in pci:
        for aygit in b['devices']:
            for r in aygit.get('regions', []):
                a, n = r.get('address', 0), r.get('size', 0)
                if r['type'] != 'memory' or a <= 0:
                    continue
                bitis = a+n
                while a < bitis:
                    _, kayma = bellek_tipi(a, 1 if bas <= a < son else 0)
                    a = ((a >> kayma) + 1) << kayma
    for a in map(int,re.findall(r'^MMIO (\d+)$',text,re.M)):
        bellek_tipi(a, 1 if bas <= a < son else 0)
    zaman = re.search(r'^PAT_TSC (\d+) (\d+) (\d+)$', text, re.M)
    if zaman:
        uc, wc, piksel = map(int,zaman.groups())
        assert uc > 0 and wc > 0 and piksel == fields[6]*fields[7]//4
        print(f'PAT_TSC UC={uc} WC={wc} piksel={piksel}',flush=True)
    print(f'PAT OK önce={once:#018x} sonra={pat:#018x} WC_sayfa={(son-bas)//4096}',flush=True)
