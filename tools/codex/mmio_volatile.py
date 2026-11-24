"""Birleşik çekirdeğin aygıt beklemelerini gerçek IR ve ELF kontrol akışında izler."""
import argparse
from pathlib import Path
import re
import subprocess

# Bekleme döngüsü, aygıt okumasına giden çağrı yolu, yükleme genişliği.
BEKLEMELER = (
    ('vblk_gonder', ('vblk_o16',), 16),
    ('vnet_gonder', ('vblk_o16',), 16),
    ('vnet_al', ('vblk_o16',), 16),
    ('bd_sata_bagli', ('kem_mmio_oku32',), 32),
    ('bd_zil_bekle', ('kem_mmio_oku64',), 64),
    ('bh_olaylar', ('bh_tuket','kem_mmio_oku32'), 32),
    ('bd_sur', ('bm_yurut','kem_mmio_oku32'), 32),
    ('bd_xsur', ('bm_yurut','kem_mmio_oku32'), 32),
    ('bh_gonder', ('bm_yurut','kem_mmio_oku32'), 32),
)


def dongu_dugumleri(kenarlar):
    # Döngü üyesi ancak bir ardıl üzerinden aynı düğüme geri dönülebiliyorsa seçilir.
    sonuc = set()
    for bas, ardillar in kenarlar.items():
        kuyruk = list(ardillar)
        goruldu = set()
        while kuyruk:
            yer = kuyruk.pop()
            if yer == bas:
                sonuc.add(bas)
                break
            if yer not in goruldu:
                goruldu.add(yer)
                kuyruk.extend(kenarlar.get(yer, ()))
    return sonuc


def ir_islevleri(path, adlar):
    secilen = {}
    baslik = re.compile(r'^define .*?@([\w.$]+)\(')
    ad = None
    with path.open(encoding='utf-8') as f:
        for line in f:
            if line.startswith('define '):
                m = baslik.match(line)
                ad = m[1] if m and m[1] in adlar else None
                if ad:
                    blok = 'entry'
                    secilen[ad] = {blok: []}
            elif line.startswith('}'):
                ad = None
            elif ad:
                m = re.match(r'^([\w.$-]+):',line)
                if m:
                    blok = m[1]
                    secilen[ad].setdefault(blok, [])
                else:
                    secilen[ad][blok].append(line)
    return {ad:{b:''.join(xs) for b,xs in bloklar.items()} for ad,bloklar in secilen.items()}


def asm_islevleri(elf, adlar):
    cmd = ['llvm-objdump','--no-show-raw-insn','--disassemble-symbols='+','.join(sorted(adlar)),str(elf)]
    ham = subprocess.check_output(cmd,text=True,encoding='utf-8')
    sonuc = {}
    for line in ham.splitlines():
        bas = re.match(r'^[0-9a-f]+ <([^>]+)>:', line)
        if bas:
            kod = {}
            sonuc[bas[1]] = kod
            continue
        op = re.match(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)',line)
        if op:
            kod[int(op[1],16)] = op[2]+' '+op[3].split('#')[0]
    return sonuc


def asm_kenarlar(kod):
    # Dallanma hedefleri temel blok sınırıdır. Böylece büyük işlevlerde
    # döngü araması her tekil makine komutu üzerinde yeniden yapılmaz.
    adresler = list(kod)
    baslar = {adresler[0]}
    hedefler = {}
    for i, a in enumerate(adresler):
        satir = kod[a]
        if satir.startswith(('j','ret','ud2')):
            if i+1 < len(adresler):
                baslar.add(adresler[i+1])
            m = re.match(r'j\w*\s+0x([0-9a-f]+)',satir)
            if m and int(m[1],16) in kod:
                hedefler[a] = int(m[1],16)
                baslar.add(hedefler[a])
    bloklar = {}
    kenarlar = {}
    for i,a in enumerate(adresler):
        if a in baslar:
            b = a
            bloklar[b] = []
            kenarlar[b] = []
        bloklar[b].append(kod[a])
        if a in hedefler:
            kenarlar[b].append(hedefler[a])
        if i+1 < len(adresler) and adresler[i+1] in baslar and not kod[a].startswith(('jmp','ret','ud2')):
            kenarlar[b].append(adresler[i+1])
    return {b:'\n'.join(xs) for b,xs in bloklar.items()}, kenarlar


def ir_kanit(bloklar, yol, bit, dongu=True):
    ad, *kalan = yol
    b = bloklar.get(ad, {})
    kenarlar = {k:re.findall(r'label %([\w.$-]+)',v) for k,v in b.items()}
    secim = dongu_dugumleri(kenarlar) if dongu else b.keys()
    kod = ''.join(b[k] for k in secim)
    if kalan and re.search(rf'\bcall [^\n]*@{re.escape(kalan[0])}\(',kod):
        return ir_kanit(bloklar,kalan,bit,False)
    return bool(re.search(rf'load volatile i{bit}\b',kod))


def asm_kanit(islevler, yol, bit, dongu=True):
    ad, *kalan = yol
    if ad not in islevler:
        return False
    bloklar, kenarlar = asm_kenarlar(islevler[ad])
    secim = dongu_dugumleri(kenarlar) if dongu else bloklar.keys()
    kod = '\n'.join(bloklar[k] for k in secim)
    ek = {8:'b',16:'w',32:'l',64:'q'}[bit]
    # AT&T kaynak operandındaki bellek okuması veya belleği sınayan cmp/test.
    normal = rf'^(?:mov{ek}|movz{ek}[lq]|movs{ek}[lq])\s+[^,\n]*\(%(?!r(?:sp|ip))'
    karsilastir = rf'^(?:cmp|test){ek}\s+[^\n]*\(%(?!r(?:sp|ip))'
    if kalan and re.search(rf'\bcallq?\s+[^\n]*<{re.escape(kalan[0])}>',kod):
        return asm_kanit(islevler,kalan,bit,False)
    return bool(re.search(normal+'|'+karsilastir,kod,re.M))


def denetle(w, liste):
    adlar = {isim for ad,yol,_ in liste for isim in (ad,*yol)}
    ham = ir_islevleri(w/'k.ll',adlar)
    opt = ir_islevleri(w/'o2.ll',adlar)
    asm = asm_islevleri(w/'k.elf',adlar)
    for ad, yol, bit in liste:
        for asama, deger in [('IR', ir_kanit(ham,(ad,*yol),bit)),
                              ('O2 IR', ir_kanit(opt,(ad,*yol),bit)),
                              ('objdump', asm_kanit(asm,(ad,*yol),bit))]:
            if not deger:
                raise SystemExit(f'MMIO RED {ad} {asama}: döngü içinde {bit} bit aygıt okuması bulunamadı')
        print(f'MMIO OK {ad} i{bit} IR/O2/objdump',flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    a = p.parse_args()
    denetle(a.dizin,BEKLEMELER)
