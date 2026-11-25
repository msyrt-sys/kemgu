"""Dalga 13 QEMU diskini bağımsız FAT32 yazıcısıyla hazırlar."""

import struct

from dalga12_imaj import DATA, ESP, gpt, mbr
from fat_yazici import hazirla
from uefi_imaj import hazirla_uefi

SEKTOR = 512
ZINCIR_SONU = 0x0FFFFFFF
GUNLUK = "Ağaç ve İş\n".encode()
BUYUK = "bir iki\nüç dört beş\n".encode() * 11000


def dosyali_fat(yol, dizinler):
    hazirla(yol)
    buf = bytearray(yol.read_bytes())
    ayr, = struct.unpack_from("<H", buf, 14)
    fs, = struct.unpack_from("<I", buf, 36)
    toplam, = struct.unpack_from("<I", buf, 32)
    ilk = ayr + 2 * fs
    gel, = struct.unpack_from("<I", buf, SEKTOR + 492)

    def konum(kume):
        return (ilk + kume - 2) * SEKTOR

    def zincir(data, adet=1):
        nonlocal gel
        bas = gel
        gerekli = max(adet, (len(data) + SEKTOR - 1) // SEKTOR)
        if gel + gerekli > toplam - ilk + 2:
            raise ValueError("FAT32 test imajı doldu")
        gel += gerekli
        for kume in range(bas, gel):
            sonraki = kume + 1 if kume + 1 < gel else ZINCIR_SONU
            for kopya in (ayr, ayr + fs):
                struct.pack_into("<I", buf, kopya * SEKTOR + kume * 4, sonraki)
        buf[konum(bas):konum(bas) + len(data)] = data
        return bas

    def kisa(ad, kume, boyut, tur):
        kayit = bytearray(32)
        kayit[:11] = ad
        kayit[11] = tur
        struct.pack_into("<H", kayit, 20, kume >> 16)
        struct.pack_into("<HHI", kayit, 24, (2026 - 1980) * 512 + 33, kume & 65535, boyut)
        return kayit

    def uzun(ad, kisa_ad, kume, boyut):
        kodlar = list(struct.iter_unpack("<H", ad.encode("utf-16le"))) + [(0,)]
        kodlar += [(65535,)] * ((-len(kodlar)) % 13)
        toplam = 0
        for b in kisa_ad:
            toplam = (((toplam & 1) << 7) | (toplam >> 1)) + b & 255
        veri = bytearray()
        for i in reversed(range(len(kodlar) // 13)):
            e = bytearray(32)
            e[0] = i + 1 | (64 if i == len(kodlar) // 13 - 1 else 0)
            e[11] = 15
            e[13] = toplam
            for ofset, (kod,) in zip((1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30), kodlar[i * 13:(i + 1) * 13]):
                struct.pack_into("<H", e, ofset, kod)
            veri += e
        return veri + kisa(kisa_ad, kume, boyut, 32)

    kok_son = konum(2)
    while buf[kok_son]:
        kok_son += 32
    baslangic = gel
    for dizin, dosyalar in dizinler.items():
        girdiler = bytearray()
        for i, (ad, data) in enumerate(dosyalar.items()):
            alias = f"F{i + 1:07d}"
            uzanti = ad.rsplit(".", 1)[-1].upper()[:3].ljust(3)
            girdiler += uzun(ad, (alias + uzanti).encode("ascii"), zincir(data), len(data))
        bas = gel
        tumu = (kisa(b".          ", bas, 0, 16)
                + kisa(b"..         ", 0, 0, 16) + girdiler + bytes(32))
        zincir(tumu)
        if kok_son + 32 >= konum(3):
            raise ValueError("Test kök dizini doldu")
        buf[kok_son:kok_son + 32] = kisa(dizin.upper().ljust(11).encode("ascii"), bas, 0, 16)
        kok_son += 32
    for info in (1, 7):
        bos, = struct.unpack_from("<I", buf, info * SEKTOR + 488)
        struct.pack_into("<II", buf, info * SEKTOR + 488, bos - (gel - baslangic), gel)
    yol.write_bytes(buf)


def usb_hazirla(w, programlar, *, ntp_otomatik=False):
    kaynak = w / "surec.fat"
    dosyali_fat(kaynak, {"bin": programlar, "notlar": {"günlük.txt": GUNLUK, "buyuk.txt": BUYUK}})
    boot = w / "surec-boot.img"
    hazirla_uefi(boot, w / "esp/EFI/BOOT/BOOTX64.EFI", w / "k.elf", ntp_otomatik=ntp_otomatik)
    return gpt(w / "kem_os_usb.img", [(boot, 2048, ESP), (kaynak, 0, DATA)])


def imajlar(w, programlar, *, ntp_otomatik=False):
    usb = usb_hazirla(w, programlar, ntp_otomatik=ntp_otomatik)
    kaynak = w / "surec.fat"
    nvme, = gpt(w / "surec-nvme.img", [(kaynak, 0, DATA)])
    sata = mbr(w / "surec-sata.img", kaynak, 0, 0x98F0D0C2)
    return usb, nvme, sata
